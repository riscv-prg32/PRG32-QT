#include "MdnsAdvertiser.h"
#include <QCoreApplication>
#include <QHostInfo>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QTimer>
#include <QUdpSocket>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

namespace {
constexpr quint16 MdnsPort = 5353;
constexpr quint32 RecordTtlSeconds = 120;
const QHostAddress MdnsGroup(QStringLiteral("224.0.0.251"));

void append16(QByteArray& data, quint16 value) {
    data.append(char(value >> 8));
    data.append(char(value));
}

void append32(QByteArray& data, quint32 value) {
    append16(data, quint16(value >> 16));
    append16(data, quint16(value));
}

void appendName(QByteArray& data, const QByteArray& name) {
    for (const QByteArray& label : name.split('.')) {
        if (label.isEmpty())
            continue;
        data.append(char(label.size()));
        data.append(label);
    }
    data.append(char(0));
}

bool read16(const QByteArray& data, qsizetype offset, quint16& value) {
    if (offset < 0 || offset + 2 > data.size())
        return false;
    value = quint16(quint8(data[offset])) << 8 | quint8(data[offset + 1]);
    return true;
}

bool readName(const QByteArray& packet, qsizetype& offset, QByteArray& name, int depth = 0) {
    if (depth > 16)
        return false;
    bool consumedPointer = false;
    qsizetype cursor = offset;
    while (cursor < packet.size()) {
        const quint8 length = quint8(packet[cursor++]);
        if (length == 0) {
            if (!consumedPointer)
                offset = cursor;
            return true;
        }
        if ((length & 0xc0u) == 0xc0u) {
            if (cursor >= packet.size())
                return false;
            const qsizetype pointer = qsizetype(length & 0x3fu) << 8 | quint8(packet[cursor++]);
            if (!consumedPointer)
                offset = cursor;
            consumedPointer = true;
            QByteArray suffix;
            qsizetype suffixOffset = pointer;
            if (!readName(packet, suffixOffset, suffix, depth + 1))
                return false;
            if (!name.isEmpty() && !suffix.isEmpty())
                name.append('.');
            name.append(suffix);
            return true;
        }
        if ((length & 0xc0u) != 0 || cursor + length > packet.size())
            return false;
        if (!name.isEmpty())
            name.append('.');
        name.append(packet.constData() + cursor, length);
        cursor += length;
    }
    return false;
}

QList<QHostAddress> localIpv4Addresses() {
    QList<QHostAddress> result;
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback())
                result.append(entry.ip());
        }
    }
    return result;
}
} // namespace

class MdnsAdvertiser::Backend final {
  public:
    explicit Backend(quint16 port) : port_(port) {
        if (!port)
            return;
#ifdef Q_OS_ANDROID
        acquireAndroidMulticastLock();
#endif
        QByteArray host = QHostInfo::localHostName().toUtf8().toLower();
        for (char& character : host) {
            const bool valid = (character >= 'a' && character <= 'z') ||
                               (character >= '0' && character <= '9') || character == '-';
            if (!valid)
                character = '-';
        }
        host = host.left(48);
        if (host.isEmpty())
            host = "host";
        hostName_ = "prg32-qt-" + host + ".local";
        name_ = QStringLiteral("PRG32-QT on %1").arg(QString::fromUtf8(host));
        instanceName_ = name_.toUtf8() + "." + serviceType();

        const auto bindMode = QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint;
        if (!socket_.bind(QHostAddress::AnyIPv4, MdnsPort, bindMode))
            return;
        bool joined = false;
        for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
            const auto flags = interface.flags();
            if (flags.testFlag(QNetworkInterface::IsUp) && flags.testFlag(QNetworkInterface::IsRunning) &&
                flags.testFlag(QNetworkInterface::CanMulticast) &&
                !flags.testFlag(QNetworkInterface::IsLoopBack))
                joined = socket_.joinMulticastGroup(MdnsGroup, interface) || joined;
        }
        if (!joined)
            joined = socket_.joinMulticastGroup(MdnsGroup);
        if (!joined) {
            socket_.close();
            return;
        }
        QObject::connect(&socket_, &QUdpSocket::readyRead, [this] { receiveQueries(); });
        QObject::connect(&refresh_, &QTimer::timeout, [this] { announce(RecordTtlSeconds); });
        refresh_.start(60'000);
        active_ = true;
        QTimer::singleShot(0, &socket_, [this] { announce(RecordTtlSeconds); });
    }

    ~Backend() {
        if (active_)
            announce(0);
#ifdef Q_OS_ANDROID
        releaseAndroidMulticastLock();
#endif
    }

    [[nodiscard]] bool active() const {
        return active_;
    }

    [[nodiscard]] QString serviceName() const {
        return name_;
    }

  private:
#ifdef Q_OS_ANDROID
    void acquireAndroidMulticastLock() {
        const QJniObject context = QNativeInterface::QAndroidApplication::context();
        const QJniObject wifiServiceName = QJniObject::fromString(QStringLiteral("wifi"));
        const QJniObject wifiManager = context.callObjectMethod(
            "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;", wifiServiceName.object<jstring>());
        if (!wifiManager.isValid())
            return;
        const QJniObject tag = QJniObject::fromString(QStringLiteral("PRG32-QT mDNS"));
        multicastLock_ =
            wifiManager.callObjectMethod("createMulticastLock",
                                         "(Ljava/lang/String;)Landroid/net/wifi/WifiManager$MulticastLock;",
                                         tag.object<jstring>());
        if (!multicastLock_.isValid())
            return;
        multicastLock_.callMethod<void>("setReferenceCounted", "(Z)V", jboolean(false));
        multicastLock_.callMethod<void>("acquire");
    }

    void releaseAndroidMulticastLock() {
        if (multicastLock_.isValid() && multicastLock_.callMethod<jboolean>("isHeld"))
            multicastLock_.callMethod<void>("release");
    }
#endif

    static QByteArray serviceType() {
        return QByteArray(MdnsAdvertiser::ServiceType) + ".local";
    }

    void appendRecord(QByteArray& packet,
                      const QByteArray& owner,
                      quint16 type,
                      bool cacheFlush,
                      quint32 ttl,
                      const QByteArray& value) const {
        appendName(packet, owner);
        append16(packet, type);
        append16(packet, cacheFlush ? 0x8001u : 1u);
        append32(packet, ttl);
        append16(packet, quint16(value.size()));
        packet.append(value);
    }

    QByteArray response(quint32 ttl) const {
        const QList<QHostAddress> addresses = localIpv4Addresses();
        QByteArray packet;
        append16(packet, 0);
        append16(packet, 0x8400);
        append16(packet, 0);
        append16(packet, quint16(3 + addresses.size()));
        append16(packet, 0);
        append16(packet, 0);

        QByteArray pointer;
        appendName(pointer, instanceName_);
        appendRecord(packet, serviceType(), 12, false, ttl, pointer);

        QByteArray srv;
        append16(srv, 0);
        append16(srv, 0);
        append16(srv, port_);
        appendName(srv, hostName_);
        appendRecord(packet, instanceName_, 33, true, ttl, srv);

        QByteArray txt;
        for (const QByteArray& entry :
             {QByteArray("api=prg32-http-1"),
              QByteArray("path=/api"),
              QByteArray("runtime=qt"),
              QByteArray("version=") + QCoreApplication::applicationVersion().toUtf8()}) {
            txt.append(char(entry.size()));
            txt.append(entry);
        }
        appendRecord(packet, instanceName_, 16, true, ttl, txt);

        for (const QHostAddress& address : addresses) {
            const quint32 ipv4 = address.toIPv4Address();
            QByteArray bytes;
            append32(bytes, ipv4);
            appendRecord(packet, hostName_, 1, true, ttl, bytes);
        }
        return packet;
    }

    void announce(quint32 ttl) {
        const QByteArray packet = response(ttl);
        bool sent = false;
        for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
            const auto flags = interface.flags();
            if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning) ||
                !flags.testFlag(QNetworkInterface::CanMulticast) ||
                flags.testFlag(QNetworkInterface::IsLoopBack))
                continue;
            socket_.setMulticastInterface(interface);
            sent = socket_.writeDatagram(packet, MdnsGroup, MdnsPort) >= 0 || sent;
        }
        if (!sent) {
            socket_.setMulticastInterface({});
            socket_.writeDatagram(packet, MdnsGroup, MdnsPort);
        }
    }

    void receiveQueries() {
        while (socket_.hasPendingDatagrams()) {
            const QByteArray packet = socket_.receiveDatagram().data();
            quint16 questionCount = 0;
            if (packet.size() < 12 || !read16(packet, 4, questionCount))
                continue;
            qsizetype offset = 12;
            bool relevant = false;
            for (quint16 question = 0; question < questionCount; ++question) {
                QByteArray name;
                quint16 type = 0;
                if (!readName(packet, offset, name) || !read16(packet, offset, type) ||
                    offset + 4 > packet.size()) {
                    relevant = false;
                    break;
                }
                offset += 4;
                const bool requestedType = type == 1 || type == 12 || type == 16 || type == 33 || type == 255;
                relevant =
                    relevant || (requestedType && (name.compare(serviceType(), Qt::CaseInsensitive) == 0 ||
                                                   name.compare(instanceName_, Qt::CaseInsensitive) == 0 ||
                                                   name.compare(hostName_, Qt::CaseInsensitive) == 0));
            }
            if (relevant)
                announce(RecordTtlSeconds);
        }
    }

    QUdpSocket socket_;
    QTimer refresh_;
    quint16 port_ = 0;
    QByteArray hostName_;
    QByteArray instanceName_;
    QString name_;
    bool active_ = false;
#ifdef Q_OS_ANDROID
    QJniObject multicastLock_;
#endif
};

MdnsAdvertiser::MdnsAdvertiser(quint16 port, QObject* parent)
    : QObject(parent), backend_(std::make_unique<Backend>(port)) {
}

MdnsAdvertiser::~MdnsAdvertiser() = default;

bool MdnsAdvertiser::active() const {
    return backend_->active();
}

QString MdnsAdvertiser::serviceName() const {
    return backend_->serviceName();
}

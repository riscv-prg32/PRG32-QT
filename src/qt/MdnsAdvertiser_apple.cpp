#include "MdnsAdvertiser.h"
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QSysInfo>
#include <dns_sd.h>

class MdnsAdvertiser::Backend final {
  public:
    explicit Backend(quint16 port) {
        if (!port)
            return;
        QString hostName = QSysInfo::machineHostName();
        if (hostName.endsWith(QStringLiteral(".local"), Qt::CaseInsensitive))
            hostName.chop(6);
        name_ = QStringLiteral("PRG32-QT on %1").arg(hostName);
        TXTRecordRef txt;
        TXTRecordCreate(&txt, 0, nullptr);
        addText(txt, "api", "prg32-http-1");
        addText(txt, "path", "/api");
        addText(txt, "runtime", "qt");
        addText(txt, "version", QCoreApplication::applicationVersion().toUtf8());
        QByteArray name = name_.toUtf8();
        const DNSServiceErrorType error = DNSServiceRegister(&service_,
                                                             0,
                                                             0,
                                                             name.constData(),
                                                             MdnsAdvertiser::ServiceType,
                                                             nullptr,
                                                             nullptr,
                                                             htons(port),
                                                             TXTRecordGetLength(&txt),
                                                             TXTRecordGetBytesPtr(&txt),
                                                             &Backend::registered,
                                                             this);
        TXTRecordDeallocate(&txt);
        if (error != kDNSServiceErr_NoError) {
            service_ = nullptr;
            return;
        }
        notifier_ = std::make_unique<QSocketNotifier>(DNSServiceRefSockFD(service_), QSocketNotifier::Read);
        QObject::connect(notifier_.get(), &QSocketNotifier::activated, [this] {
            if (DNSServiceProcessResult(service_) != kDNSServiceErr_NoError)
                stop();
        });
    }

    ~Backend() {
        stop();
    }

    [[nodiscard]] bool active() const {
        return service_ != nullptr && registered_;
    }

    [[nodiscard]] QString serviceName() const {
        return name_;
    }

  private:
    static void addText(TXTRecordRef& record, const char* key, const QByteArray& value) {
        TXTRecordSetValue(&record, key, uint8_t(value.size()), value.constData());
    }

    static void registered(DNSServiceRef,
                           DNSServiceFlags,
                           DNSServiceErrorType error,
                           const char* name,
                           const char*,
                           const char*,
                           void* context) {
        auto* backend = static_cast<Backend*>(context);
        if (error != kDNSServiceErr_NoError) {
            backend->stop();
            return;
        }
        backend->registered_ = true;
        backend->name_ = QString::fromUtf8(name);
    }

    void stop() {
        notifier_.reset();
        if (service_) {
            DNSServiceRefDeallocate(service_);
            service_ = nullptr;
        }
        registered_ = false;
    }

    DNSServiceRef service_ = nullptr;
    std::unique_ptr<QSocketNotifier> notifier_;
    QString name_;
    bool registered_ = false;
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

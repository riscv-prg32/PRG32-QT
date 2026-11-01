#include "IoProtocolServer.h"
#include "AppController.h"
#include "InputButtons.h"
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>
#include <cstring>

namespace {
constexpr char Magic[] = {'P', 'R', 'G', '3', '2', 'I', 'O', '\0'};
constexpr int HeaderSize = 24;
constexpr quint8 Version = 1;
constexpr int MaximumPayload = 16 * 1024;
constexpr int MaximumChunk = 4 * 1024;
/** One firmware cartridge slot and execution window. */
constexpr int MaximumCartridgeBytes = 128 * 1024;
constexpr int DefaultKeyHoldMs = 200;
constexpr int MaximumKeyHoldMs = 60000;
enum : quint8 {
    Hello = 1,
    UploadBegin,
    UploadData,
    UploadEnd,
    Run,
    Debug,
    PerformanceGet,
    WifiScan,
    WifiSet,
    WifiForget,
    Key,
    Status = 0x80,
    ResultData = 0x81
};
quint32 crc32(const QByteArray& bytes) {
    quint32 crc = 0xffffffffu;
    for (const auto value : bytes) {
        crc ^= quint8(value);
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & quint32(-qint32(crc & 1u)));
    }
    return ~crc;
}
QByteArray compact(const QJsonObject& value) {
    return QJsonDocument(value).toJson(QJsonDocument::Compact);
}
/** The firmware's REMOTE_KEYS table, in the same order as HELLO `keys` on the board. */
struct KeyName {
    const char* name;
    quint32 bit;
};
constexpr KeyName KeyNames[] = {{"LEFT", prg32qt::input::Left},
                                {"RIGHT", prg32qt::input::Right},
                                {"UP", prg32qt::input::Up},
                                {"DOWN", prg32qt::input::Down},
                                {"A", prg32qt::input::A},
                                {"B", prg32qt::input::B},
                                {"START", prg32qt::input::Select},
                                {"C", prg32qt::input::C},
                                {"D", prg32qt::input::D},
                                {"L", prg32qt::input::L},
                                {"R", prg32qt::input::R}};
QJsonArray keyNames() {
    QJsonArray names;
    for (const KeyName& entry : KeyNames)
        names.append(entry.name);
    return names;
}
/** Bit of a key name, with SELECT as a second name for START; 0 when the name is unknown. */
quint32 keyBit(const QString& name) {
    if (name == "SELECT")
        return prg32qt::input::Select;
    for (const KeyName& entry : KeyNames) {
        if (name == QLatin1String(entry.name))
            return entry.bit;
    }
    return 0;
}
/** The runtime contract as a protocol host needs it: uploads here have the firmware's slot size. */
QJsonObject helloRuntime(QJsonObject runtime) {
    runtime["cart_max_size"] = MaximumCartridgeBytes;
    return runtime;
}
} // namespace

IoProtocolServer::IoProtocolServer(AppController* controller, QObject* parent)
    : QObject(parent), controller_(controller) {
    connect(&server_, &QTcpServer::newConnection, this, [this] {
        while (auto* socket = server_.nextPendingConnection()) {
            auto* buffer = new QByteArray;
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                buffer->append(socket->readAll());
                process(socket, *buffer);
            });
            connect(socket, &QObject::destroyed, this, [buffer] { delete buffer; });
        }
    });
    server_.listen(QHostAddress::LocalHost, 8081);
}

void IoProtocolServer::process(QTcpSocket* socket, QByteArray& buffer) {
    const QByteArray magic(Magic, int(sizeof(Magic)));
    while (true) {
        const int start = buffer.indexOf(magic);
        if (start < 0) {
            buffer = buffer.right(qMin(buffer.size(), magic.size() - 1));
            return;
        }
        if (start)
            buffer.remove(0, start);
        if (buffer.size() < HeaderSize)
            return;
        const auto* header = reinterpret_cast<const uchar*>(buffer.constData());
        const quint8 version = header[8], type = header[9];
        const quint32 requestId = qFromLittleEndian<quint32>(header + 12);
        const quint32 size = qFromLittleEndian<quint32>(header + 16);
        const quint32 checksum = qFromLittleEndian<quint32>(header + 20);
        if (version != Version || size > quint32(MaximumPayload)) {
            buffer.remove(0, 1);
            continue;
        }
        if (buffer.size() < HeaderSize + int(size))
            return;
        const QByteArray payload = buffer.mid(HeaderSize, int(size));
        buffer.remove(0, HeaderSize + int(size));
        if (crc32(payload) != checksum) {
            status(socket, requestId, false, QStringLiteral("payload checksum mismatch"));
            continue;
        }
        handle(socket, type, requestId, payload);
    }
}

void IoProtocolServer::handle(QTcpSocket* socket, quint8 type, quint32 requestId, const QByteArray& payload) {
    if (type == Hello) {
        send(socket,
             Status,
             1,
             requestId,
             compact({{"ok", true},
                      {"protocol", "prg32-io-1.0"},
                      {"capabilities", QJsonArray{"upload", "run", "debug", "performance", "keys"}},
                      {"keys", keyNames()},
                      {"debug_supported", true},
                      {"firmware", QStringLiteral("qt-" PRG32QT_VERSION)},
                      {"target", "qt"},
                      {"slots", AppController::CartridgeSlotCount},
                      {"max_chunk", MaximumChunk},
                      {"max_payload", MaximumPayload},
                      {"runtime", helloRuntime(controller_->runtimeJson())}}));
        return;
    }
    if (type == UploadBegin) {
        const auto metadata = QJsonDocument::fromJson(payload).object();
        const QString slot = metadata.value("slot").toString();
        bool slotOk = false;
        const int slotNumber = slot.startsWith("cart") ? slot.mid(4).toInt(&slotOk) : -1;
        const int size = metadata.value("size").toInt(-1);
        if (!slotOk || slotNumber < 0 || slotNumber >= AppController::CartridgeSlotCount || size <= 0 ||
            size > MaximumCartridgeBytes) {
            status(socket, requestId, false, QStringLiteral("invalid upload metadata"));
            return;
        }
        upload_.clear();
        upload_.reserve(size);
        uploadName_ = metadata.value("name").toString(QStringLiteral("cartridge"));
        uploadCommand_ = metadata.value("command").toString(QStringLiteral("store"));
        uploadSlot_ = slotNumber;
        uploadSize_ = quint32(size);
        uploadCrc_ = quint32(metadata.value("crc32").toDouble());
        lastChunk_.clear();
        lastChunkOffset_ = 0;
        controller_->releaseRemoteKeys(controller_->remoteKeys());
        controller_->stop();
        controller_->setUploadState(true, uploadName_, slot, 0, QStringLiteral("UPLOADING..."));
        status(socket, requestId, true, QStringLiteral("upload accepted"));
        return;
    }
    if (type == UploadData) {
        if (uploadSlot_ < 0 || payload.size() < 4) {
            status(socket, requestId, false, QStringLiteral("no upload is active"));
            return;
        }
        const quint32 offset =
            qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(payload.constData()));
        const QByteArray data = payload.mid(4);
        // A chunk repeating the previous one is a resend after a lost reply: acknowledge it, write nothing.
        const bool repeated = !lastChunk_.isEmpty() && offset == lastChunkOffset_ && data == lastChunk_;
        if (!repeated) {
            if (offset != quint32(upload_.size()) || upload_.size() + data.size() > int(uploadSize_)) {
                status(socket, requestId, false, QStringLiteral("out-of-order upload chunk"));
                return;
            }
            upload_.append(data);
            lastChunk_ = data;
            lastChunkOffset_ = offset;
        }
        controller_->setUploadState(true,
                                    uploadName_,
                                    QString("cart%1").arg(uploadSlot_),
                                    int((quint64(upload_.size()) * 100u) / uploadSize_),
                                    QStringLiteral("UPLOADING..."));
        send(socket,
             Status,
             1,
             requestId,
             compact({{"ok", true}, {"received", upload_.size()}, {"total", double(uploadSize_)}}));
        return;
    }
    if (type == UploadEnd) {
        QString error;
        bool ok = uploadSlot_ >= 0 && quint32(upload_.size()) == uploadSize_ && crc32(upload_) == uploadCrc_;
        if (!ok)
            error = QStringLiteral("upload length or checksum mismatch");
        if (ok)
            ok = controller_->uploadSlot(uploadSlot_, upload_, error);
        if (ok && (uploadCommand_ == "run" || uploadCommand_ == "debug" || uploadCommand_ == "performance"))
            ok = controller_->selectSlot(uploadSlot_, error);
        if (ok && uploadCommand_ == "debug")
            controller_->setDebugEnabled(true);
        if (ok && uploadCommand_ != "store")
            emit controller_->playerRequested();
        controller_->setUploadState(true,
                                    uploadName_,
                                    QString("cart%1").arg(uploadSlot_),
                                    100,
                                    ok ? QStringLiteral("UPLOAD SUCCESSFUL")
                                       : QStringLiteral("UPLOAD FAILED: %1").arg(error));
        if (ok) {
            send(socket,
                 Status,
                 1,
                 requestId,
                 compact({{"ok", true},
                          {"message", "upload complete"},
                          {"name", uploadName_},
                          {"slot", QString("cart%1").arg(uploadSlot_)},
                          {"size", double(uploadSize_)},
                          {"running", uploadCommand_ != "store"}}));
        } else {
            status(socket, requestId, false, error);
        }
        uploadSlot_ = -1;
        QTimer::singleShot(1800, controller_, [controller = controller_] {
            controller->setUploadState(false, {}, {}, 0, {});
        });
        return;
    }
    if (type == Run || type == Debug) {
        const QString slot = QJsonDocument::fromJson(payload).object().value("slot").toString("cart0");
        bool slotOk = false;
        const int slotNumber = slot.startsWith("cart") ? slot.mid(4).toInt(&slotOk) : -1;
        QString error;
        controller_->releaseRemoteKeys(controller_->remoteKeys());
        const bool ok = slotOk && controller_->selectSlot(slotNumber, error);
        if (ok && type == Debug)
            controller_->setDebugEnabled(true);
        if (ok)
            emit controller_->playerRequested();
        status(socket, requestId, ok, ok ? QStringLiteral("cartridge running") : error);
        return;
    }
    if (type == PerformanceGet) {
        send(socket, ResultData, 1, requestId, compact(controller_->performanceJson()));
        return;
    }
    if (type == Key) {
        handleKey(socket, requestId, payload);
        return;
    }
    if (type == WifiScan || type == WifiSet || type == WifiForget) {
        status(socket, requestId, false, QStringLiteral("Wi-Fi is not available in this build"));
        return;
    }
    status(socket, requestId, false, QStringLiteral("unsupported protocol message"));
}

void IoProtocolServer::handleKey(QTcpSocket* socket, quint32 requestId, const QByteArray& payload) {
    if (uploadSlot_ >= 0) {
        status(socket, requestId, false, QStringLiteral("upload in progress"));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        status(socket, requestId, false, QStringLiteral("invalid key request"));
        return;
    }
    const QJsonObject request = document.object();
    const QString action = request.value("action").toString(QStringLiteral("press"));
    if (action != "press" && action != "release") {
        status(socket, requestId, false, QStringLiteral("invalid key action"));
        return;
    }
    const QJsonValue keysValue = request.value("keys");
    if (!keysValue.isUndefined() && !keysValue.isArray()) {
        status(socket, requestId, false, QStringLiteral("keys must be an array"));
        return;
    }
    quint32 mask = 0;
    const QJsonArray keys = keysValue.toArray();
    for (const QJsonValue& key : keys) {
        const quint32 bit = keyBit(key.toString());
        if (bit == 0) {
            // One unknown name refuses the whole request, with no key changed.
            status(socket, requestId, false, QStringLiteral("unknown key %1").arg(key.toString()));
            return;
        }
        mask |= bit;
    }
    if (action == "release") {
        controller_->releaseRemoteKeys(keys.isEmpty() ? controller_->remoteKeys() : mask);
    } else {
        if (mask == 0) {
            status(socket, requestId, false, QStringLiteral("missing keys"));
            return;
        }
        const int holdMs =
            request.contains("hold_ms") ? request.value("hold_ms").toInt(-1) : DefaultKeyHoldMs;
        if (holdMs < 1 || holdMs > MaximumKeyHoldMs) {
            status(socket, requestId, false, QStringLiteral("invalid hold_ms"));
            return;
        }
        controller_->pressRemoteKeys(mask, holdMs);
    }
    QJsonArray active;
    const quint32 down = controller_->remoteKeys();
    for (const KeyName& entry : KeyNames) {
        if (down & entry.bit)
            active.append(entry.name);
    }
    send(socket, Status, 1, requestId, compact({{"ok", true}, {"active", active}}));
}

void IoProtocolServer::send(
    QTcpSocket* socket, quint8 type, quint16 flags, quint32 requestId, const QByteArray& payload) {
    QByteArray frame(HeaderSize + payload.size(), Qt::Uninitialized);
    auto* header = reinterpret_cast<uchar*>(frame.data());
    std::memcpy(header, Magic, sizeof(Magic));
    header[8] = Version;
    header[9] = type;
    qToLittleEndian<quint16>(flags, header + 10);
    qToLittleEndian<quint32>(requestId, header + 12);
    qToLittleEndian<quint32>(quint32(payload.size()), header + 16);
    qToLittleEndian<quint32>(crc32(payload), header + 20);
    std::memcpy(frame.data() + HeaderSize, payload.constData(), size_t(payload.size()));
    socket->write(frame);
}
void IoProtocolServer::status(QTcpSocket* socket, quint32 requestId, bool ok, const QString& message) {
    QJsonObject response{{"ok", ok}};
    response.insert(ok ? "message" : "error", message);
    send(socket, Status, 1, requestId, compact(response));
}

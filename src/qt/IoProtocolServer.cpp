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
/** PROTOCOL_UPLOAD_IDLE_MS in prg32_usb_protocol.c: an upload silent this long is cancelled. */
constexpr int UploadIdleMs = 10000;
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
/** slot_from_name() in prg32_usb_protocol.c: exactly "cart0" to "cart7", otherwise -1. */
int slotFromName(const QJsonValue& value) {
    const QString name = value.toString();
    if (!value.isString() || name.size() != 5 || !name.startsWith(QLatin1String("cart")))
        return -1;
    const int slot = name.at(4).unicode() - '0';
    return slot >= 0 && slot < AppController::CartridgeSlotCount ? slot : -1;
}
/** command_is_known() in prg32_usb_protocol.c. */
bool commandIsKnown(const QString& command) {
    return command == QLatin1String("store") || command == QLatin1String("run") ||
           command == QLatin1String("debug") || command == QLatin1String("performance");
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
            // A host that goes away mid-upload must not leave KEY and RUN refused for good.
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                if (uploadSlot_ >= 0 && uploadSocket_ == socket)
                    abandonUpload(QStringLiteral("UPLOAD FAILED: host disconnected"));
                socket->deleteLater();
            });
        }
    });
    uploadIdle_.setSingleShot(true);
    uploadIdle_.setInterval(UploadIdleMs);
    connect(&uploadIdle_, &QTimer::timeout, this, [this] {
        if (uploadSlot_ >= 0)
            abandonUpload(QStringLiteral("UPLOAD TIMED OUT"));
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
        if (uploadSlot_ >= 0)
            uploadIdle_.start();
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
        // handle_upload_begin(): name, size, crc32 and a cartN slot are required; command defaults to store.
        const auto metadata = QJsonDocument::fromJson(payload).object();
        const QJsonValue command = metadata.value("command");
        const int slotNumber = slotFromName(metadata.value("slot"));
        const QString slot = metadata.value("slot").toString();
        const double size = metadata.value("size").toDouble(-1);
        const double crc = metadata.value("crc32").toDouble(-1);
        const bool commandOk =
            command.isUndefined() || (command.isString() && commandIsKnown(command.toString()));
        if (!metadata.value("name").isString() || slotNumber < 0 || size <= 0 ||
            size > MaximumCartridgeBytes || size != double(qint64(size)) || crc < 0 || crc > 4294967295.0 ||
            crc != double(qint64(crc)) || !commandOk) {
            status(socket, requestId, false, QStringLiteral("invalid upload metadata"));
            return;
        }
        upload_.clear();
        upload_.reserve(int(size));
        uploadName_ = metadata.value("name").toString().left(32);
        uploadCommand_ = command.toString(QStringLiteral("store"));
        uploadSlot_ = slotNumber;
        uploadSize_ = quint32(size);
        uploadCrc_ = quint32(crc);
        uploadSocket_ = socket;
        uploadIdle_.start();
        controller_->releaseRemoteKeys(controller_->remoteKeys());
        controller_->stop();
        controller_->setUploadState(true, uploadName_, slot, 0, QStringLiteral("UPLOADING..."));
        status(socket, requestId, true, QStringLiteral("upload accepted"));
        return;
    }
    if (type == UploadData) {
        // handle_upload_data(): a 4-byte offset and 1 to 4096 bytes of data.
        if (uploadSlot_ < 0) {
            status(socket, requestId, false, QStringLiteral("no upload in progress"));
            return;
        }
        if (payload.size() <= 4 || payload.size() - 4 > MaximumChunk) {
            status(socket, requestId, false, QStringLiteral("invalid upload chunk"));
            return;
        }
        const quint32 offset =
            qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(payload.constData()));
        const QByteArray data = payload.mid(4);
        // A chunk that ends where the received data ends is a resend after a lost reply: acknowledge it and
        // write nothing, as the firmware does (the final CRC still covers what was kept).
        const bool repeated = quint64(offset) + quint64(data.size()) == quint64(upload_.size());
        if (!repeated) {
            if (offset != quint32(upload_.size()) || upload_.size() + data.size() > int(uploadSize_)) {
                status(socket, requestId, false, QStringLiteral("out-of-order upload chunk"));
                return;
            }
            upload_.append(data);
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
        if (uploadSlot_ < 0) {
            status(socket, requestId, false, QStringLiteral("no upload in progress"));
            return;
        }
        uploadIdle_.stop();
        uploadSocket_.clear();
        QString error;
        bool ok = quint32(upload_.size()) == uploadSize_ && crc32(upload_) == uploadCrc_;
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
        // handle_run(): refused during an upload, and the slot must be named exactly.
        if (uploadSlot_ >= 0) {
            status(socket, requestId, false, QStringLiteral("upload in progress"));
            return;
        }
        const int slotNumber = slotFromName(QJsonDocument::fromJson(payload).object().value("slot"));
        QString error;
        controller_->releaseRemoteKeys(controller_->remoteKeys());
        if (slotNumber < 0) {
            status(socket, requestId, false, QStringLiteral("invalid cartridge slot"));
            return;
        }
        const bool ok = controller_->selectSlot(slotNumber, error);
        if (ok && type == Debug)
            controller_->setDebugEnabled(true);
        if (ok)
            emit controller_->playerRequested();
        status(socket, requestId, ok, ok ? QStringLiteral("cartridge selected") : error);
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

void IoProtocolServer::abandonUpload(const QString& reason) {
    uploadIdle_.stop();
    uploadSocket_.clear();
    upload_.clear();
    const QString slot = QString("cart%1").arg(uploadSlot_);
    uploadSlot_ = -1;
    controller_->setUploadState(true, uploadName_, slot, 0, reason);
    QTimer::singleShot(
        1800, controller_, [controller = controller_] { controller->setUploadState(false, {}, {}, 0, {}); });
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

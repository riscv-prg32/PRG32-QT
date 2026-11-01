#include "QtMultiplayerService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>

namespace {
constexpr uint32_t SendPeriodMs = 50;
constexpr uint32_t PeerTimeoutMs = 3000;
constexpr int MaximumPeers = 8;
constexpr uint32_t ReconnectInitialDelayMs = 1000;
constexpr uint32_t ReconnectMaximumDelayMs = 30000;
constexpr int RoomFullRetryMs = 5000;
const char* const ProtocolName = "prg32-multiplayer-1.0";
const char* const PlatformName = "prg32-qt";
const char* const InstallationKey = "multiplayer/installationId";
/** Mask of the protocol's `input` field, 0 to 127: buttons LEFT through START. */
constexpr uint32_t WireInputMask = 0x7fu;

/** Read an exact integer in [minimum, maximum]; the protocol rejects strings, fractions and out-of-range
 * values. */
bool exactInteger(
    const QJsonObject& object, const char* name, double minimum, double maximum, double& output) {
    const QJsonValue value = object.value(QLatin1String(name));
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (number < minimum || number > maximum || number != double(qint64(number)))
        return false;
    output = number;
    return true;
}
} // namespace

QtMultiplayerService::QtMultiplayerService(QObject* parent) : QObject(parent) {
    reconnectTimer_.setSingleShot(true);
    joinRetryTimer_.setInterval(RoomFullRetryMs);
    connect(&reconnectTimer_, &QTimer::timeout, this, [this] {
        if (memberRequested_ && socket_.state() == QAbstractSocket::UnconnectedState)
            socket_.open(endpoint_);
    });
    connect(&joinRetryTimer_, &QTimer::timeout, this, &QtMultiplayerService::sendJoin);
    connect(&socket_, &QWebSocket::connected, this, &QtMultiplayerService::sendJoin);
    connect(&socket_, &QWebSocket::textMessageReceived, this, &QtMultiplayerService::handleMessage);
    connect(&socket_, &QWebSocket::disconnected, this, &QtMultiplayerService::handleDisconnected);
    serverUrl_ = QSettings().value("multiplayer/serverUrl").toString();
    applyEndpoint();
}

void QtMultiplayerService::setStoreUrl(const QUrl& storeUrl) {
    QUrl endpoint = storeUrl;
    endpoint.setScheme(storeUrl.scheme() == "https" ? "wss" : "ws");
    QString path = endpoint.path();
    if (!path.endsWith('/'))
        path += '/';
    endpoint.setPath(path + "api/multiplayer");
    endpoint.setQuery(QString());
    endpoint.setFragment(QString());
    storeEndpoint_ = endpoint;
    applyEndpoint();
}

void QtMultiplayerService::setServerUrl(const QString& serverUrl) {
    serverUrl_ = serverUrl;
    applyEndpoint();
}

void QtMultiplayerService::applyEndpoint() {
    const QUrl endpoint = serverUrl_.isEmpty() ? storeEndpoint_ : QUrl(serverUrl_);
    if (endpoint_ == endpoint)
        return;
    leave();
    endpoint_ = endpoint;
}

void QtMultiplayerService::initialize() {
    if (initialized_)
        return;
    initialized_ = true;
    clock_.start();
    preferredPlayerId_ = QRandomGenerator::global()->generate();
    if (preferredPlayerId_ == 0)
        preferredPlayerId_ = 1;
}

bool QtMultiplayerService::available() const {
    return endpoint_.isValid() && (endpoint_.scheme() == "ws" || endpoint_.scheme() == "wss");
}

bool QtMultiplayerService::validSignature(const QString& signature) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_.:-]{1,47}$"));
    return pattern.match(signature).hasMatch();
}

QString QtMultiplayerService::installationId() {
    QSettings settings;
    QString identifier = settings.value(InstallationKey).toString();
    if (identifier.isEmpty()) {
        identifier = QUuid::createUuid().toString(QUuid::WithoutBraces);
        settings.setValue(InstallationKey, identifier);
    }
    return identifier;
}

int QtMultiplayerService::join(const std::string& signature, uint32_t flags) {
    initialize();
    QString room = QString::fromStdString(signature);
    if (!available() || !validSignature(room))
        return -1;
    if (memberRequested_)
        leave();
    memberRequested_ = true;
    policyStopped_ = false;
    signature_ = room;
    flags_ = flags;
    reconnectDelayMs_ = ReconnectInitialDelayMs;
    peers_.clear();
    socket_.open(endpoint_);
    return 0;
}

int QtMultiplayerService::leave() {
    memberRequested_ = false;
    reconnectTimer_.stop();
    joinRetryTimer_.stop();
    if (admittedPlayerId_ != 0)
        sendText({{"type", "leave"}});
    admittedPlayerId_ = 0;
    signature_.clear();
    peers_.clear();
    // Leaving also closes the socket, as the firmware does: nothing else would use it.
    if (socket_.state() != QAbstractSocket::UnconnectedState)
        socket_.close();
    return 0;
}

void QtMultiplayerService::sendText(const QJsonObject& message) {
    if (socket_.state() == QAbstractSocket::ConnectedState)
        socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void QtMultiplayerService::sendJoin() {
    if (!memberRequested_)
        return;
    sendText({{"type", "join"},
              {"signature", signature_},
              {"flags", double(flags_)},
              {"player_id", double(preferredPlayerId_)},
              {"protocol", ProtocolName},
              {"platform", PlatformName},
              {"device_id", installationId()}});
}

void QtMultiplayerService::sendState() {
    // No player_id: the server takes the sender from the connection.
    sendText({{"type", "state"},
              {"x", local_.x},
              {"y", local_.y},
              {"sprite", local_.sprite},
              {"flags", local_.flags},
              {"input", double(local_.input & WireInputMask)},
              {"frame", double(local_.frame)}});
}

void QtMultiplayerService::tick(uint32_t nowMs) {
    prune(nowMs);
    // Snapshots wait for `welcome`: the latest one is sent on the first tick after admission.
    if (admittedPlayerId_ == 0)
        return;
    const uint32_t elapsedMs = uint32_t(nowMs - lastSendMs_);
    if (elapsedMs < SendPeriodMs)
        return;
    // Count from the time this send was due, as the firmware does: at 30 frames per second the gaps then
    // alternate between 33 and 66 ms and average the protocol's 50 ms.
    lastSendMs_ = elapsedMs < 2 * SendPeriodMs ? lastSendMs_ + SendPeriodMs : nowMs;
    sendState();
}

int QtMultiplayerService::setLocalState(int16_t x, int16_t y, uint16_t sprite, uint16_t flags) {
    if (!memberRequested_)
        return -1;
    local_.x = x;
    local_.y = y;
    local_.sprite = sprite;
    local_.flags = flags;
    ++local_.frame;
    return 0;
}

int QtMultiplayerService::setInput(uint32_t input) {
    if (!memberRequested_)
        return -1;
    local_.input = input;
    return 0;
}

uint32_t QtMultiplayerService::localPlayerId() const {
    return admittedPlayerId_;
}

void QtMultiplayerService::prune(uint32_t) {
    // Peers age on this service's real-time clock, not the runtime's: in Accurate mode the runtime's clock
    // is virtual, and network silence is measured in real time.
    if (!clock_.isValid())
        return;
    const uint32_t nowMs = uint32_t(clock_.elapsed());
    for (auto iterator = peers_.begin(); iterator != peers_.end();) {
        if (uint32_t(nowMs - iterator->lastSeenMs) > PeerTimeoutMs)
            iterator = peers_.erase(iterator);
        else
            ++iterator;
    }
}

int QtMultiplayerService::peerCount(uint32_t nowMs) {
    prune(nowMs);
    return int(peers_.size());
}

int QtMultiplayerService::peer(int index, uint32_t nowMs, prg32::MultiplayerPeer& output) {
    prune(nowMs);
    if (index < 0 || index >= peers_.size())
        return -1;
    auto iterator = peers_.cbegin();
    std::advance(iterator, index);
    output = iterator.value();
    // Cartridges compare last_seen_ms with prg32_ticks_ms(), so report it on the runtime's clock, which
    // differs from this service's real-time clock: virtual in Accurate mode, and started at another moment.
    output.lastSeenMs = nowMs - (uint32_t(clock_.elapsed()) - iterator->lastSeenMs);
    return 0;
}

void QtMultiplayerService::handleDisconnected() {
    // A new socket is a new session: forget the peers and the admission.
    if (admittedPlayerId_ != 0)
        preferredPlayerId_ = admittedPlayerId_;
    admittedPlayerId_ = 0;
    peers_.clear();
    joinRetryTimer_.stop();
    if (memberRequested_ && !policyStopped_)
        scheduleReconnect();
}

void QtMultiplayerService::scheduleReconnect() {
    // Backoff with jitter of up to half the delay, so a classroom does not reconnect all at once.
    const uint32_t jitter = QRandomGenerator::global()->bounded(reconnectDelayMs_ / 2 + 1);
    reconnectTimer_.start(int(reconnectDelayMs_ + jitter));
    reconnectDelayMs_ = qMin(reconnectDelayMs_ * 2, ReconnectMaximumDelayMs);
}

void QtMultiplayerService::handleError(const QString& code) {
    if (code == "room_full") {
        // The socket stays open, and join is repeated until a place frees up.
        if (!joinRetryTimer_.isActive())
            joinRetryTimer_.start();
    } else if (code == "rate_limited") {
        reconnectDelayMs_ = ReconnectMaximumDelayMs;
    } else if (code == "device_blocked") {
        policyStopped_ = true;
    }
}

void QtMultiplayerService::handleMessage(const QString& message) {
    const QJsonObject object = QJsonDocument::fromJson(message.toUtf8()).object();
    const QString type = object.value("type").toString();
    double number = 0;
    if (type == "error") {
        handleError(object.value("code").toString());
        return;
    }
    if (!exactInteger(object, "player_id", 1, 4294967295.0, number))
        return;
    const uint32_t id = uint32_t(number);
    if (type == "welcome") {
        admittedPlayerId_ = id;
        preferredPlayerId_ = id;
        local_.playerId = id;
        reconnectDelayMs_ = ReconnectInitialDelayMs;
        joinRetryTimer_.stop();
        return;
    }
    if (type == "leave") {
        peers_.remove(id);
        return;
    }
    if (type != "peer" || id == admittedPlayerId_)
        return;
    double x = 0;
    double y = 0;
    double sprite = 0;
    double flags = 0;
    double input = 0;
    double frame = 0;
    if (!exactInteger(object, "x", -32768, 32767, x) || !exactInteger(object, "y", -32768, 32767, y) ||
        !exactInteger(object, "sprite", 0, 65535, sprite) ||
        !exactInteger(object, "flags", 0, 65535, flags) || !exactInteger(object, "input", 0, 127, input) ||
        !exactInteger(object, "frame", 0, 4294967295.0, frame))
        return;
    const auto known = peers_.find(id);
    // Drop a snapshot older than the one held, with 32-bit wraparound.
    if (known != peers_.end() && int32_t(uint32_t(frame) - known->frame) < 0)
        return;
    if (known == peers_.end() && peers_.size() >= MaximumPeers)
        return;
    prg32::MultiplayerPeer peer;
    peer.playerId = id;
    peer.x = int16_t(x);
    peer.y = int16_t(y);
    peer.sprite = uint16_t(sprite);
    peer.flags = uint16_t(flags);
    peer.input = uint32_t(input);
    peer.frame = uint32_t(frame);
    peer.lastSeenMs = uint32_t(clock_.elapsed());
    peers_[id] = peer;
}

#include "QtMultiplayerService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
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
/** The firmware's discovery task repeats its UDP query every 2 s. */
constexpr int DiscoveryQueryPeriodMs = 2000;
/** PRG32_MP_PUBLIC_FALLBACK_MS: a search that finds nothing for 10 s gives way to the fallback. */
constexpr int PublicFallbackMs = 10000;
/** A known address first gets 3 s to answer before the local network is searched. */
constexpr int LinkDownSearchDelayMs = 3000;
/** Discovery datagrams are UTF-8 JSON objects of at most 512 bytes (server docs/protocol.md). */
constexpr qint64 MaximumDiscoveryDatagramBytes = 512;
/** PRG32_MP_URL_LEN is 128 bytes including the terminator. */
constexpr int MaximumServerUrlLength = 127;
/** Highest TCP port; prg32_mp_valid_port() accepts 1 to 65535. */
constexpr int MaximumPort = 65535;
/** prg32_mp_valid_port() accepts at most five digits. */
constexpr int MaximumPortDigits = 5;
const char* const ProtocolName = "prg32-multiplayer-1.0";
const char* const PlatformName = "prg32-qt";
const char* const InstallationKey = "multiplayer/installationId";
const char* const ServerUrlKey = "multiplayer/serverUrl";
const char* const LocalServerKey = "multiplayer/localServer";
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

/** The firmware's prg32_mp_valid_port(): 1 to 65535, digits only. */
bool validPort(const QString& port) {
    if (port.isEmpty() || port.size() > MaximumPortDigits)
        return false;
    for (const QChar character : port) {
        if (character < QLatin1Char('0') || character > QLatin1Char('9'))
            return false;
    }
    const int value = port.toInt();
    return value >= 1 && value <= MaximumPort;
}
} // namespace

QtMultiplayerService::QtMultiplayerService(QObject* parent) : QObject(parent) {
    reconnectTimer_.setSingleShot(true);
    joinRetryTimer_.setInterval(RoomFullRetryMs);
    discoveryTimer_.setInterval(DiscoveryQueryPeriodMs);
    fallbackTimer_.setSingleShot(true);
    fallbackTimer_.setInterval(PublicFallbackMs);
    linkDownTimer_.setSingleShot(true);
    linkDownTimer_.setInterval(LinkDownSearchDelayMs);
    connect(&reconnectTimer_, &QTimer::timeout, this, [this] {
        if (memberRequested_ && endpoint_.isValid() && socket_.state() == QAbstractSocket::UnconnectedState)
            socket_.open(endpoint_);
    });
    connect(&linkDownTimer_, &QTimer::timeout, this, [this] {
        if (memberRequested_ && localServerEnabled_ && !admitted())
            startDiscovery();
    });
    connect(&joinRetryTimer_, &QTimer::timeout, this, &QtMultiplayerService::sendJoin);
    connect(&discoveryTimer_, &QTimer::timeout, this, &QtMultiplayerService::sendDiscoveryQuery);
    connect(&fallbackTimer_, &QTimer::timeout, this, &QtMultiplayerService::handleFallback);
    connect(&discoverySocket_, &QUdpSocket::readyRead, this, &QtMultiplayerService::readDiscovery);
    connect(&socket_, &QWebSocket::connected, this, &QtMultiplayerService::handleConnected);
    connect(&socket_, &QWebSocket::textMessageReceived, this, &QtMultiplayerService::handleMessage);
    connect(&socket_, &QWebSocket::disconnected, this, &QtMultiplayerService::handleDisconnected);
    QSettings settings;
    serverUrl_ = normalizeServerUrl(settings.value(ServerUrlKey).toString());
    localServerEnabled_ = settings.value(LocalServerKey, true).toBool();
    setEndpoint(fallbackEndpoint(), false);
}

bool QtMultiplayerService::validServerUrl(const QString& url) {
    int schemeLength = 0;
    if (url.startsWith(QLatin1String("ws://")))
        schemeLength = int(qstrlen("ws://"));
    else if (url.startsWith(QLatin1String("wss://")))
        schemeLength = int(qstrlen("wss://"));
    if (schemeLength == 0 || url.size() <= schemeLength || url.size() > MaximumServerUrlLength)
        return false;
    // A host must follow the scheme, before any port or path.
    const QChar first = url.at(schemeLength);
    if (first == QLatin1Char(':') || first == QLatin1Char('/'))
        return false;
    // Printable ASCII only, without spaces: '!' (0x21) through '~' (0x7e).
    for (const QChar character : url) {
        if (character <= QLatin1Char(' ') || character > QLatin1Char('~'))
            return false;
    }
    return true;
}

QString QtMultiplayerService::normalizeServerUrl(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return {};
    QString url;
    if (trimmed.startsWith(QLatin1String("ws://")) || trimmed.startsWith(QLatin1String("wss://"))) {
        url = trimmed;
    } else if (trimmed.contains(QLatin1String("://"))) {
        // http://, https://, or another scheme.
        return {};
    } else if (trimmed.contains(QLatin1Char(':'))) {
        if (!validPort(trimmed.mid(trimmed.lastIndexOf(QLatin1Char(':')) + 1)))
            return {};
        url = QStringLiteral("ws://") + trimmed;
    } else {
        url = QStringLiteral("ws://%1:%2").arg(trimmed).arg(DefaultServerPort);
    }
    return validServerUrl(url) ? url : QString();
}

QString QtMultiplayerService::parseDiscovery(const QByteArray& datagram) {
    if (datagram.isEmpty() || datagram.size() > MaximumDiscoveryDatagramBytes)
        return {};
    const QJsonDocument document = QJsonDocument::fromJson(datagram);
    if (!document.isObject())
        return {};
    const QJsonObject object = document.object();
    const QJsonValue type = object.value(QLatin1String("type"));
    const QJsonValue protocol = object.value(QLatin1String("protocol"));
    const QJsonValue url = object.value(QLatin1String("url"));
    if (!type.isString() || !protocol.isString() || !url.isString())
        return {};
    if (type.toString() != QLatin1String("server") || protocol.toString() != QLatin1String(ProtocolName))
        return {};
    return validServerUrl(url.toString()) ? url.toString() : QString();
}

bool QtMultiplayerService::setServerUrl(const QString& serverUrl) {
    const QString normalized = normalizeServerUrl(serverUrl);
    if (!serverUrl.trimmed().isEmpty() && normalized.isEmpty())
        return false;
    if (normalized == serverUrl_)
        return true;
    serverUrl_ = normalized;
    // A server found on the local network is kept, as the firmware's forget_server_url() keeps it.
    if (memberRequested_ && !endpointIsLocal_)
        switchServer(QUrl(), false);
    else if (!memberRequested_ && !endpointIsLocal_)
        setEndpoint(fallbackEndpoint(), false);
    return true;
}

void QtMultiplayerService::setLocalServerEnabled(bool enabled) {
    if (localServerEnabled_ == enabled)
        return;
    localServerEnabled_ = enabled;
    QSettings().setValue(LocalServerKey, enabled);
    if (!enabled) {
        localEndpoint_.clear();
        stopDiscovery();
        fallbackTimer_.stop();
        linkDownTimer_.stop();
    }
    if (memberRequested_)
        switchServer(QUrl(), false);
    else
        setEndpoint(fallbackEndpoint(), false);
}

QUrl QtMultiplayerService::fallbackEndpoint() const {
    return QUrl(serverUrl_.isEmpty() ? QString::fromLatin1(PublicServerUrl) : serverUrl_);
}

void QtMultiplayerService::setEndpoint(const QUrl& endpoint, bool local) {
    if (endpoint_ == endpoint && endpointIsLocal_ == local)
        return;
    endpoint_ = endpoint;
    endpointIsLocal_ = local;
    emit currentServerChanged();
}

void QtMultiplayerService::searchLocalNetwork() {
    if (!localServerEnabled_ || memberRequested_)
        return;
    startDiscovery();
    fallbackTimer_.start();
}

void QtMultiplayerService::connectToServer() {
    if (localServerEnabled_ && localEndpoint_.isValid()) {
        setEndpoint(localEndpoint_, true);
    } else if (!serverUrl_.isEmpty() || !localServerEnabled_) {
        setEndpoint(fallbackEndpoint(), false);
    } else {
        // No address is known: the join stands while the local network is searched, and the public server
        // is used after 10 s of finding nothing, as the firmware's ensure_client() returning 1.
        setEndpoint(QUrl(), false);
        startDiscovery();
        fallbackTimer_.start();
        return;
    }
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        // A socket still closing after a leave: end it now, without scheduling a reconnection.
        switching_ = true;
        socket_.abort();
        switching_ = false;
    }
    socket_.open(endpoint_);
    if (endpointIsLocal_)
        fallbackTimer_.start();
    else if (localServerEnabled_)
        linkDownTimer_.start();
}

void QtMultiplayerService::switchServer(const QUrl& endpoint, bool local) {
    reconnectTimer_.stop();
    joinRetryTimer_.stop();
    linkDownTimer_.stop();
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        // The disconnection this causes is expected: handleDisconnected() must not schedule a reconnection.
        switching_ = true;
        socket_.abort();
        switching_ = false;
    }
    reconnectDelayMs_ = ReconnectInitialDelayMs;
    if (!memberRequested_) {
        setEndpoint(endpoint.isValid() ? endpoint : fallbackEndpoint(), local);
        return;
    }
    if (!endpoint.isValid()) {
        connectToServer();
        return;
    }
    setEndpoint(endpoint, local);
    socket_.open(endpoint_);
    if (local)
        fallbackTimer_.start();
}

void QtMultiplayerService::startDiscovery() {
    if (discovering_ || !localServerEnabled_)
        return;
    if (discoverySocket_.state() != QAbstractSocket::BoundState) {
        // Port 8082 also receives the server's beacons. When a server on this computer holds it without
        // sharing, an ephemeral port still receives answers to this console's own queries.
        const auto mode = QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint;
        if (!discoverySocket_.bind(QHostAddress::AnyIPv4, DiscoveryPort, mode) &&
            !discoverySocket_.bind(QHostAddress::AnyIPv4, 0, mode))
            return;
    }
    discovering_ = true;
    sendDiscoveryQuery();
    discoveryTimer_.start();
}

void QtMultiplayerService::stopDiscovery() {
    discovering_ = false;
    discoveryTimer_.stop();
    if (discoverySocket_.state() != QAbstractSocket::UnconnectedState)
        discoverySocket_.close();
}

void QtMultiplayerService::sendDiscoveryQuery() {
    QJsonObject query;
    query.insert(QStringLiteral("type"), QStringLiteral("discover"));
    query.insert(QStringLiteral("protocol"), QLatin1String(ProtocolName));
    const QByteArray datagram = QJsonDocument(query).toJson(QJsonDocument::Compact);
    // 255.255.255.255, as the firmware sends it, and each IPv4 network's own broadcast address, since a
    // computer with several interfaces sends the limited broadcast on one of them only.
    discoverySocket_.writeDatagram(datagram, QHostAddress::Broadcast, DiscoveryPort);
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& networkInterface : interfaces) {
        const auto flags = networkInterface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::CanBroadcast))
            continue;
        for (const QNetworkAddressEntry& entry : networkInterface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !entry.broadcast().isNull())
                discoverySocket_.writeDatagram(datagram, entry.broadcast(), DiscoveryPort);
        }
    }
}

void QtMultiplayerService::readDiscovery() {
    while (discoverySocket_.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(qMax<qint64>(discoverySocket_.pendingDatagramSize(), 0)));
        discoverySocket_.readDatagram(datagram.data(), datagram.size());
        if (!discovering_)
            continue;
        const QString found = parseDiscovery(datagram);
        if (found.isEmpty())
            continue;
        // The first valid answer, or beacon, wins.
        localEndpoint_ = QUrl(found);
        stopDiscovery();
        fallbackTimer_.stop();
        switchServer(localEndpoint_, true);
        return;
    }
}

void QtMultiplayerService::handleConnected() {
    linkDownTimer_.stop();
    fallbackTimer_.stop();
    stopDiscovery();
    sendJoin();
}

void QtMultiplayerService::handleFallback() {
    if (!memberRequested_) {
        // A search started when the application came up ends here when nothing answered.
        stopDiscovery();
        return;
    }
    if (socket_.state() == QAbstractSocket::ConnectedState)
        return;
    if (endpoint_.isValid() && !endpointIsLocal_)
        return;
    // Nothing found, or a local server that has not answered for 10 s: the typed address, else the public
    // server. The search of the local network goes on and can still move this console to a local server.
    localEndpoint_.clear();
    switchServer(fallbackEndpoint(), false);
    if (localServerEnabled_)
        linkDownTimer_.start();
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
    // The public server is always a last resort, so a room can always be requested.
    return true;
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
    if (!validSignature(room))
        return -1;
    if (memberRequested_)
        leave();
    memberRequested_ = true;
    policyStopped_ = false;
    signature_ = room;
    flags_ = flags;
    reconnectDelayMs_ = ReconnectInitialDelayMs;
    peers_.clear();
    // A network-up search that is still running belongs to this join from now on.
    fallbackTimer_.stop();
    connectToServer();
    return 0;
}

int QtMultiplayerService::leave() {
    memberRequested_ = false;
    reconnectTimer_.stop();
    joinRetryTimer_.stop();
    linkDownTimer_.stop();
    fallbackTimer_.stop();
    stopDiscovery();
    if (admittedPlayerId_ != 0)
        sendText({{"type", "leave"}});
    admittedPlayerId_ = 0;
    signature_.clear();
    peers_.clear();
    // Leaving also closes the socket, as the firmware does: nothing else would use it.
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        switching_ = true;
        socket_.close();
        switching_ = false;
    }
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
    if (switching_ || !memberRequested_ || policyStopped_)
        return;
    scheduleReconnect();
    // A known address that stays silent leads to a search of the local network; a local server that
    // stays silent for 10 s gives way to the fallback.
    if (localServerEnabled_ && !discovering_ && !linkDownTimer_.isActive())
        linkDownTimer_.start();
    if (endpointIsLocal_ && !fallbackTimer_.isActive())
        fallbackTimer_.start();
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

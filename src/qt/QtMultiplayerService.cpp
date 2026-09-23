#include "QtMultiplayerService.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>

namespace {
constexpr uint32_t SendPeriodMs = 50;
constexpr uint32_t PeerTimeoutMs = 3000;
constexpr int MaximumPeers = 8;
} // namespace

QtMultiplayerService::QtMultiplayerService(QObject* parent) : QObject(parent) {
    connect(&socket_, &QWebSocket::connected, this, &QtMultiplayerService::sendJoin);
    connect(&socket_, &QWebSocket::textMessageReceived, this, &QtMultiplayerService::handleMessage);
    connect(&socket_, &QWebSocket::disconnected, this, [this] {
        peers_.clear();
        if (joined_)
            QTimer::singleShot(1000, this, [this] {
                if (joined_ && socket_.state() == QAbstractSocket::UnconnectedState)
                    socket_.open(endpoint_);
            });
    });
}

void QtMultiplayerService::setStoreUrl(const QUrl& storeUrl) {
    QUrl endpoint = storeUrl;
    endpoint.setScheme(storeUrl.scheme() == "https" ? "wss" : "ws");
    QString path = endpoint.path();
    if (!path.endsWith('/'))
        path += '/';
    endpoint.setPath(path + "api/multiplayer");
    endpoint.setQuery({});
    endpoint.setFragment({});
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
    playerId_ = QRandomGenerator::global()->generate();
    if (playerId_ == 0)
        playerId_ = 1;
    local_.playerId = playerId_;
}

bool QtMultiplayerService::available() const {
    return endpoint_.isValid() && (endpoint_.scheme() == "ws" || endpoint_.scheme() == "wss");
}

bool QtMultiplayerService::validSignature(const QString& signature) {
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_.:-]{1,47}$"));
    return pattern.match(signature).hasMatch();
}

int QtMultiplayerService::join(const std::string& signature, uint32_t flags) {
    initialize();
    QString room = QString::fromStdString(signature);
    if (!available() || !validSignature(room))
        return -1;
    if (joined_)
        leave();
    joined_ = true;
    signature_ = room;
    flags_ = flags;
    peers_.clear();
    socket_.open(endpoint_);
    return 0;
}

int QtMultiplayerService::leave() {
    if (joined_)
        sendLeave();
    joined_ = false;
    signature_.clear();
    peers_.clear();
    if (socket_.state() != QAbstractSocket::UnconnectedState)
        socket_.close();
    return 0;
}

void QtMultiplayerService::sendJoin() {
    if (!joined_ || socket_.state() != QAbstractSocket::ConnectedState)
        return;
    QJsonObject message{{"type", "join"},
                        {"signature", signature_},
                        {"flags", double(flags_)},
                        {"player_id", double(playerId_)}};
    socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void QtMultiplayerService::sendLeave() {
    if (socket_.state() != QAbstractSocket::ConnectedState)
        return;
    QJsonObject message{{"type", "leave"}, {"player_id", double(playerId_)}};
    socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void QtMultiplayerService::sendState() {
    if (!joined_ || socket_.state() != QAbstractSocket::ConnectedState)
        return;
    QJsonObject message{{"type", "state"},
                        {"player_id", double(playerId_)},
                        {"x", local_.x},
                        {"y", local_.y},
                        {"sprite", local_.sprite},
                        {"flags", local_.flags},
                        {"input", double(local_.input)},
                        {"frame", double(local_.frame)}};
    socket_.sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
}

void QtMultiplayerService::tick(uint32_t nowMs) {
    prune(nowMs);
    if (!joined_ || uint32_t(nowMs - lastSendMs_) < SendPeriodMs)
        return;
    lastSendMs_ = nowMs;
    sendState();
}

int QtMultiplayerService::setLocalState(int16_t x, int16_t y, uint16_t sprite, uint16_t flags) {
    if (!joined_)
        return -1;
    local_.playerId = playerId_;
    local_.x = x;
    local_.y = y;
    local_.sprite = sprite;
    local_.flags = flags;
    ++local_.frame;
    return 0;
}

int QtMultiplayerService::setInput(uint32_t input) {
    if (!joined_)
        return -1;
    local_.input = input & 0x7fu;
    return 0;
}

void QtMultiplayerService::prune(uint32_t nowMs) {
    for (auto iterator = peers_.begin(); iterator != peers_.end();) {
        if (uint32_t(nowMs - iterator->lastSeenMs) > PeerTimeoutMs)
            iterator = peers_.erase(iterator);
        else
            ++iterator;
    }
}

int QtMultiplayerService::peerCount(uint32_t nowMs) {
    prune(nowMs);
    return peers_.size();
}

int QtMultiplayerService::peer(int index, uint32_t nowMs, prg32::MultiplayerPeer& output) {
    prune(nowMs);
    if (index < 0 || index >= peers_.size())
        return -1;
    auto iterator = peers_.cbegin();
    std::advance(iterator, index);
    output = iterator.value();
    return 0;
}

void QtMultiplayerService::handleMessage(const QString& message) {
    QJsonObject object = QJsonDocument::fromJson(message.toUtf8()).object();
    QString type = object.value("type").toString();
    if (type == "welcome") {
        uint32_t assigned = uint32_t(object.value("player_id").toDouble());
        if (assigned != 0) {
            playerId_ = assigned;
            local_.playerId = assigned;
        }
        return;
    }
    uint32_t id = uint32_t(object.value("player_id").toDouble());
    if (type == "leave") {
        peers_.remove(id);
        return;
    }
    if (type != "peer" || id == 0 || id == playerId_)
        return;
    prg32::MultiplayerPeer peer;
    peer.playerId = id;
    peer.x = int16_t(object.value("x").toInt());
    peer.y = int16_t(object.value("y").toInt());
    peer.sprite = uint16_t(object.value("sprite").toInt());
    peer.flags = uint16_t(object.value("flags").toInt());
    peer.input = uint32_t(object.value("input").toDouble()) & 0x7fu;
    peer.frame = uint32_t(object.value("frame").toDouble());
    peer.lastSeenMs = uint32_t(clock_.elapsed());
    if (peers_.contains(id) || peers_.size() < MaximumPeers)
        peers_[id] = peer;
}

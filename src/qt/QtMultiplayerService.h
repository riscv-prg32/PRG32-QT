#pragma once
#include "Multiplayer.h"
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

/**
 * prg32-multiplayer-1.0 client behind the public PRG32 multiplayer snapshot service.
 *
 * The session follows docs/protocol.md in PRG32-Multiplayer-Server and the firmware's
 * prg32_multiplayer.c: admission only through `welcome`, a new socket is a new session, and a cartridge's
 * request for membership stands across reconnections until it leaves.
 */
class QtMultiplayerService final : public QObject, public prg32::MultiplayerService {
    Q_OBJECT

  public:
    explicit QtMultiplayerService(QObject* parent = nullptr);
    /** Derive the relay address from the Cartridge Store, used while no server address is set. */
    void setStoreUrl(const QUrl& storeUrl);
    /** Set the multiplayer server address; an empty string returns to the Store's relay. */
    void setServerUrl(const QString& serverUrl);
    QString serverUrl() const {
        return serverUrl_;
    }
    void initialize() override;
    bool available() const override;
    int join(const std::string& signature, uint32_t flags) override;
    int leave() override;
    void tick(uint32_t nowMs) override;
    int setLocalState(int16_t x, int16_t y, uint16_t sprite, uint16_t flags) override;
    int setInput(uint32_t input) override;
    int peerCount(uint32_t nowMs) override;
    int peer(int index, uint32_t nowMs, prg32::MultiplayerPeer& output) override;
    uint32_t localPlayerId() const override;

  private:
    static bool validSignature(const QString& signature);
    /** The application's random installation UUID, the protocol's identity source for `prg32-qt`. */
    static QString installationId();
    void applyEndpoint();
    void sendText(const QJsonObject& message);
    void sendJoin();
    void sendState();
    void prune(uint32_t nowMs);
    void handleMessage(const QString& message);
    void handleError(const QString& code);
    void handleDisconnected();
    void scheduleReconnect();
    QWebSocket socket_;
    QElapsedTimer clock_;
    QTimer reconnectTimer_;
    QTimer joinRetryTimer_;
    QUrl storeEndpoint_;
    QUrl endpoint_;
    QString serverUrl_;
    QString signature_;
    uint32_t flags_ = 0;
    /** Player ID requested in `join`: the previous one after a reconnection, as the protocol allows. */
    uint32_t preferredPlayerId_ = 1;
    uint32_t admittedPlayerId_ = 0;
    uint32_t lastSendMs_ = 0;
    uint32_t reconnectDelayMs_ = 1000;
    bool initialized_ = false;
    /** The cartridge asked to be in a room. */
    bool memberRequested_ = false;
    /** The server stopped this session (`device_blocked`): only a new join brings it back. */
    bool policyStopped_ = false;
    prg32::MultiplayerPeer local_;
    /** Peers ordered by player ID, so a cartridge's peer index does not move between frames. */
    QMap<uint32_t, prg32::MultiplayerPeer> peers_;
};

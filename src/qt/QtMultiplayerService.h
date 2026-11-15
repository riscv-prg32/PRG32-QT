#pragma once
#include "Multiplayer.h"
#include <QElapsedTimer>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QUrl>
#include <QWebSocket>

/**
 * prg32-multiplayer-1.0 client for a PRG32 Multiplayer Server.
 *
 * The session follows docs/protocol.md in PRG32-Multiplayer-Server and the firmware's
 * prg32_multiplayer.c: admission only through `welcome`, a new socket is a new session, and a cartridge's
 * request for membership stands across reconnections until it leaves.
 *
 * The server is chosen in the firmware's order (PRG32 `prg32_multiplayer.h`, SDK main `5d2e650`):
 *   1. a server found on the local network by LAN discovery (UDP port 8082), when the local server
 *      setting is on, which is the default;
 *   2. the address typed in Settings;
 *   3. the public server `wss://multi.prg32.uniparthenope.it`, after 10 s of finding nothing, or at once
 *      with the local server off.
 * A local server that does not answer for 10 s gives way to 2 and 3.
 */
class QtMultiplayerService final : public QObject, public prg32::MultiplayerService {
    Q_OBJECT

  public:
    /** The firmware's CONFIG_PRG32_MULTIPLAYER_PUBLIC_SERVER_URL. */
    static constexpr const char* PublicServerUrl = "wss://multi.prg32.uniparthenope.it";
    /** The PRG32 Multiplayer Server's WebSocket port, added to an address typed without one. */
    static constexpr quint16 DefaultServerPort = 8081;
    /** The firmware's CONFIG_PRG32_MULTIPLAYER_DISCOVERY_PORT. */
    static constexpr quint16 DiscoveryPort = 8082;

    explicit QtMultiplayerService(QObject* parent = nullptr);
    /**
     * Set the typed server address. An empty string forgets it. The text is normalized as the firmware's
     * prg32_mp_normalize_server_url() does; returns false, keeping the previous address, when it is invalid.
     */
    bool setServerUrl(const QString& serverUrl);
    /** The typed server address, normalized, or empty. */
    QString serverUrl() const {
        return serverUrl_;
    }
    /** Whether a server found on the local network comes first (the firmware's LOCAL SERVER setting). */
    void setLocalServerEnabled(bool enabled);
    bool localServerEnabled() const {
        return localServerEnabled_;
    }
    /** The address in use now, or the one that would be used next; empty while still searching. */
    QString currentServerUrl() const {
        return endpoint_.toString();
    }
    /** True when the address in use was found on the local network (the firmware shows it as LOCAL). */
    bool currentServerIsLocal() const {
        return endpointIsLocal_;
    }
    /** True while the WebSocket is open and the server has admitted this console. */
    bool admitted() const {
        return admittedPlayerId_ != 0;
    }
    /** Start a short search of the local network, as the firmware does when Wi-Fi connects. */
    void searchLocalNetwork();

    /**
     * Turn typed text into a server URL, as the firmware's prg32_mp_normalize_server_url(): a ws:// or
     * wss:// URL is kept; a bare host becomes ws://host:8081 and host:port becomes ws://host:port. Returns
     * an empty string for another scheme, an invalid port or URL.
     */
    static QString normalizeServerUrl(const QString& text);
    /**
     * The firmware's prg32_mp_valid_server_url(): ws:// or wss://, a host, printable ASCII without
     * spaces, and at most 127 characters.
     */
    static bool validServerUrl(const QString& url);
    /**
     * The firmware's prg32_mp_parse_discovery(): returns the URL of a `server` message of this protocol,
     * or an empty string for anything else.
     */
    static QString parseDiscovery(const QByteArray& datagram);

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

  signals:
    /** The address in use, or whether it is local, changed. */
    void currentServerChanged();

  private:
    static bool validSignature(const QString& signature);
    /** The application's random installation UUID, the protocol's identity source for `prg32-qt`. */
    static QString installationId();
    /** The typed address, else the public server; the firmware's fallback after local discovery. */
    QUrl fallbackEndpoint() const;
    /** Pick the address for the next connection and open it, or start searching when there is none. */
    void connectToServer();
    /** Close any socket and move to `endpoint`; the firmware's switch_server(). */
    void switchServer(const QUrl& endpoint, bool local);
    void setEndpoint(const QUrl& endpoint, bool local);
    void startDiscovery();
    void stopDiscovery();
    void sendDiscoveryQuery();
    void readDiscovery();
    void handleConnected();
    void handleFallback();
    void sendText(const QJsonObject& message);
    void sendJoin();
    void sendState();
    void prune(uint32_t nowMs);
    void handleMessage(const QString& message);
    void handleError(const QString& code);
    void handleDisconnected();
    void scheduleReconnect();
    QWebSocket socket_;
    QUdpSocket discoverySocket_;
    QElapsedTimer clock_;
    QTimer reconnectTimer_;
    QTimer joinRetryTimer_;
    /** Repeats the discovery query while a search runs. */
    QTimer discoveryTimer_;
    /** Ends a search that found nothing: the public fallback, or the end of a network-up search. */
    QTimer fallbackTimer_;
    /** Starts a search when a known address has not answered for a while after a lost connection. */
    QTimer linkDownTimer_;
    QUrl endpoint_;
    /** Server most recently found on the local network; kept until the setting changes. */
    QUrl localEndpoint_;
    QString serverUrl_;
    QString signature_;
    uint32_t flags_ = 0;
    /** Player ID requested in `join`: the previous one after a reconnection, as the protocol allows. */
    uint32_t preferredPlayerId_ = 1;
    uint32_t admittedPlayerId_ = 0;
    uint32_t lastSendMs_ = 0;
    uint32_t reconnectDelayMs_ = 1000;
    bool initialized_ = false;
    bool localServerEnabled_ = true;
    bool endpointIsLocal_ = false;
    /** A search of the local network is running. */
    bool discovering_ = false;
    /** The socket is being closed on purpose to move to another server; its disconnection is expected. */
    bool switching_ = false;
    /** The cartridge asked to be in a room. */
    bool memberRequested_ = false;
    /** The server stopped this session (`device_blocked`): only a new join brings it back. */
    bool policyStopped_ = false;
    prg32::MultiplayerPeer local_;
    /** Peers ordered by player ID, so a cartridge's peer index does not move between frames. */
    QMap<uint32_t, prg32::MultiplayerPeer> peers_;
};

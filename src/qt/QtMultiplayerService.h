#pragma once
#include "Multiplayer.h"
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QUrl>
#include <QWebSocket>

/** Store-compatible WebSocket implementation of the public PRG32 multiplayer snapshot service. */
class QtMultiplayerService final : public QObject, public prg32::MultiplayerService {
    Q_OBJECT

  public:
    explicit QtMultiplayerService(QObject* parent = nullptr);
    void setStoreUrl(const QUrl& storeUrl);
    void initialize() override;
    bool available() const override;
    int join(const std::string& signature, uint32_t flags) override;
    int leave() override;
    void tick(uint32_t nowMs) override;
    int setLocalState(int16_t x, int16_t y, uint16_t sprite, uint16_t flags) override;
    int setInput(uint32_t input) override;
    int peerCount(uint32_t nowMs) override;
    int peer(int index, uint32_t nowMs, prg32::MultiplayerPeer& output) override;

  private:
    static bool validSignature(const QString& signature);
    void sendJoin();
    void sendLeave();
    void sendState();
    void prune(uint32_t nowMs);
    void handleMessage(const QString& message);
    QWebSocket socket_;
    QElapsedTimer clock_;
    QUrl endpoint_;
    QString signature_;
    uint32_t flags_ = 0;
    uint32_t playerId_ = 1;
    uint32_t lastSendMs_ = 0;
    bool initialized_ = false;
    bool joined_ = false;
    prg32::MultiplayerPeer local_;
    QHash<uint32_t, prg32::MultiplayerPeer> peers_;
};

#pragma once

#include <QByteArray>
#include <QObject>
#include <QTcpServer>

class AppController;
class QTcpSocket;

class IoProtocolServer : public QObject {
    Q_OBJECT

  public:
    explicit IoProtocolServer(AppController* controller, QObject* parent = nullptr);
    quint16 port() const {
        return server_.serverPort();
    }

  private:
    void process(QTcpSocket* socket, QByteArray& buffer);
    void handle(QTcpSocket* socket, quint8 type, quint32 requestId, const QByteArray& payload);
    void send(QTcpSocket* socket, quint8 type, quint16 flags, quint32 requestId, const QByteArray& payload);
    void status(QTcpSocket* socket, quint32 requestId, bool ok, const QString& message);

    QTcpServer server_;
    AppController* controller_;
    QByteArray upload_;
    QString uploadName_;
    QString uploadCommand_;
    int uploadSlot_ = -1;
    quint32 uploadSize_ = 0;
    quint32 uploadCrc_ = 0;
};

#pragma once
#include <QObject>
#include <QTcpServer>
class AppController;

class WebApiServer final : public QObject {
    Q_OBJECT
  public:
    explicit WebApiServer(AppController* controller, QObject* parent = nullptr);
    bool listening() const {
        return server_.isListening();
    }

  private:
    void serve(class QTcpSocket* socket, const QByteArray& request);
    AppController* controller_;
    QTcpServer server_;
};

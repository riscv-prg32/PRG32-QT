#include "WebApiServer.h"
#include "AppController.h"
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <memory>

WebApiServer::WebApiServer(AppController *controller, QObject *parent)
    : QObject(parent), controller_(controller) {
    connect(&server_, &QTcpServer::newConnection, this, [this] {
        while (auto *socket = server_.nextPendingConnection()) {
            auto buffer = std::make_shared<QByteArray>();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                buffer->append(socket->readAll());
                if (buffer->size() > int(prg32::Cartridge::MaximumGuestBytes) + 16384) {
                    socket->write("HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                int end = buffer->indexOf("\r\n\r\n");
                if (end < 0) return;
                int contentLength = 0;
                for (const auto &line : buffer->left(end).split('\n')) {
                    if (line.trimmed().toLower().startsWith("content-length:"))
                        contentLength = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                }
                if (contentLength < 0 || contentLength > int(prg32::Cartridge::MaximumGuestBytes)) {
                    socket->write("HTTP/1.1 413 Payload Too Large\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                    socket->disconnectFromHost();
                    return;
                }
                if (buffer->size() < end + 4 + contentLength) return;
                serve(socket, buffer->left(end + 4 + contentLength));
                socket->disconnectFromHost();
            });
        }
    });
    server_.listen(QHostAddress::AnyIPv4, 8080);
}

void WebApiServer::serve(QTcpSocket *socket, const QByteArray &request) {
    int headerEnd = request.indexOf("\r\n\r\n");
    auto first = request.left(request.indexOf("\r\n")).split(' ');
    QByteArray body = request.mid(headerEnd + 4);
    int status = 200;
    QByteArray mime = "application/json";
    QByteArray output;
    auto json = [&](const QJsonValue &value) {
        output = value.isArray() ? QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact)
                                 : QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
    };
    if (first.size() < 2) status = 400;
    else {
        QByteArray method = first[0];
        QUrl url(QString::fromUtf8(first[1]));
        QString path = url.path();
        QUrlQuery query(url);
        auto slotText = query.queryItemValue("slot");
        int slot = slotText.startsWith("cart") ? slotText.mid(4).toInt() : slotText.isEmpty() ? 0 : -1;
        if (method == "GET" && (path == "/api" || path == "/api/")) {
            QJsonArray endpoints;
            for (const auto &entry : {qMakePair("GET","/api"),qMakePair("GET","/api/"),qMakePair("GET","/api/runtime"),qMakePair("GET","/api/games"),qMakePair("POST","/api/games"),qMakePair("POST","/api/games/select"),qMakePair("GET","/api/screenshot.bmp"),qMakePair("GET","/api/performance.json"),qMakePair("GET","/api/scores"),qMakePair("POST","/api/scores"),qMakePair("GET","/api/memory")})
                endpoints.append(QJsonObject{{"method",entry.first},{"path",entry.second},{"available",true}});
            json(QJsonObject{{"ok",true},{"service","PRG32"},{"endpoints",endpoints}});
        } else if (method == "GET" && path == "/api/runtime") json(controller_->runtimeJson());
        else if (method == "GET" && path == "/api/games") json(controller_->gamesJson());
        else if (method == "GET" && path == "/api/performance.json") json(controller_->performanceJson());
        else if (method == "GET" && path == "/api/memory") json(controller_->memoryJson());
        else if (method == "GET" && path == "/api/scores") json(controller_->scoresJson());
        else if (method == "GET" && path == "/api/screenshot.bmp") { mime = "image/bmp"; output = controller_->screenshotBmp(); }
        else if (method == "POST" && path == "/api/games") {
            QString error;
            if (!controller_->uploadSlot(slot,body,error)) { status=400;json(QJsonObject{{"ok",false},{"error",error}}); }
            else json(QJsonObject{{"ok",true},{"slot",QString("cart%1").arg(slot)},{"stored",true},{"loaded",false}});
        } else if (method == "POST" && path == "/api/games/select") {
            QString error;
            if (!controller_->selectSlot(slot,error)) { status=400;json(QJsonObject{{"ok",false},{"error",error}}); }
            else json(QJsonObject{{"ok",true},{"slot",QString("cart%1").arg(slot)}});
        } else if (method == "POST" && path == "/api/scores") {
            QString error;auto o=QJsonDocument::fromJson(body).object();
            if (!controller_->submitScore(o,error)) { status=400;json(QJsonObject{{"ok",false},{"error",error}}); }
            else json(QJsonObject{{"ok",true}});
        } else { status=404;json(QJsonObject{{"ok",false},{"error","unknown endpoint"}}); }
    }
    if (output.isEmpty()) output = "{}";
    QByteArray reason = status == 200 ? "OK" : status == 400 ? "Bad Request" : "Not Found";
    QByteArray headers = "HTTP/1.1 " + QByteArray::number(status) + " " + reason + "\r\nContent-Type: " + mime + "\r\nContent-Length: " + QByteArray::number(output.size()) + "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    socket->write(headers);
    socket->write(output);
}

#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
class StoreClient:public QObject{
 Q_OBJECT
 Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY baseUrlChanged)
 Q_PROPERTY(QJsonArray cartridges READ cartridges NOTIFY cartridgesChanged)
 Q_PROPERTY(QString error READ error NOTIFY errorChanged)
 Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
public:
 explicit StoreClient(QObject*p=nullptr);QString baseUrl()const{return base_.toString();}void setBaseUrl(const QString&);QJsonArray cartridges()const{return carts_;}QString error()const{return error_;}bool loading()const{return loading_;}
 Q_INVOKABLE void refresh();Q_INVOKABLE void downloadGame(const QJsonObject&game);Q_INVOKABLE QString iconUrl(const QString&id)const;Q_INVOKABLE void resetDefault();
signals:void baseUrlChanged();void cartridgesChanged();void errorChanged();void loadingChanged();void cartridgeDownloaded(const QString&id,const QByteArray&data);
private:
 void setError(QString);void setLoading(bool);void fetchGamesDirect(bool allowDiscoveryFallback=true);void fetchDiscovery();void fetchCatalog(const QJsonObject&);QUrl join(QString)const;QNetworkAccessManager net_;QUrl base_;QJsonArray carts_;QString error_;bool loading_=false;
};

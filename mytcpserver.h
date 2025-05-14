#ifndef MYTCPSERVER_H
#define MYTCPSERVER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QDateTime>

class myTcpServer : public QObject
{
    Q_OBJECT
public:
    explicit myTcpServer(QObject *parent = nullptr);
    void startServer(quint16 port);

    bool isClientConnected();

signals:
    void sendToUi(const QString &data);

private slots:
    void onNewConnection();
    void onClientMessage(QTcpSocket* client);

public slots:
    void recvFromGui(const QString& data);

private:
    QTcpServer *myServer;
    QList<QTcpSocket*> clientSockets; // Store all client connections

    bool Connection = false;

};

#endif // MYTCPSERVER_H

#include "mytcpserver.h"

myTcpServer::myTcpServer(QObject *parent) : QObject(parent)
{
    myServer = new QTcpServer(this);
    connect(myServer,&QTcpServer::newConnection,this,&myTcpServer::onNewConnection);
}

void myTcpServer::startServer(quint16 port)
{
    if(!myServer->listen(QHostAddress::Any,port))
    {
        qDebug()<<"Server Failed To Start "<<myServer->errorString();
    }
    else
    {
        qDebug()<<"Server Connected on port "<<port;
    }
}

bool myTcpServer::isClientConnected()
{
    return Connection;
}


void myTcpServer::onNewConnection()
{
    // Check if there's a pending connection
    if (myServer->hasPendingConnections()) {
        qDebug() << "There is a pending connection to process.";
    } else {
        qDebug() << "No pending connections.";
        emit sendToUi("No pending connections.");
    }

    // Get the next pending connection
    QTcpSocket* newClientSocket = myServer->nextPendingConnection();

    // Check if the connection is valid
    if (newClientSocket) {
        Connection = true;
        clientSockets.append(newClientSocket); // Add the new client to the list

        qDebug()<<clientSockets<<" :How does socket look";

        qDebug() << "New Client Connected.";
        QString myTime = QDateTime::currentDateTime().toString("hh:mm:ss:zzz dd/MM/yyyy");
        emit sendToUi("### New Client Connected ###.  "+myTime);

        qDebug() << "Client IP: " << newClientSocket->peerAddress().toString();
        QString realIp = newClientSocket->peerAddress().toString();

        emit sendToUi("Client IP: " + realIp.mid(7) + " Client Port: " + QString::number(newClientSocket->peerPort()));

        qDebug() << "Client Port: " << newClientSocket->peerPort();

        // Connect the readyRead signal to a lambda or slot for this specific client
        connect(newClientSocket, &QTcpSocket::readyRead, this, [this, newClientSocket]() {
            onClientMessage(newClientSocket); // Pass the specific client socket to the message handler
        });

        // Handle client disconnection
        connect(newClientSocket, &QTcpSocket::disconnected, this, [this, newClientSocket]() {
            clientSockets.removeOne(newClientSocket);
            newClientSocket->deleteLater();
            qDebug() << "Client disconnected.";
            emit sendToUi("Client disconnected.");
        });

    } else {
        Connection = false;
        qDebug() << "Failed to get a valid client connection.";
        emit sendToUi("Failed to get a valid client connection.");
    }
}


void myTcpServer::onClientMessage(QTcpSocket* client)
{
    QByteArray data = client->readAll(); // Read data from the specific client
    QString addr = client->peerAddress().toString(); // Get client IP address
    quint16 port = client->peerPort(); // Get client port
    QString nowTime = QDateTime::currentDateTime().toString("[hh:mm:ss:zzz dd/MM/yyyy]");

    // Format the message with IP, port, and received data
    QByteArray fullMessage = QString("STime: %1 Client IP: %2, Port: %3, Message: %4")
            .arg(nowTime)
            .arg(addr.mid(7))
            .arg(port)
            .arg(QString(data))
            .toUtf8();

    qDebug() << "Full Message: " << fullMessage;

    emit sendToUi(fullMessage); // Emit formatted message
}


void myTcpServer::recvFromGui(const QString &data)
{
    // Iterate through all connected clients and send the message
    for (QTcpSocket *socket : clientSockets) {
        if (socket && socket->state() == QAbstractSocket::ConnectedState) {
            socket->write("*Server Message*: " + data.toUtf8());
        }
    }
}


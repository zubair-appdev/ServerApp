#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mytcpserver.h"  // Include the header for myTcpServer

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Server Application");

    // Create myTcpServer as a member object
    myServer = new myTcpServer(this);  // Dynamically allocate server object

    // Start the server
    myServer->startServer(1234);

    // Connect the signal to the slot
    connect(myServer, &myTcpServer::sendToUi, this, &MainWindow::handleRawData);
    connect(this,&MainWindow::sendToRaw,myServer,&myTcpServer::recvFromGui);

}

MainWindow::~MainWindow()
{
    delete ui;
    delete myServer;  // Clean up the server object
}

void MainWindow::handleRawData(const QString &rawData)
{
    ui->textEdit_server->append(rawData);
}

void MainWindow::on_pushButton_serverSend_clicked()
{
    QString nowTime = QDateTime::currentDateTime().toString("[hh:mm:ss:zzz dd/MM/yyyy]");
    QString serverToClient = ui->textEdit_serverToClient->toPlainText() + "&nbsp;&nbsp;&nbsp;" + nowTime;

    if (myServer->isClientConnected())
    {
        emit sendToRaw(serverToClient);
        ui->textEdit_server->append("Sending message ...  " +ui->textEdit_serverToClient->toPlainText());
        ui->textEdit_serverToClient->clear();
    }
    else
    {
        // Show a warning message if no client is connected
        QMessageBox::warning(this, "Warning", "No client connected. Cannot send message.");
    }
}



#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mytcpserver.h"  // Include the header for myTcpServer

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle("Server Application");

    serverConnected = false;

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



void MainWindow::on_pushButton_startServer_clicked()
{
    if(serverConnected == false)
    {
        serverConnected = true;

        // Create myTcpServer as a member object
        myServer = new myTcpServer(this);  // Dynamically allocate server object

        int port = ui->spinBox_serverPort->value();
        this->port = port;
        // Start the server
        myServer->startServer(port);
        ui->textEdit_server->append("Server Listening to "+QString::number(port));

        // Connect the signal to the slot
        connect(myServer, &myTcpServer::sendToUi, this, &MainWindow::handleRawData);
        connect(this,&MainWindow::sendToRaw,myServer,&myTcpServer::recvFromGui);
        connect(this,&MainWindow::sendToRawFile,myServer,&myTcpServer::recvFromGuiFile);
    }
    else
    {
        QMessageBox::warning(this,"Already Connected","Server Already Listening to port "
                             +QString::number(port) + " .Please restart application to connect new port");
    }
}

void MainWindow::on_pushButton_sendFile_clicked()
{
    if(!ui->radioButton_ultraSpeedMode->isChecked() &&
       !ui->radioButton_speedMode->isChecked()      &&
       !ui->radioButton_steadyMode->isChecked())
    {
        QMessageBox::information(this,"Select","Please Select type of speed");
        return;
    }

    static int packetCounter;
    int msecSpeed;

    QString filePath = QFileDialog::getOpenFileName(this, "Select File to Send",
                                                    QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    if (filePath.isEmpty()) return;

    QFile *file = new QFile(filePath, this);
    if (!file->open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, "Error", "Failed to open file.");
        delete file;
        return;
    }

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    qint64 fileSize = file->size();

    // Custom Speed Logic Start ######################################################
    if(ui->radioButton_ultraSpeedMode->isChecked())
    {
        ui->textEdit_server->append("\n*** Selected Ultra Speed Mode : Don't touch ui Heavy Operation Under Process ***\n");
        msecSpeed = 0;
    }
    else if(ui->radioButton_steadyMode->isChecked())
    {
        ui->textEdit_server->append("\n*** Selected Steady Mode : File Transfer happens little bit slow ***\n");
        msecSpeed = 1;
    }
    else if(ui->radioButton_speedMode)
    {
        ui->textEdit_server->append("\n*** Selected Speed Mode : File Transfer happens at balanced rate ***\n");
        msecSpeed = 2;
    }
    else
    {
        ui->textEdit_server->append("Invalid Speed");
        msecSpeed = 999;

    }
    // Custom Speed Logic End ######################################################

    if (myServer->isClientConnected())
    {
        // First, send metadata (file name + size)
        QString realHeader = "FILE_*_"+fileName+"_*_"+QString::number(fileSize)+"!!#!!";
        QByteArray header = realHeader.toUtf8();
        emit sendToRawFile(header);


        // Send the file in chunks
        const qint64 chunkSize = 64 * 1024; // 64 KB
        while (!file->atEnd())
        {
            packetCounter++;

            QByteArray chunk = file->read(chunkSize);
            emit sendToRawFile(chunk);

            if(msecSpeed == 0)
            {
                lightPause(msecSpeed);
            }
            else if(msecSpeed == 1)
            {
                lightPause(msecSpeed);
            }
            else
            {
                //Speed Mode
                if(packetCounter % 2 == 0)
                {
                    lightPause(1);
                }
            }

            qApp->processEvents(); // Keep UI responsive

        }

        ui->textEdit_server->append("Repeater : "+QString::number(packetCounter));
        QString ender = "###_FILE_DONE_###";
        QByteArray doneMsg = ender.toUtf8();
        emit sendToRawFile(doneMsg);


        file->close();
        file->deleteLater();
        ui->textEdit_server->append("File sent: " + fileName);
    }
}


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <mytcpserver.h>
#include <QDateTime>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QEventLoop>
#include <QTimer>
#include <QApplication>
#include <QStandardPaths>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    inline void lightPause(quint8 msec)
    {
        QEventLoop loop;
        QTimer::singleShot(msec, &loop, &QEventLoop::quit);
        loop.exec();
        QApplication::processEvents();  // Keep UI healthy
    }

public slots:
    void handleRawData(const QString &rawData);

signals:
    void sendToRaw(const QString &data);
    void sendToRawFile(const QByteArray &data);    // for file chunks

private slots:
    void on_pushButton_serverSend_clicked();

    void on_pushButton_startServer_clicked();

    void on_pushButton_sendFile_clicked();

private:
    Ui::MainWindow *ui;

    myTcpServer *myServer;

    bool serverConnected;
    int port;
};
#endif // MAINWINDOW_H

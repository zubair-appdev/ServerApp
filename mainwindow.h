#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <mytcpserver.h>
#include <QDateTime>
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void handleRawData(const QString &rawData);

signals:
    void sendToRaw(const QString &data);

private slots:
    void on_pushButton_serverSend_clicked();

private:
    Ui::MainWindow *ui;

    myTcpServer *myServer;
};
#endif // MAINWINDOW_H

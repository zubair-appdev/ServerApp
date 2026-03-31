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
#include <QTextEdit>
#include <QKeyEvent>
#include <QRandomGenerator>

#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QSoundEffect>

#include <chrono>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    //PTP IEEE 1588 Code
    static qint64 now_us()
    {
        auto now =
            std::chrono::high_resolution_clock::now();

        return std::chrono::duration_cast
        <
            std::chrono::microseconds
        >(now.time_since_epoch()).count();
    }

    void startPtpSync();

    inline void lightPause(quint8 msec)
    {
        QEventLoop loop;
        QTimer::singleShot(msec, &loop, &QEventLoop::quit);
        loop.exec();
        QApplication::processEvents();  // Keep UI healthy
    }

    void scrollDown(QTextEdit *myEdit);

    void spawnInitialFood();

    void startGameTimer();

    void checkFoodCollision();

    void handleFoodEaten(int id);

    void removeFoodById(int id);

    void handleGameOver();

    void resetThings();

    void initializeMusic();

    void spawnSpecialBall();

    void generateSpecialSpawnSchedule();

    void checkSpecialCollision();

    void removeSpecialById(int id);

    QRect drawLaser(
        int direction,
        bool isClient
    );

    void checkLaserHit(
        QRect laserRect,
        bool firedByClient
    );

protected:
    void keyPressEvent(QKeyEvent *event) override;

public slots:
    void handleRawData(const QString &rawData);

signals:
    void sendToRaw(const QString &data);
    void sendToRawFile(const QByteArray &data);    // for file chunks

private slots:
    void on_pushButton_serverSend_clicked();

    void on_pushButton_startServer_clicked();

    void on_pushButton_sendFile_clicked();

    void on_actionPlay_Game_triggered();

    void on_pushButton_back_clicked();

    void setInitialPos();

    void on_pushButton_generateEatables_clicked();

    void on_actionSync_720p_triggered();

    void on_actionPTP_Page_triggered();

    void on_pushButton_back_ptp_clicked();

    void on_pushButton_startPTP_clicked();

    void on_pushButton_stopPTP_clicked();

    void on_actionHelp_triggered();

private:
    Ui::MainWindow *ui;

    myTcpServer *myServer;

    bool serverConnected;
    int port;

    QTimer *gameTimer = nullptr;
    int remainingTime = 0;

    QVector<QLabel*> eatables;

    const int TOTAL_FOOD = 25;

    int myScore = 0;      // server score
    int enemyScore = 0;   // client score (later use)

    int globalFoodId = 0;

    QMediaPlayer *bgMusic;
    QMediaPlaylist *playlist;
    QSoundEffect *eatSound;
    QSoundEffect *laserSound;

    QTimer *ptpTimer = nullptr;

    qint64 t1 = 0;
    qint64 t3 = 0;
    qint64 t4 = 0;

    // SPECIAL BALL SYSTEM

    QVector<QLabel*> specialBalls;

    QVector<int> specialSpawnTimes;

    int specialBallId = 0;

    int bulletCount = 0;
    int clientBulletCount = 0;

    enum Direction
    {
        RIGHT = 0,
        LEFT,
        UP,
        DOWN
    };

    int lastDirection = RIGHT;

    int boostSteps = 0;      // remaining boost moves
    int normalStep = 0;      // default movement
    int boostStep = 0;      // boosted movement
    bool useBoost = false;   // next move uses boost

    int clientBoostSteps = 0;
};
#endif // MAINWINDOW_H

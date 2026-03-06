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

    // Gaming session
    ui->stackedWidget->setCurrentWidget(ui->page_main);

    // Music
    initializeMusic();
}

MainWindow::~MainWindow()
{
    delete ui;
    delete myServer;  // Clean up the server object
}

void MainWindow::scrollDown(QTextEdit *myEdit)
{
    // Always scroll to bottom
       QTextCursor cursor = myEdit->textCursor();
       cursor.movePosition(QTextCursor::End);
       myEdit->setTextCursor(cursor);
       myEdit->ensureCursorVisible();
}

void MainWindow::spawnInitialFood()
{
    QWidget *pad = ui->groupBox_gamePad;

    for(int i=0; i<TOTAL_FOOD; i++)
    {
        // ✅ generate unique id
        int id = globalFoodId++;   // ✅ unique forever

        double rx = 0.05 + QRandomGenerator::global()->bounded(0.90);
        double ry = 0.05 + QRandomGenerator::global()->bounded(0.90);

        QLabel *food = new QLabel(pad);
        food->resize(14,14);
        food->setStyleSheet("background:orange; border-radius:7px;");

        int x = rx * (pad->width()  - food->width());
        int y = ry * (pad->height() - food->height());

        food->move(x,y);
        food->show();

        // ✅ attach ID to widget
        food->setProperty("foodId", id);

        eatables.append(food);

        // ✅ send id + ratios to client
        QString msg = QString("@@@FOOD@@@_%1_%2_%3")
                        .arg(id)
                        .arg(rx)
                        .arg(ry);

        emit sendToRaw(msg);
    }
}

void MainWindow::startGameTimer()
{
    remainingTime = ui->spinBox_gameTime->value() * 60;

    if(gameTimer)
    {
        gameTimer->stop();
        gameTimer->deleteLater();
    }

    gameTimer = new QTimer(this);

    connect(gameTimer, &QTimer::timeout, this, [=]()
    {
        remainingTime--;

        int m = remainingTime / 60;
        int s = remainingTime % 60;

        ui->label_timer->setText(
            QString("%1:%2")
            .arg(m,2,10,QChar('0'))
            .arg(s,2,10,QChar('0'))
        );

        emit sendToRaw("@@@TIME@@@_" + QString::number(remainingTime));

        // 🔥 AUTO RESPAWN LOGIC
        if(eatables.size() < 5)
        {
            qDebug() << "Low food! Respawning 25 new foods...";
            spawnInitialFood();   // will create 25 new with new IDs
        }

        // 🔥 GAME OVER
        if(remainingTime <= 0)
        {
            gameTimer->stop();
            handleGameOver();
        }

    });

    gameTimer->start(1000);
}

void MainWindow::checkFoodCollision()
{
    for(int i=0; i<eatables.size(); i++)
    {
        if(ui->label_gameBox->geometry().intersects(
           eatables[i]->geometry()))
        {
            int id = eatables[i]->property("foodId").toInt();

            // 🔥 server handles directly
            handleFoodEaten(id);

            return;
        }
    }
}

void MainWindow::handleFoodEaten(int id)
{
    removeFoodById(id);

    myScore++;
    eatSound->play();
    ui->label_myScore->setText(QString::number(myScore));

    emit sendToRaw(QString("@@@REMOVE@@@_%1").arg(id));
    emit sendToRaw(QString("@@@SCORE_SERVER@@@_%1").arg(myScore));
}

void MainWindow::removeFoodById(int id)
{
    for(int i = 0; i < eatables.size(); i++)
    {
        if(eatables[i]->property("foodId").toInt() == id)
        {
            eatables[i]->deleteLater();
            eatables.remove(i);
            return;
        }
    }
}

void MainWindow::handleGameOver()
{
    QString result;

    if(myScore > enemyScore)
        result = "YOU WIN!";
    else if(enemyScore > myScore)
        result = "CLIENT WINS!";
    else
        result = "DRAW!";

    // 🔥 Stop timer if running
    if(gameTimer)
        gameTimer->stop();

    // 🔥 Clear all eatables
    for(QLabel* f : eatables)
        f->deleteLater();

    eatables.clear();

    // 🔥 Reset food ID counter
    globalFoodId = 0;

    // 🔥 Notify client with opposite result
    if(result == "YOU WIN!")
        emit sendToRaw("@@@GAME_OVER@@@_YOU LOSE !!!");
    else if(result == "CLIENT WINS!")
        emit sendToRaw("@@@GAME_OVER@@@_YOU WIN !!!");
    else
        emit sendToRaw("@@@GAME_OVER@@@_DRAW!");

    bgMusic->stop();

    QMessageBox::information(this, "GAME OVER", result);

    // ===================================
    // 🔥 RESET SCORES (IMPORTANT)
    // ===================================

    myScore = 0;
    enemyScore = 0;

    ui->label_myScore->setText("0");
    ui->label_enemyScore->setText("0");

    // Optional: reset timer label
    ui->label_timer->setText("00:00");
}

void MainWindow::resetThings()
{
    if(gameTimer)
        gameTimer->stop();

    // 🔥 Clear all eatables
    for(QLabel* f : eatables)
        f->deleteLater();

    remainingTime = 0;
    myScore = 0;
    enemyScore = 0;
    globalFoodId = 0;

    eatables.clear();

    ui->label_timer->setText("00:00");
    ui->label_myScore->setText("0");
    ui->label_enemyScore->setText("0");
}

void MainWindow::initializeMusic()
{
    bgMusic = new QMediaPlayer(this);

    playlist = new QMediaPlaylist(this);
    playlist->addMedia(QUrl("qrc:/new/prefix1/mixkit-wedding-01-657.mp3"));
    playlist->setPlaybackMode(QMediaPlaylist::Loop);

    bgMusic->setPlaylist(playlist);
    bgMusic->setVolume(60);


    // eating sound
    eatSound = new QSoundEffect(this);
    eatSound->setSource(QUrl("qrc:/new/prefix1/mixkit-video-game-retro-click-237.wav"));
    eatSound->setVolume(0.9);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QLabel *box = ui->label_gameBox;
    QWidget *pad = ui->groupBox_gamePad;

    int step = 5; // movement speed (pixels)

    QPoint pos = box->pos();

    switch (event->key())
    {
        case Qt::Key_Left:
            pos.rx() -= step;
            break;

        case Qt::Key_Right:
            pos.rx() += step;
            break;

        case Qt::Key_Up:
            pos.ry() -= step;
            break;

        case Qt::Key_Down:
            pos.ry() += step;
            break;

        default:
            QMainWindow::keyPressEvent(event);
            return;
    }

    // 🔒 Boundary check (stay inside groupBox)
    int maxX = pad->width() - box->width();
    int maxY = pad->height() -box->height();

    qDebug()<<maxX<<" :maxX "<<maxY<<" :maxY";
    qDebug()<<pos.x()<<" :x direction";
    qDebug()<<pos.y()<<" :y direction";

    pos.setX(qBound(0, pos.x(), maxX));
    int initialY = 0 + pad->height() * 0.01;
    pos.setY(qBound(initialY, pos.y(), maxY));

    box->move(pos);
    checkFoodCollision();

    // 🔥 send movement to clients
    QString msg = QString("@@@MOVE_SERVER@@@_%1_%2")
                    .arg(pos.x())
                    .arg(pos.y());

    emit sendToRaw(msg);
}

void MainWindow::handleRawData(const QString &rawData)
{
    // show raw data in UI log
    ui->textEdit_server->append(rawData);

    // =========================
    // HANDLE MOVE PACKETS
    // =========================
    QRegularExpression reMove("@@@MOVE_CLIENT@@@_(\\d+)_(\\d+)");
    QRegularExpressionMatchIterator moveIt = reMove.globalMatch(rawData);

    while (moveIt.hasNext())
    {
        QRegularExpressionMatch match = moveIt.next();

        int x = match.captured(1).toInt();
        int y = match.captured(2).toInt();

        qDebug() << "Enemy Move:" << x << y;

        ui->label_gameBox_enemy->move(x, y);
    }

    // =========================
    // HANDLE FOOD EAT PACKET
    // =========================
    QRegularExpression reAte("@@@ATE@@@_(\\d+)");
    QRegularExpressionMatch ateMatch = reAte.match(rawData);

    if (ateMatch.hasMatch())
    {
        int id = ateMatch.captured(1).toInt();

        qDebug() << "Food eaten by client. ID:" << id;

        // remove food from UI
        removeFoodById(id);

        // update enemy score
        enemyScore++;
        ui->label_enemyScore->setText(QString::number(enemyScore));

        // sync removal + score to client
        emit sendToRaw(QString("@@@REMOVE@@@_%1").arg(id));
        emit sendToRaw(QString("@@@SCORE_CLIENT@@@_%1").arg(enemyScore));
    }
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

    scrollDown(ui->textEdit_server);
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
        //Read
        connect(myServer, &myTcpServer::sendToUi, this, &MainWindow::handleRawData);
        //write
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


        file->close();
        file->deleteLater();
        ui->textEdit_server->append("File sent: " + fileName);
        scrollDown(ui->textEdit_server);
    }
}


void MainWindow::on_actionPlay_Game_triggered()
{
    ui->stackedWidget->setCurrentWidget(ui->page_game);

    resetThings();

    // Enable key handling
    this->setFocusPolicy(Qt::StrongFocus);
    this->setFocus();

    setInitialPos();
}

void MainWindow::on_pushButton_back_clicked()
{
    this->setFixedSize(700,600);

    resetThings();

    bgMusic->stop();

    ui->stackedWidget->setCurrentWidget(ui->page_main);

    // Disable key handling
    this->setFocusPolicy(Qt::NoFocus);
}

void MainWindow::setInitialPos()
{
    //Setting positions
    QWidget *pad = ui->groupBox_gamePad;
    QLabel *me = ui->label_gameBox;
    QLabel *enemy = ui->label_gameBox_enemy;

    // 🔹 compute vertical center
    int centerY = (pad->height() - me->height()) / 2;

    // 🔹 place at extremes
    enemy->move(0, centerY);   // LEFT extreme
    me->move(pad->width() - me->width(), centerY); // RIGHT extreme
}


void MainWindow::on_pushButton_generateEatables_clicked()
{

    if(remainingTime > 0)
    {
        QMessageBox::warning(this,"Error","Game is still running");
        return;
    }

    bgMusic->play();

    resetThings();

    spawnInitialFood();

    startGameTimer();
}

void MainWindow::on_actionSync_720p_triggered()
{
    this->setFixedSize(1280,720);
}

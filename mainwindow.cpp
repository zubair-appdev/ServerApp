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

    // Steps
    normalStep = ui->groupBox_gamePad->width() * 0.01;
    boostStep = ui->groupBox_gamePad->width() * 0.2;

}

MainWindow::~MainWindow()
{
    delete ui;
    delete myServer;  // Clean up the server object
}

void MainWindow::startPtpSync()
{
    qint64 t1 = now_us();

    QString msg =
        QString("@@@SYNC@@@_%1").arg(t1);

    emit sendToRaw(msg);
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

    // Initialize steps
    int minutes =
        ui->spinBox_gameTime->value();

    boostSteps =
        minutes * 12;

    ui->label_steps->setText(
        QString::number(boostSteps)
    );

    ui->label_steps_client->setText(
            QString::number(boostSteps)
    );

    qDebug()
        << "Boost steps initialized:"
        << boostSteps;

    // Send server boost steps

    emit sendToRaw(
        QString(
            "@@@BOOST_STEPS@@@_%1"
        ).arg(boostSteps)
    );

    if(gameTimer)
    {
        gameTimer->stop();
        gameTimer->deleteLater();
    }

    gameTimer = new QTimer(this);

    connect(gameTimer, &QTimer::timeout, this, [=]()
    {
        remainingTime--;

        int currentSecond =
            remainingTime % 60;

        // NEW MINUTE

        if(currentSecond == 59)
        {
            generateSpecialSpawnSchedule();
        }

        // SPAWN

        if(specialSpawnTimes.contains(currentSecond))
        {
            spawnSpecialBall();
        }

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

    for(QLabel* s : specialBalls)
        s->deleteLater();

    specialBalls.clear();

    specialSpawnTimes.clear();

    bulletCount = 0;

    ui->label_bullets->setText("0");

    clientBulletCount = 0;

    ui->label_bullets_client
        ->setText("0");

    boostSteps = 0;
    ui->label_steps->setText("0");

    clientBoostSteps = 0;
    ui->label_steps_client->setText("0");
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

    // For Special ball clearance
    for(QLabel* s : specialBalls)
        s->deleteLater();

    specialBalls.clear();

    specialBallId = 0;

    specialSpawnTimes.clear();

    bulletCount = 0;

    ui->label_bullets->setText("0");

    clientBulletCount = 0;

    ui->label_bullets_client
        ->setText("0");

    boostSteps = 0;
    ui->label_steps->setText("0");

    clientBoostSteps = 0;
    ui->label_steps_client->setText("0");

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

    // laser sound
    laserSound = new QSoundEffect(this);
    laserSound->setSource(QUrl("qrc:/new/prefix1/mixkit-laser-cannon-shot-1678.wav"));
    laserSound->setVolume(0.9);
}

void MainWindow::spawnSpecialBall()
{
    QWidget *pad = ui->groupBox_gamePad;

    int id = specialBallId++;

    double rx = 0.05 + QRandomGenerator::global()->bounded(0.90);
    double ry = 0.05 + QRandomGenerator::global()->bounded(0.90);

    QLabel *ball = new QLabel(pad);

    ball->resize(16,16);

    ball->setStyleSheet(
        "background:black;"
        "border-radius:8px;"
    );

    int x = rx * (pad->width()  - ball->width());
    int y = ry * (pad->height() - ball->height());

    ball->move(x,y);
    ball->show();

    ball->setProperty("specialId", id);

    specialBalls.append(ball);

    // SEND TO CLIENT

    QString msg =
        QString("@@@SPECIAL@@@_%1_%2_%3")
        .arg(id)
        .arg(rx)
        .arg(ry);

    emit sendToRaw(msg);

    qDebug()
        << "Special ball spawned ID:"
        << id;
}

void MainWindow::generateSpecialSpawnSchedule()
{
    specialSpawnTimes.clear();

    int count;

    if(QRandomGenerator::global()->bounded(100) < 30)
        count = 4;
    else
        count = 3;

    const int MIN_GAP = 8;

    while(specialSpawnTimes.size() < count)
    {
        int sec =
            QRandomGenerator::global()
            ->bounded(60);

        bool valid = true;

        for(int existing : specialSpawnTimes)
        {
            if(qAbs(existing - sec) < MIN_GAP)
            {
                valid = false;
                break;
            }
        }

        if(valid)
            specialSpawnTimes.append(sec);
    }

    std::sort(
        specialSpawnTimes.begin(),
        specialSpawnTimes.end()
    );

    qDebug()
        << "Special spawn times:"
        << specialSpawnTimes;
}

void MainWindow::checkSpecialCollision()
{
    QRect playerRect =
        ui->label_gameBox->geometry();

    for(int i = 0; i < specialBalls.size(); i++)
    {
        QLabel *ball =
            specialBalls[i];

        if(playerRect.intersects(
                ball->geometry()))
        {
            int id =
                ball->property(
                    "specialId"
                ).toInt();

            qDebug()
                << "Special collected ID:"
                << id;

            // Remove from UI

            ball->deleteLater();

            specialBalls.removeAt(i);
            eatSound->play();

            // Increase bullets

            bulletCount += 4;

            ui->label_bullets->setText(
                QString::number(
                    bulletCount
                )
            );

            // Sync removal

            emit sendToRaw(
                QString(
                    "@@@SPECIAL_REMOVE@@@_%1"
                ).arg(id)
            );

            // Sync bullet count

            emit sendToRaw(
                QString(
                    "@@@BULLETS@@@_%1"
                ).arg(bulletCount)
            );

            break;
        }
    }
}

void MainWindow::removeSpecialById(int id)
{
    for(int i = 0;
        i < specialBalls.size();
        i++)
    {
        QLabel *ball =
            specialBalls[i];

        if(ball->property(
               "specialId"
           ).toInt() == id)
        {
            qDebug()
                << "Server removing special ID:"
                << id;

            ball->deleteLater();

            specialBalls.removeAt(i);

            return;
        }
    }
}

QRect MainWindow::drawLaser(
    int direction,
    bool isClient
)
{
    QWidget *pad =
        ui->groupBox_gamePad;

    QLabel *player;

    if(isClient)
        player =
            ui->label_gameBox_enemy;
    else
        player =
            ui->label_gameBox;

    QPoint pos =
        player->pos();

    int length =
        pad->width() * 0.4;

    int thickness = 6;

    QFrame *laser =
        new QFrame(pad);

    laser->setStyleSheet(
        "background:red;"
    );

    QRect rect;

    if(direction == 0) // RIGHT
    {
        rect =
            QRect(
                pos.x() + player->width(),
                pos.y() +
                player->height()/2,
                length,
                thickness
            );
    }
    else if(direction == 1) // LEFT
    {
        rect =
            QRect(
                pos.x() - length,
                pos.y() +
                player->height()/2,
                length,
                thickness
            );
    }
    else if(direction == 2) // UP
    {
        rect =
            QRect(
                pos.x() +
                player->width()/2,
                pos.y() - length,
                thickness,
                length
            );
    }
    else // DOWN
    {
        rect =
            QRect(
                pos.x() +
                player->width()/2,
                pos.y() +
                player->height(),
                thickness,
                length
            );
    }

    laser->setGeometry(rect);

    laser->show();

    laserSound->play();

    QTimer::singleShot(
        200,
        laser,
        &QFrame::deleteLater
    );

    return rect;
}

void MainWindow::checkLaserHit(
    QRect laserRect,
    bool firedByClient
)
{
    QRect targetRect;

    if(firedByClient)
        targetRect =
            ui->label_gameBox
                ->geometry();
    else
        targetRect =
            ui->label_gameBox_enemy
                ->geometry();

    if(laserRect.intersects(
            targetRect))
    {
        qDebug()
            << "LASER HIT DETECTED";

        if(firedByClient)
        {
            myScore -= 5;

            ui->label_myScore
                ->setText(
                    QString::number(
                        myScore
                    )
                );

            emit sendToRaw(
                QString(
                    "@@@SCORE_SERVER@@@_%1"
                ).arg(myScore)
            );
        }
        else
        {
            enemyScore -= 5;

            ui->label_enemyScore
                ->setText(
                    QString::number(
                        enemyScore
                    )
                );

            emit sendToRaw(
                QString(
                    "@@@SCORE_CLIENT@@@_%1"
                ).arg(enemyScore)
            );
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    QLabel *box = ui->label_gameBox;
    QWidget *pad = ui->groupBox_gamePad;

    int step;

    if(useBoost)
    {
        step = boostStep;   // 0.2

        useBoost = false;   // reset after one move
    }
    else
    {
        step = normalStep;  // 0.01
    }

    QPoint pos = box->pos();

    if(event->key() == Qt::Key_X)
    {
        if(bulletCount <= 0)
        {
            qDebug() << "Server has no bullets";
            return;
        }

        bulletCount--;

        ui->label_bullets->setText(
            QString::number(bulletCount)
        );

        qDebug()
            << "Server fired laser direction:"
            << lastDirection;

        // Draw locally
        QRect laserRect =
            drawLaser(
                lastDirection,
                false
            );

        checkLaserHit(
            laserRect,
            false
        );

        // 🔥 Inform client

        emit sendToRaw(
            QString(
                "@@@DRAW_LASER_SERVER@@@_%1_%2"
            )
            .arg(lastDirection)
            .arg(bulletCount)
        );

        return;
    }

   //Boost Steps Code
    if(event->key() == Qt::Key_Z)
    {
        if(boostSteps <= 0)
        {
            qDebug()
                << "No boost steps remaining";

            return;
        }

        boostSteps--;

        useBoost = true;

        ui->label_steps->setText(
            QString::number(boostSteps)
        );

        qDebug()
            << "Boost activated. Remaining:"
            << boostSteps;

        // 🔥 Sync to client (fair display)
        emit sendToRaw(
            QString(
                "@@@BOOST_STEPS@@@_%1"
            ).arg(boostSteps)
        );

        return;
    }

    switch (event->key())
    {
        case Qt::Key_Left:
            pos.rx() -= step;
            lastDirection = LEFT;
            break;

        case Qt::Key_Right:
            pos.rx() += step;
            lastDirection = RIGHT;
            break;

        case Qt::Key_Up:
            pos.ry() -= step;
            lastDirection = UP;
            break;

        case Qt::Key_Down:
            pos.ry() += step;
            lastDirection = DOWN;
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
    checkSpecialCollision();

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

    //PTP Code
    QRegularExpression reDelayReq(
        "@@@DELAY_REQ@@@_(\\d+)");

    QRegularExpressionMatch match =
        reDelayReq.match(rawData);

    if(match.hasMatch())
    {
        t3 = match.captured(1).toLongLong();

        t4 = now_us();

        QString resp =
            QString("@@@DELAY_RESP@@@_%1")
            .arg(t4);

        emit sendToRaw(resp);

        qDebug()
            << "DELAY_REQ received"
            << "t3:" << t3
            << "t4:" << t4;
    }

    // =========================
    // HANDLE CLIENT BULLETS
    // =========================

    QRegularExpression reClientBullets(
        "@@@CLIENT_BULLETS@@@_(\\d+)_(\\d+)"
    );

    QRegularExpressionMatch bulletMatch =
        reClientBullets.match(rawData);

    if(bulletMatch.hasMatch())
    {
        int count =
            bulletMatch.captured(1).toInt();

        int id =
            bulletMatch.captured(2).toInt();

        qDebug()
            << "Server received client bullets:"
            << count
            << "Remove special ID:"
            << id;

        // Update server's record of client bullets

        clientBulletCount = count;

        ui->label_bullets_client
            ->setText(
                QString::number(
                    clientBulletCount
                )
            );

        // Remove special ball on server

        removeSpecialById(id);
    }

    // =========================
    // HANDLE SHOOTING
    // =========================
    QRegularExpression reShoot(
        "@@@SHOOT@@@_(\\d+)_(\\d+)"
    );

    QRegularExpressionMatch shootMatch =
        reShoot.match(rawData);

    if(shootMatch.hasMatch())
    {
        int direction =
            shootMatch.captured(1).toInt();

        int bullets =
            shootMatch.captured(2).toInt();

        clientBulletCount = bullets;

        ui->label_bullets_client
            ->setText(
                QString::number(
                    clientBulletCount
                )
            );

        qDebug()
            << "Client fired laser direction:"
            << direction;

        // 🔥 DRAW LASER
        QRect laserRect =
            drawLaser(
                direction,
                true
            );

        checkLaserHit(
            laserRect,
            true
        );
    }

    // =========================
    // HANDLE CLIENT BOOST STEPS
    // =========================

    QRegularExpression reClientBoost(
        "@@@CLIENT_BOOST_STEPS@@@_(\\d+)"
    );

    QRegularExpressionMatch boostMatch =
        reClientBoost.match(rawData);

    if(boostMatch.hasMatch())
    {
        int steps =
            boostMatch.captured(1).toInt();

        clientBoostSteps = steps;

        ui->label_steps_client
            ->setText(
                QString::number(
                    clientBoostSteps
                )
            );

        qDebug()
            << "Server updated client boost steps:"
            << clientBoostSteps;
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

void MainWindow::on_actionPTP_Page_triggered()
{
    ui->stackedWidget->setCurrentWidget(ui->page_ptp);
}

void MainWindow::on_pushButton_back_ptp_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_main);
}

void MainWindow::on_pushButton_startPTP_clicked()
{
    if(!ptpTimer)
    {
        ptpTimer = new QTimer(this);

        connect(ptpTimer,
                &QTimer::timeout,
                this,
                &MainWindow::startPtpSync);
    }

    if(!ptpTimer->isActive())
    {
        ptpTimer->start(1000);

        qDebug() << "PTP Timer started";
    }
}

void MainWindow::on_pushButton_stopPTP_clicked()
{
    if(ptpTimer)
    {
        if(ptpTimer->isActive())
        {
            ptpTimer->stop();

            qDebug() << "PTP Timer stopped";
        }
    }
}

void MainWindow::on_actionHelp_triggered()
{
    QString helpText;

    helpText += "================ GAME RULES ================\n\n";

    helpText += "1. STARTING THE GAME\n";
    helpText += "   - Click the 'Generate Eatables' button on the Server to start the game.\n";
    helpText += "   - Use the Timer SpinBox to set the game duration (1 to 10 minutes).\n";
    helpText += "   - Once started, the timer will begin counting down.\n\n";

    helpText += "2. OBJECTIVES\n";
    helpText += "   - Collect orange eatables to increase your score.\n";
    helpText += "   - Avoid getting hit by the opponent's laser.\n";
    helpText += "   - The player with the higher score when the timer ends wins.\n\n";

    helpText += "3. SPECIAL ITEMS (BLACK BALLS)\n";
    helpText += "   - Special black balls appear periodically during gameplay.\n";
    helpText += "   - Collecting a black ball gives you 4 bullets.\n";
    helpText += "   - Bullets are required to shoot lasers.\n\n";

    helpText += "4. SHOOTING LASER\n";
    helpText += "   - Press arrow keys to choose direction.\n";
    helpText += "   - Press 'X' to shoot a laser in that direction.\n";
    helpText += "   - Each shot consumes 1 bullet.\n";
    helpText += "   - If a laser hits the opponent, their score decreases by 5 points.\n\n";

    helpText += "5. BOOST STEPS (SPEED BOOST)\n";
    helpText += "   - You receive 12 boost steps per minute of gameplay.\n";
    helpText += "   - Press 'Z' to use one boost step.\n";
    helpText += "   - The next movement will be faster than normal.\n";
    helpText += "   - Each boost can be used only once per key press.\n";
    helpText += "   - When boost steps reach zero, only normal movement is available.\n\n";

    helpText += "6. SCORE AND STATUS INDICATORS\n";
    helpText += "   - Blue Label   : Player Score\n";
    helpText += "   - Red Label    : Enemy Score\n";
    helpText += "   - Grey Label   : Player Bullets\n";
    helpText += "   - Yellow Label : Enemy Bullets\n";
    helpText += "   - Green Label  : Player Boost Steps\n";
    helpText += "   - Pink Label   : Enemy Boost Steps\n";
    helpText += "   - Timer Label  : Remaining Game Time\n\n";

    helpText += "7. GAME END\n";
    helpText += "   - The game automatically ends when the timer reaches 00:00.\n";
    helpText += "   - The player with the higher score is declared the winner.\n";
    helpText += "   - All scores, bullets, boost steps, and objects reset for the next game.\n\n";

    helpText += "============================================";

    // Create dialog
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Game Help");
    dialog->resize(500, 400);

    // Layout
    QVBoxLayout *layout = new QVBoxLayout(dialog);

    // Scrollable text area
    QTextEdit *textEdit = new QTextEdit(dialog);
    textEdit->setReadOnly(true);
    textEdit->setText(helpText);

    layout->addWidget(textEdit);

    dialog->setLayout(layout);

    dialog->exec();
}

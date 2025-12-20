#include "core/Smtp.h"
#include <QNetworkProxy>

Smtp::Smtp(const QString &user, const QString &pass, const QString &host, int port)
{
    this->socket = new QSslSocket(this);
    
    this->user = user;
    this->pass = pass;
    this->host = host;
    this->port = port;

    connect(socket, &QSslSocket::readyRead, this, &Smtp::readyRead);
    connect(socket, &QSslSocket::connected, this, &Smtp::connected);
    
    // Обработка ошибок (совместимость Qt5 и Qt6)
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(socket, &QSslSocket::errorOccurred, this, &Smtp::errorReceived);
#else
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(errorReceived(QAbstractSocket::SocketError)));
#endif

    connect(socket, &QSslSocket::stateChanged, this, &Smtp::stateChanged);
    connect(socket, &QSslSocket::disconnected, this, &Smtp::disconnected);
}

Smtp::~Smtp()
{
    // Не удаляем socket вручную, так как у него есть parent (this).
    // Это предотвращает двойное удаление и вылеты.
}

void Smtp::sendMail(const QString &to, const QString &subject, const QString &body)
{
    this->rcpt = to;
    this->subject = subject;
    this->body = body;
    this->state = Init;

    // Асинхронное подключение
    socket->connectToHostEncrypted(host, port);
    qDebug() << "SMTP: Connecting to" << host;
}

void Smtp::stateChanged(QAbstractSocket::SocketState socketState)
{
    qDebug() << "SMTP State:" << socketState;
}

void Smtp::errorReceived(QAbstractSocket::SocketError socketError)
{
    qDebug() << "SMTP Error:" << socketError << socket->errorString();
    emit status("Error: " + socket->errorString());
    
    emit finished(); 
}

void Smtp::disconnected()
{
    qDebug() << "SMTP: Disconnected";
    emit finished();
}

void Smtp::connected()
{
    qDebug() << "SMTP: Connected securely!";
}

void Smtp::readyRead()
{
    // Читаем ответ сервера построчно
    QString responseLine;
    while (socket->canReadLine()) {
        responseLine = QString(socket->readLine());
        
        // 1. Приветствие сервера (код 220)
        if (state == Init && responseLine.startsWith("220")) {
            socket->write("EHLO localhost\r\n");
            state = HandShake;
        }
        // 2. Ответ на EHLO (код 250) -> Начинаем авторизацию
        else if (state == HandShake && responseLine.startsWith("250")) {
            socket->write("AUTH LOGIN\r\n");
            state = Auth;
        }
        // 3. Сервер просит Логин (код 334) -> Шлем Base64 логин
        else if (state == Auth && responseLine.startsWith("334")) {
            socket->write(QByteArray().append(user.toUtf8()).toBase64() + "\r\n");
            state = User;
        }
        // 4. Сервер просит Пароль (код 334) -> Шлем Base64 пароль
        else if (state == User && responseLine.startsWith("334")) {
            socket->write(QByteArray().append(pass.toUtf8()).toBase64() + "\r\n");
            state = Pass;
        }
        // 5. Авторизация успешна (код 235) -> Шлем от кого
        else if (state == Pass && responseLine.startsWith("235")) {
            qDebug() << "SMTP: Auth success";
            socket->write(QString("MAIL FROM:<%1>\r\n").arg(user).toUtf8());
            state = Mail;
        }
        // 6. Ответ на MAIL FROM -> Шлем кому
        else if (state == Mail && responseLine.startsWith("250")) {
            socket->write(QString("RCPT TO:<%1>\r\n").arg(rcpt).toUtf8());
            state = Rcpt;
        }
        // 7. Ответ на RCPT TO -> Шлем команду DATA
        else if (state == Rcpt && responseLine.startsWith("250")) {
            socket->write("DATA\r\n");
            state = Data;
        }
        // 8. Сервер готов принять тело письма (код 354)
        else if (state == Data && responseLine.startsWith("354")) {
            // Формируем заголовки и тело
            QString emailData;
            emailData.append("To: " + rcpt + "\r\n");
            emailData.append("From: " + user + "\r\n");
            emailData.append("Subject: " + subject + "\r\n");
            
            // Важно для кириллицы
            emailData.append("Content-Type: text/plain; charset=UTF-8\r\n"); 
            
            emailData.append("\r\n"); // Пустая строка перед телом
            emailData.append(body);
            emailData.append("\r\n.\r\n"); // Точка в конце = конец письма

            socket->write(emailData.toUtf8());
            state = Body;
        }
        // 9. Письмо принято (код 250) -> Выходим
        else if (state == Body && responseLine.startsWith("250")) {
            qDebug() << "SMTP: Mail sent successfully";
            socket->write("QUIT\r\n");
            state = Quit;
            emit status("Message sent");
        }
        // 10. Сервер закрывает соединение (код 221)
        else if (state == Quit && responseLine.startsWith("221")) {
            socket->disconnectFromHost();
        }
    }
}
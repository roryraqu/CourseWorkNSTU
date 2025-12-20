#ifndef SMTP_H
#define SMTP_H

#include <QObject>
#include <QSslSocket>
#include <QString>
#include <QDebug>

class Smtp : public QObject
{
    Q_OBJECT

public:
    Smtp(const QString &user, const QString &pass, const QString &host, int port = 465);
    ~Smtp();

    void sendMail(const QString &to, const QString &subject, const QString &body);

signals:
    void status(const QString &);
    void finished();

private slots:
    void stateChanged(QAbstractSocket::SocketState socketState);
    void errorReceived(QAbstractSocket::SocketError socketError);
    void disconnected();
    void connected();
    void readyRead();

private:
    QSslSocket *socket;
    QString from;
    QString rcpt;
    QString subject;
    QString body;
    QString message;
    
    QString user;
    QString pass;
    QString host;
    int port;

    // Состояния протокола SMTP
    enum states {Tls, HandShake, Auth, User, Pass, Rcpt, Mail, Data, Init, Body, Quit, Close};
    int state;
};

#endif
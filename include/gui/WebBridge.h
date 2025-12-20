#ifndef WEBBRIDGE_H
#define WEBBRIDGE_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "models/User.h"

class WebBridge : public QObject {
    Q_OBJECT
public:
    explicit WebBridge(QObject *parent = nullptr);

public slots:
    void login(QString email, QString password);
    void verifyCode(QString email, QString code);
    void registerUser(QString fio, QString email, QString password, int regionId);
    void logout();
    void checkSession();
    
    void requestTickets();
    void createTicket(QString title, QString message);
    void openTicket(int ticketId);
    void sendTicketMessage(int ticketId, QString message);
    
    void requestDashboardData();
    void requestAllRegions();
    void requestOwnData();
    void updateUserField(int id, QString field, QString value, QString role);
    void closeTicket(int ticketId);

    void initiatePayment(double amount);
    void finalizePayment(double amount, bool success);

private slots:
    void onRefreshTimerTick();
    void onPaymentRegistered(QNetworkReply *reply, double amount);

    // Слоты ответов БД
    void onAuthFinished(bool success, User user, QString msg);
    void onUserByIdReceived(User user);
    void onRegistrationFinished(bool success, QString msg);
    void onTicketsReceived(QJsonArray tickets);
    void onTicketCreated(bool success);
    void onTicketMessagesReceived(int ticketId, QJsonArray msgs);
    void onMessageAdded(bool success);
    void onRegionsReceived(QJsonArray regions);
    void onUsersListReceived(QJsonArray users);
    void onOperationResult(bool success, QString msg);

signals:
    void loginSuccess(QString role, QString fio);
    void loginFailed(QString msg);
    void authCodeRequired(QString email);
    void registrationSuccess();
    void registrationFailed(QString msg);
    
    void ticketsDataReceived(QJsonArray tickets);
    void ticketMessagesReceived(int ticketId, QJsonArray messages);
    void operationResult(bool success, QString msg);
    
    void regionsDataReceived(QJsonArray regions);
    void dashboardDataReceived(QJsonArray users);
    void ownDataReceived(QString email, double balance, int regionId);
    
    void openStripeView(QString url, double amount);

private:
    QNetworkAccessManager *netManager;
    QTimer *refreshTimer;
    
    User currentUser;
    QMap<QString, QString> pendingCodes;
    QMap<QString, User> pendingUsers;
    
    int currentOpenTicketId = -1;
    int pollCounter = 0;
    
    void saveSession();
    void sendEmailCode(const QString &email, const QString &code);
    QString generateSignature(const QByteArray &data);
};

#endif
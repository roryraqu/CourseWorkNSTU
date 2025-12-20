#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "models/User.h"

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance() {
        static DatabaseManager instance;
        return instance;
    }

    void init(); 

    QString getCurrentToken() const { return currentJwt; }
    void setToken(const QString &token) { currentJwt = token; }

    void authenticate(const QString &email, const QString &password);
    void getUserById(int id); 
    void registerUser(const QString &fio, const QString &email, const QString &password, int regionId);
    
    // Тикеты
    void requestTickets(const User &user);
    void createTicket(int userId, int regionId, const QString &title, const QString &message);
    void getTicketMessages(int ticketId);
    void addTicketMessage(int ticketId, const QString &senderName, const QString &senderRole, const QString &message);
    
    // Управление
    void closeTicket(int ticketId);
    void markTicketAsRead(int ticketId);
    
    // Данные
    void getAllRegions();
    void requestUsersList(UserRole role, int regionId);
    void updateUserField(int userId, const QString &field, const QString &value);

    // Утилиты
    QString hashPassword(const QString &password);

signals:
    void authFinished(bool success, User user, QString message);
    void userByIdReceived(User user); 
    void registrationFinished(bool success, QString message);
    
    void ticketsReceived(QJsonArray tickets);
    void ticketCreated(bool success);
    void ticketMessagesReceived(int ticketId, QJsonArray messages);
    void messageAdded(bool success);
    
    void regionsReceived(QJsonArray regions);
    void usersListReceived(QJsonArray users);
    void operationResult(bool success, QString msg);

private:
    DatabaseManager();
    ~DatabaseManager();
    
    QNetworkAccessManager *net;
    QString currentJwt;

    QNetworkRequest createRequest(const QString &endpoint, bool useAuth = true);
    QString extractError(QNetworkReply *reply, const QByteArray &data);
};

#endif
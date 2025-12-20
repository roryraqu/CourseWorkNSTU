#include "gui/WebBridge.h"
#include "core/DatabaseManager.h"
#include "core/EnvManager.h"
#include "core/Smtp.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QFile>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QUrlQuery>

const QString SESSION_FILE = "session.json";

WebBridge::WebBridge(QObject *parent) : QObject(parent) {
    netManager = new QNetworkAccessManager(this);
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &WebBridge::onRefreshTimerTick);

    auto &db = DatabaseManager::instance();
    connect(&db, &DatabaseManager::authFinished, this, &WebBridge::onAuthFinished);
    connect(&db, &DatabaseManager::userByIdReceived, this, &WebBridge::onUserByIdReceived);
    connect(&db, &DatabaseManager::registrationFinished, this, &WebBridge::onRegistrationFinished);
    connect(&db, &DatabaseManager::ticketsReceived, this, &WebBridge::onTicketsReceived);
    connect(&db, &DatabaseManager::ticketCreated, this, &WebBridge::onTicketCreated);
    connect(&db, &DatabaseManager::ticketMessagesReceived, this, &WebBridge::onTicketMessagesReceived);
    connect(&db, &DatabaseManager::messageAdded, this, &WebBridge::onMessageAdded);
    connect(&db, &DatabaseManager::regionsReceived, this, &WebBridge::onRegionsReceived);
    connect(&db, &DatabaseManager::usersListReceived, this, &WebBridge::onUsersListReceived);
    connect(&db, &DatabaseManager::operationResult, this, &WebBridge::onOperationResult);
}

void WebBridge::login(QString email, QString password) {
    DatabaseManager::instance().authenticate(email, password);
}

void WebBridge::onAuthFinished(bool success, User user, QString msg) {
    if (success) {
        int codeInt = QRandomGenerator::global()->bounded(100000, 999999);
        QString code = QString::number(codeInt);
        
        pendingCodes[user.email] = code;
        pendingUsers[user.email] = user;
        
        sendEmailCode(user.email, code);
        emit authCodeRequired(user.email);
    } else {
        emit loginFailed(msg);
    }
}

void WebBridge::verifyCode(QString email, QString code) {
    if (pendingCodes.value(email) == code) {
        currentUser = pendingUsers[email];
        saveSession();
        refreshTimer->start(4000);
        
        QString role = (currentUser.role == UserRole::Admin) ? "admin" : 
                       (currentUser.role == UserRole::Manager) ? "manager" : "user";
        emit loginSuccess(role, currentUser.fio);
    } else {
        emit loginFailed("Неверный код");
    }
}

QString WebBridge::generateSignature(const QByteArray &data) {
    QString secret = EnvManager::instance().get("SESSION_SECRET");
    return QMessageAuthenticationCode::hash(data, secret.toUtf8(), QCryptographicHash::Sha256).toHex();
}

void WebBridge::saveSession() {
    QJsonObject payload;
    payload["user_id"] = currentUser.id;
    payload["expires"] = QDateTime::currentSecsSinceEpoch() + 3600 * 24; // 24 часа
    
    payload["access_token"] = DatabaseManager::instance().getCurrentToken();
    
    QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QJsonObject finalJson;
    finalJson["payload"] = QString(data);
    finalJson["signature"] = generateSignature(data);
    
    QFile file(SESSION_FILE);
    if(file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(finalJson).toJson());
        file.close();
        qDebug() << "Session saved with Token.";
    }
}

void WebBridge::checkSession() {
    QFile file(SESSION_FILE);
    if (!file.open(QIODevice::ReadOnly)) return;
    
    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    QString payloadStr = root["payload"].toString();
    
    // Проверка подписи
    if (generateSignature(payloadStr.toUtf8()) != root["signature"].toString()) {
        qDebug() << "Session signature mismatch!";
        file.remove(); return; 
    }
    
    QJsonObject session = QJsonDocument::fromJson(payloadStr.toUtf8()).object();
    if (QDateTime::currentSecsSinceEpoch() > session["expires"].toVariant().toLongLong()) {
        qDebug() << "Session expired!";
        file.remove(); return; 
    }

    QString token = session["access_token"].toString();
    if (!token.isEmpty()) {
        DatabaseManager::instance().setToken(token);
    }

    DatabaseManager::instance().getUserById(session["user_id"].toInt());
}

void WebBridge::onUserByIdReceived(User user) {
    if (user.id != -1) {
        currentUser = user;
        refreshTimer->start(4000);
        QString role = (currentUser.role == UserRole::Admin) ? "admin" : 
                       (currentUser.role == UserRole::Manager) ? "manager" : "user";
        emit loginSuccess(role, currentUser.fio);
    } else {
        QFile::remove(SESSION_FILE);
    }
}

void WebBridge::logout() {
    refreshTimer->stop();
    currentUser = User();
    DatabaseManager::instance().setToken(""); // Сбрасываем токен
    QFile::remove(SESSION_FILE);
}

void WebBridge::registerUser(QString fio, QString email, QString password, int regionId) {
    DatabaseManager::instance().registerUser(fio, email, password, regionId);
}
void WebBridge::onRegistrationFinished(bool success, QString msg) {
    if(success) emit registrationSuccess();
    else emit registrationFailed(msg);
}

void WebBridge::requestTickets() {
    DatabaseManager::instance().requestTickets(currentUser);
}
void WebBridge::onTicketsReceived(QJsonArray tickets) {
    emit ticketsDataReceived(tickets);
}

void WebBridge::createTicket(QString title, QString message) {
    DatabaseManager::instance().createTicket(currentUser.id, currentUser.regionId, title, message);
}
void WebBridge::onTicketCreated(bool success) {
    if(success) {
        emit operationResult(true, "Тикет создан");
        requestTickets();
    } else {
        emit operationResult(false, "Ошибка создания");
    }
}

void WebBridge::openTicket(int ticketId) {
    currentOpenTicketId = ticketId;
    pollCounter = 3; 
    onRefreshTimerTick(); // Форсируем обновление
    
    // Если открыл саппорт, помечаем прочитанным
    if(currentUser.role != UserRole::User) {
        DatabaseManager::instance().markTicketAsRead(ticketId);
    }
}

void WebBridge::sendTicketMessage(int ticketId, QString message) {
    QString role = (currentUser.role == UserRole::User) ? "user" : "manager";
    DatabaseManager::instance().addTicketMessage(ticketId, currentUser.fio, role, message);
}
void WebBridge::onMessageAdded(bool success) {
    if(success && currentOpenTicketId != -1) {
        DatabaseManager::instance().getTicketMessages(currentOpenTicketId);
    }
}
void WebBridge::onTicketMessagesReceived(int ticketId, QJsonArray msgs) {
    emit ticketMessagesReceived(ticketId, msgs);
}

void WebBridge::closeTicket(int ticketId) {
    if (currentUser.role != UserRole::User) {
        emit operationResult(false, "Только пользователь может закрыть тикет");
        return;
    }
    DatabaseManager::instance().closeTicket(ticketId);
    requestTickets();
}

void WebBridge::requestDashboardData() {
    DatabaseManager::instance().requestUsersList(currentUser.role, currentUser.regionId);
}
void WebBridge::onUsersListReceived(QJsonArray users) {
    emit dashboardDataReceived(users);
}
void WebBridge::requestAllRegions() {
    DatabaseManager::instance().getAllRegions();
}
void WebBridge::onRegionsReceived(QJsonArray regions) {
    emit regionsDataReceived(regions);
}
void WebBridge::updateUserField(int id, QString field, QString value, QString role) {
    Q_UNUSED(role);
    DatabaseManager::instance().updateUserField(id, field, value);
}
void WebBridge::onOperationResult(bool success, QString msg) {
    emit operationResult(success, msg);
}
void WebBridge::requestOwnData() {
    emit ownDataReceived(currentUser.email, currentUser.balance, currentUser.regionId);
}

void WebBridge::onRefreshTimerTick() {
    pollCounter++;
    if (currentUser.id <= 0) return;
    
    if (currentOpenTicketId != -1) {
        DatabaseManager::instance().getTicketMessages(currentOpenTicketId);
    }
    if (pollCounter % 5 == 0) {
        requestTickets();
    }
}

void WebBridge::sendEmailCode(const QString &email, const QString &code) {
    QString host = EnvManager::instance().get("SMTP_HOST");
    int port = EnvManager::instance().get("SMTP_PORT").toInt();
    QString user = EnvManager::instance().get("SMTP_USER");
    QString pass = EnvManager::instance().get("SMTP_PASS");

    Smtp* smtp = new Smtp(user, pass, host, port);
    connect(smtp, &Smtp::finished, smtp, &Smtp::deleteLater);
    smtp->sendMail(email, "Код входа", "Ваш код: " + code);
}

void WebBridge::initiatePayment(double amount) {
    if (amount < 60) { emit operationResult(false, "Минимум 60 руб"); return; }

    QString vpsUrl = EnvManager::instance().get("STRIPE_API_URL");
    if (vpsUrl.isEmpty()) {
        emit operationResult(false, "Ошибка конфига: нет STRIPE_API_URL");
        return;
    }
    
    QUrlQuery postData;
    postData.addQueryItem("amount_cents", QString::number(static_cast<int>(amount * 100)));
    postData.addQueryItem("description", "Topup");
    postData.addQueryItem("user_id", QString::number(currentUser.id));
    postData.addQueryItem("currency", "RUB"); 

    QNetworkRequest request((QUrl(vpsUrl)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    QNetworkReply *reply = netManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, amount](){ 
        onPaymentRegistered(reply, amount); 
    });
}

void WebBridge::onPaymentRegistered(QNetworkReply *reply, double amount) {
    if (reply->error() != QNetworkReply::NoError) {
        emit operationResult(false, "Ошибка сети: " + reply->errorString());
        reply->deleteLater(); return;
    }
    QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
    reply->deleteLater();
    
    if (json.contains("payment_url")) { 
        emit openStripeView(json["payment_url"].toString(), amount);
    } else {
        emit operationResult(false, "Ошибка создания платежа");
    }
}

void WebBridge::finalizePayment(double amount, bool success) {
    if (success) {
        double newBalance = currentUser.balance + amount;
        currentUser.balance = newBalance;
        DatabaseManager::instance().updateUserField(currentUser.id, "balance", QString::number(newBalance));
        requestOwnData();
        emit operationResult(true, "Баланс пополнен!");
    } else {
        emit operationResult(false, "Отмена оплаты");
    }
}
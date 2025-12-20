#include "core/DatabaseManager.h"
#include "core/EnvManager.h"
#include <QUrlQuery>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

DatabaseManager::DatabaseManager() {
    net = new QNetworkAccessManager(this);
}

DatabaseManager::~DatabaseManager() {
    // Сетевой менеджер удалится автоматически, так как у него установлен parent (this)
}

void DatabaseManager::init() {
    // Инициализация менеджера конфигураций (если еще не сделана)
    EnvManager::instance().load();
}

QString DatabaseManager::hashPassword(const QString &password) {
    // Используется стандартный SHA256 для совместимости [cite: 5]
    return QString(QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QNetworkRequest DatabaseManager::createRequest(const QString &endpoint, bool useAuth) {
    // Получаем базовый URL и Ключ из .env через EnvManager [cite: 316, 319]
    QString baseUrl = EnvManager::instance().get("SUPABASE_URL");
    QString anonKey = EnvManager::instance().get("SUPABASE_KEY");

    QNetworkRequest req(QUrl(baseUrl + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("apikey", anonKey.toUtf8());
    
    // Если есть JWT токен, используем его, иначе — системный ключ [cite: 316]
    if (useAuth && !currentJwt.isEmpty()) {
        req.setRawHeader("Authorization", "Bearer " + currentJwt.toUtf8());
    } else {
        req.setRawHeader("Authorization", "Bearer " + anonKey.toUtf8());
    }
    return req;
}

void DatabaseManager::authenticate(const QString &email, const QString &password) {
    QNetworkRequest req = createRequest("/auth/v1/token?grant_type=password", false);
    QJsonObject json;
    json["email"] = email;
    json["password"] = password;

    QNetworkReply *reply = net->post(req, QJsonDocument(json).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply, email](){
        QByteArray data = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            emit authFinished(false, User(), extractError(reply, data));
            reply->deleteLater();
            return;
        }

        QJsonObject resp = QJsonDocument::fromJson(data).object();
        currentJwt = resp["access_token"].toString(); 
        QString authId = resp["user"].toObject()["id"].toString();
        
        // Получаем метаданные пользователя (если профиля нет, возьмем данные оттуда)
        QJsonObject userMeta = resp["user"].toObject()["user_metadata"].toObject();
        QString metaFio = userMeta["fio"].toString();
        int metaRegion = userMeta["region_id"].toInt();
        
        reply->deleteLater();

        // 1. Пытаемся получить профиль
        QString endpoint = EnvManager::instance().getQuery("GET_PROFILE").arg(authId);
        QNetworkReply *profRep = net->get(createRequest(endpoint));

        connect(profRep, &QNetworkReply::finished, this, [this, profRep, authId, email, metaFio, metaRegion](){
            QByteArray profData = profRep->readAll();
            QJsonArray arr = QJsonDocument::fromJson(profData).array();
            profRep->deleteLater();

            if (arr.isEmpty()) {
                // === ИСПРАВЛЕНИЕ: Если профиля нет, создаем его вручную ===
                qDebug() << "Profile missing for" << email << "- creating manually...";
                
                QJsonObject newProfile;
                newProfile["auth_id"] = authId;
                newProfile["email"] = email;
                newProfile["fio"] = metaFio.isEmpty() ? "User" : metaFio;
                newProfile["role"] = "user";
                newProfile["region_id"] = (metaRegion == 0) ? 1 : metaRegion; // Дефолтный регион
                newProfile["balance"] = 0.0;

                QNetworkRequest createReq = createRequest("/rest/v1/ts_profiles");
                createReq.setRawHeader("Prefer", "return=representation");
                
                QNetworkReply *createReply = net->post(createReq, QJsonDocument(newProfile).toJson());
                
                connect(createReply, &QNetworkReply::finished, this, [this, createReply](){
                    if (createReply->error() == QNetworkReply::NoError) {
                        // Успешно создали, рекурсивно вызываем authenticate снова, 
                        // но чтобы не зациклить, просто эмулируем успех
                        QJsonObject p = QJsonDocument::fromJson(createReply->readAll()).array().first().toObject();
                        User user;
                        user.id = p["id"].toInt();
                        user.fio = p["fio"].toString();
                        user.email = p["email"].toString();
                        user.regionId = p["region_id"].toInt();
                        user.role = UserRole::User;
                        
                        emit authFinished(true, user, "Профиль создан и вход выполнен");
                    } else {
                        emit authFinished(false, User(), "Ошибка создания профиля: " + createReply->errorString());
                    }
                    createReply->deleteLater();
                });
                return;
            }
            
            // Если профиль есть (стандартный путь)
            QJsonObject p = arr.first().toObject();
            User user;
            user.id = p["id"].toInt();
            user.fio = p["fio"].toString();
            user.email = p["email"].toString();
            user.regionId = p["region_id"].toInt();
            user.balance = p["balance"].toDouble();
            
            QString r = p["role"].toString();
            if(r == "admin") user.role = UserRole::Admin;
            else if(r == "manager") user.role = UserRole::Manager;
            else user.role = UserRole::User;

            emit authFinished(true, user, "Успешно");
        });
    });
}

void DatabaseManager::getUserById(int id) {
    QString endpoint = EnvManager::instance().getQuery("GET_USER_BY_ID").arg(id);
    QNetworkRequest req = createRequest(endpoint);
    QNetworkReply *reply = net->get(req);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
        reply->deleteLater();

        if(!arr.isEmpty()) {
            QJsonObject p = arr.first().toObject();
            User user;
            user.id = p["id"].toInt();
            user.fio = p["fio"].toString();
            user.email = p["email"].toString();
            user.regionId = p["region_id"].toInt();
            user.balance = p["balance"].toDouble();
            
            QString r = p["role"].toString();
            user.role = (r == "admin") ? UserRole::Admin : (r == "manager" ? UserRole::Manager : UserRole::User);
            
            emit userByIdReceived(user);
        } else {
            User empty; empty.id = -1;
            emit userByIdReceived(empty);
        }
    });
}

void DatabaseManager::registerUser(const QString &fio, const QString &email, const QString &password, int regionId) {
    QNetworkRequest req = createRequest("/auth/v1/signup", false);
    
    QJsonObject meta;
    meta["app"] = "transport"; 
    meta["fio"] = fio;
    meta["role"] = "user";
    meta["region_id"] = regionId;

    QJsonObject json;
    json["email"] = email;
    json["password"] = password;
    json["data"] = meta;

    QNetworkReply *reply = net->post(req, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        QByteArray data = reply->readAll();
        
        if (reply->error() != QNetworkReply::NoError) {
            emit registrationFinished(false, extractError(reply, data));
        } else {
            emit registrationFinished(true, "Регистрация успешна! Проверьте почту.");
        }
        reply->deleteLater();
    });
}

void DatabaseManager::requestTickets(const User &user) {
    Q_UNUSED(user); 
    QString q = EnvManager::instance().getQuery("GET_TICKETS");
    QNetworkReply *reply = net->get(createRequest(q));
    
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        QJsonArray raw = QJsonDocument::fromJson(reply->readAll()).array();
        reply->deleteLater();

        QJsonArray result;
        for(const auto &item : raw) {
            QJsonObject t = item.toObject();
            QJsonObject res;
            res["id"] = t["id"];
            res["title"] = t["title"];
            res["status"] = t["status"];
            res["created_at"] = t["created_at"];
            res["unread"] = t["has_unread_support"];
            if(t["ts_profiles"].isObject()) res["author"] = t["ts_profiles"].toObject()["fio"];
            result.append(res);
        }
        emit ticketsReceived(result);
    });
}

void DatabaseManager::createTicket(int userId, int regionId, const QString &title, const QString &message) {
    QNetworkRequest req = createRequest(EnvManager::instance().getQuery("CREATE_TICKET"));
    req.setRawHeader("Prefer", "return=representation"); 
    
    QJsonObject json;
    json["user_id"] = userId;
    json["region_id"] = regionId;
    json["title"] = title;

    QNetworkReply *reply = net->post(req, QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, message](){
        if(reply->error() != QNetworkReply::NoError) {
            emit ticketCreated(false);
            reply->deleteLater();
            return;
        }
        
        QJsonArray arr = QJsonDocument::fromJson(reply->readAll()).array();
        reply->deleteLater();
        if (arr.isEmpty()) return;

        int ticketId = arr.first().toObject()["id"].toInt();
        addTicketMessage(ticketId, "User", "user", message);
        emit ticketCreated(true);
    });
}

void DatabaseManager::getTicketMessages(int ticketId) {
    QString q = EnvManager::instance().getQuery("GET_MESSAGES").arg(ticketId);
    QNetworkReply *reply = net->get(createRequest(q));
    
    connect(reply, &QNetworkReply::finished, this, [this, reply, ticketId](){
        QJsonArray raw = QJsonDocument::fromJson(reply->readAll()).array();
        reply->deleteLater();

        QJsonArray result;
        for(const auto &item : raw) {
            QJsonObject m = item.toObject();
            QJsonObject res;
            res["sender"] = m["sender_name"];
            res["role"] = m["sender_role"];
            res["text"] = m["message"];
            res["time"] = QDateTime::fromString(m["created_at"].toString(), Qt::ISODate).toString("dd.MM HH:mm");
            result.append(res);
        }
        emit ticketMessagesReceived(ticketId, result);
    });
}

void DatabaseManager::addTicketMessage(int ticketId, const QString &senderName, const QString &senderRole, const QString &message) {
    QJsonObject json;
    json["ticket_id"] = ticketId;
    json["sender_name"] = senderName;
    json["sender_role"] = senderRole;
    json["message"] = message;
    
    QNetworkReply *reply = net->post(createRequest(EnvManager::instance().getQuery("ADD_MESSAGE")), QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, senderRole, ticketId](){
        bool success = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        emit messageAdded(success);

        if (success && senderRole == "user") {
            QJsonObject upd;
            upd["has_unread_support"] = true;
            upd["status"] = "open";
            QNetworkRequest r = createRequest(EnvManager::instance().getQuery("UPDATE_TICKET").arg(ticketId));
            net->sendCustomRequest(r, "PATCH", QJsonDocument(upd).toJson());
        }
    });
}

void DatabaseManager::getAllRegions() {
    QNetworkReply *reply = net->get(createRequest(EnvManager::instance().getQuery("GET_REGIONS")));
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        emit regionsReceived(QJsonDocument::fromJson(reply->readAll()).array());
        reply->deleteLater();
    });
}

void DatabaseManager::requestUsersList(UserRole role, int regionId) {
    QString q = (role == UserRole::Manager) ? 
                EnvManager::instance().getQuery("GET_USERS_MANAGER").arg(regionId) : 
                EnvManager::instance().getQuery("GET_USERS_ADMIN");
    
    QNetworkReply *reply = net->get(createRequest(q));
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        emit usersListReceived(QJsonDocument::fromJson(reply->readAll()).array());
        reply->deleteLater();
    });
}

void DatabaseManager::updateUserField(int userId, const QString &field, const QString &value) {
    QNetworkRequest req = createRequest(EnvManager::instance().getQuery("UPDATE_PROFILE").arg(userId));
    QJsonObject json;
    if(field == "balance") json[field] = value.toDouble();
    else if(field == "region_id") json[field] = value.toInt();
    else json[field] = value;

    QNetworkReply *reply = net->sendCustomRequest(req, "PATCH", QJsonDocument(json).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply](){
        QByteArray data = reply->readAll();
        
        if (reply->error() == QNetworkReply::NoError) {
            emit operationResult(true, "Обновлено");
        } else {
            emit operationResult(false, extractError(reply, data));
        }
        reply->deleteLater();
    });
}

void DatabaseManager::closeTicket(int ticketId) {
    QJsonObject json;
    json["status"] = "closed";
    QNetworkRequest req = createRequest(EnvManager::instance().getQuery("UPDATE_TICKET").arg(ticketId));
    net->sendCustomRequest(req, "PATCH", QJsonDocument(json).toJson());
}

void DatabaseManager::markTicketAsRead(int ticketId) {
    QJsonObject json;
    json["has_unread_support"] = false;
    QNetworkRequest req = createRequest(EnvManager::instance().getQuery("UPDATE_TICKET").arg(ticketId));
    net->sendCustomRequest(req, "PATCH", QJsonDocument(json).toJson());
}

QString DatabaseManager::extractError(QNetworkReply *reply, const QByteArray &data) {
    QString errorMsg = "Неизвестная ошибка";
    
    // 1. Пытаемся распарсить JSON от Supabase
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isNull() && doc.isObject()) {
        QJsonObject obj = doc.object();
        
        // Вариант 1: Ошибка Auth (GoTrue)
        // Пример: { "error": "invalid_grant", "error_description": "Invalid login credentials" }
        if (obj.contains("error_description")) {
            errorMsg = obj["error_description"].toString();
        }
        // Вариант 2: Ошибка API (PostgREST)
        // Пример: { "message": "duplicate key value violates unique constraint" }
        else if (obj.contains("message")) {
            errorMsg = obj["message"].toString();
        }
        // Вариант 3: Просто msg
        else if (obj.contains("msg")) {
            errorMsg = obj["msg"].toString();
        }
    } else {
        // Если JSON не пришел, берем стандартную ошибку сети (например "Connection refused")
        errorMsg = reply->errorString();
    }

    // Здесь мы заменяем технические английские тексты на понятные русские
    if (errorMsg.contains("Invalid login credentials", Qt::CaseInsensitive)) 
        return "Неверный логин или пароль";
        
    if (errorMsg.contains("User already registered", Qt::CaseInsensitive)) 
        return "Пользователь с таким Email уже существует";
        
    if (errorMsg.contains("Password should be at least", Qt::CaseInsensitive)) 
        return "Пароль слишком короткий (минимум 6 символов)";
        
    if (errorMsg.contains("duplicate key", Qt::CaseInsensitive)) 
        return "Такая запись уже существует";

    if (errorMsg.contains("violates check constraint", Qt::CaseInsensitive)) 
        return "Недопустимые данные (проверьте поля)";

    if (errorMsg.contains("Email not confirmed", Qt::CaseInsensitive)) 
        return "Почта не подтверждена. Проверьте входящие.";

    // Если перевода нет, возвращаем как есть (или добавляем префикс)
    return "Ошибка сервера: " + errorMsg;
}
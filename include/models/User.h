#ifndef USER_H
#define USER_H

#include <QString>

enum class UserRole {
    Admin,
    Manager,
    User,
    Unknown
};

struct User {
    int id = -1;
    QString fio;
    QString email;
    UserRole role = UserRole::Unknown;
    double balance = 0.0;
    int regionId = 0;
};

#endif
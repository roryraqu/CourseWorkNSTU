#ifndef ENVMANAGER_H
#define ENVMANAGER_H

#include <QString>
#include <QMap>

class EnvManager {
public:
    static EnvManager& instance() {
        static EnvManager instance;
        return instance;
    }

    void load();
    QString get(const QString &key, const QString &defaultValue = "") const;
    QString getQuery(const QString &key) const;

private:
    EnvManager() {}
    void parseFile(const QString& path, QMap<QString, QString>& map);
    QMap<QString, QString> variables;
    QMap<QString, QString> queries;
};

#endif
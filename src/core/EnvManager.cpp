#include "core/EnvManager.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>

void EnvManager::parseFile(const QString& path, QMap<QString, QString>& map) {
    QFile file(path);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "CRITICAL ERROR: Could not load embedded config:" << path;
        return;
    }

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8); // Принудительно UTF-8

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        
        // Пропускаем комментарии
        if (line.isEmpty() || line.startsWith("#") || line.startsWith("//")) continue;

        int splitIndex = line.indexOf('=');
        if (splitIndex != -1) {
            QString key = line.left(splitIndex).trimmed();
            QString value = line.mid(splitIndex + 1).trimmed();
            
            // Убираем кавычки, если есть
            if ((value.startsWith('"') && value.endsWith('"')) || 
                (value.startsWith('\'') && value.endsWith('\''))) {
                value = value.mid(1, value.length() - 2);
            }

            map[key] = value;
        }
    }
    file.close();
    qDebug() << "Loaded embedded config:" << path;
}

void EnvManager::load() {
    parseFile(":/.env", variables);

    parseFile(":/queries.conf", queries);
}

QString EnvManager::get(const QString &key, const QString &defaultValue) const {
    return variables.value(key, defaultValue);
}

QString EnvManager::getQuery(const QString &key) const {
    return queries.value(key, "");
}
#include "gui/MainWindow.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <vector>
#include <string>
#include "core/DatabaseManager.h"

int main(int argc, char *argv[]) {
    std::vector<std::string> newArgs;
    for(int i = 0; i < argc; i++) newArgs.push_back(argv[i]);
    newArgs.push_back("--proxy-server=socks5://37.1.222.45:1080");
    newArgs.push_back("--host-resolver-rules=MAP * ~NOTFOUND , EXCLUDE 127.0.0.1");
    newArgs.push_back("--ignore-certificate-errors");
    newArgs.push_back("--no-sandbox");
    newArgs.push_back("--disable-gpu");
    
    int newArgc = newArgs.size();
    char** newArgv = new char*[newArgc + 1];
    for(int i = 0; i < newArgc; i++) newArgv[i] = const_cast<char*>(newArgs[i].c_str());
    newArgv[newArgc] = nullptr;

    QApplication a(newArgc, newArgv);

    DatabaseManager::instance().init();

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "TransportSecureSystem_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    MainWindow w;
    w.show();
    
    int ret = a.exec();
    delete[] newArgv;
    return ret;
}
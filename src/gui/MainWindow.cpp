#include "gui/MainWindow.h"
#include <QDebug>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebChannel>
#include <QWebEngineCertificateError>
#include <QWebEnginePage>
#include <QWebEngineProfile>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    resize(1200, 800);
    setWindowTitle("Transport System");

    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    appView = new QWebEngineView(this);
    QWebChannel* channel = new QWebChannel(this);
    bridge = new WebBridge(this);
    channel->registerObject("backend", bridge);
    appView->page()->setWebChannel(channel);
    connect(bridge, &WebBridge::openStripeView, this, &MainWindow::showStripePage);
    appView->setUrl(QUrl("qrc:///web/index.html"));

    // Stripe
    stripeView = new QWebEngineView(this);

    // Игнорируем ошибки SSL
    connect(stripeView->page(), &QWebEnginePage::certificateError, this,
            [](const QWebEngineCertificateError& error) { return true; });

    // Лог
    connect(stripeView, &QWebEngineView::loadFinished, [](bool success) {
        if (success)
            qDebug() << "Stripe: Успешно загружено";
        else
            qDebug() << "Stripe: Ошибка загрузки (возможно, прокси медленный)";
    });

    connect(stripeView, &QWebEngineView::urlChanged, this, &MainWindow::onStripeUrlChanged);

    stackedWidget->addWidget(appView);
    stackedWidget->addWidget(stripeView);
    stackedWidget->setCurrentIndex(0);
}

MainWindow::~MainWindow() {}

void MainWindow::showStripePage(QString url, double amount) {
    currentPaymentAmount = amount;

    stripeView->page()->profile()->clearHttpCache();

    QString loadingHtml = R"(
        <html>
        <body style="display:flex;justify-content:center;align-items:center;height:100vh;font-family:sans-serif;background:#f6f9fc;">
            <div style="text-align:center;">
                <h2 style="color:#6772e5;">Подключение к банку...</h2>
                <p style="color:#8898aa;">Пожалуйста, подождите, открываем Stripe.</p>
                <div style="margin-top:20px;border: 4px solid #f3f3f3;border-top: 4px solid #6772e5;border-radius: 50%;width: 30px;height: 30px;animation: spin 1s linear infinite;margin:0 auto;"></div>
                <style>@keyframes spin {0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); }}</style>
            </div>
        </body>
        </html>
    )";
    stripeView->setHtml(loadingHtml);

    stackedWidget->setCurrentIndex(1);

    QTimer::singleShot(500, this, [this, url]() {
        qDebug() << "Stripe: Старт загрузки URL:" << url;
        stripeView->setUrl(QUrl(url));
    });
}

void MainWindow::onStripeUrlChanged(const QUrl& url) {
    QString urlStr = url.toString();

    qDebug() << "Stripe URL:" << urlStr;

    if (urlStr.contains("/success")) {
        stackedWidget->setCurrentIndex(0);
        stripeView->setUrl(QUrl("about:blank"));
        bridge->finalizePayment(currentPaymentAmount, true);
    } else if (urlStr.contains("/cancel")) {
        stackedWidget->setCurrentIndex(0);
        stripeView->setUrl(QUrl("about:blank"));
        bridge->finalizePayment(currentPaymentAmount, false);
    }
}
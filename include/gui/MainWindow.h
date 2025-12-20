#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWebEngineView>
#include <QStackedWidget> 
#include "gui/WebBridge.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void showStripePage(QString url, double amount);
    void onStripeUrlChanged(const QUrl &url);

private:
    QStackedWidget *stackedWidget;
    
    QWebEngineView *appView;
    QWebEngineView *stripeView;
    
    WebBridge *bridge;
    double currentPaymentAmount;
};

#endif
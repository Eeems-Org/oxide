#ifndef APPSVIEW_H
#define APPSVIEW_H

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QObject>
#include <QQuickItem>

struct OptionItem {
    std::string name;
    std::string desc;
    std::string call;
    std::string term;
    std::string imgFile;
    QObject* object;
    QObject* handler;
};

class AppsView {
public:
    explicit AppsView(QQmlApplicationEngine* engine, QGuiApplication* app, QQuickItem* appsView);
    void load();
    bool read_directory(const std::string name, std::vector<std::string>& files);
private:
    void error(std::string text);
    void createOption(OptionItem &option, size_t index);
    std::vector<OptionItem> appsList;
    QQmlApplicationEngine* engine;
    QGuiApplication* app;
    QQuickItem* appsView;
};

#endif // APPSVIEW_H

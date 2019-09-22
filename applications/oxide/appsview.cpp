#include "appsview.h"
#include "handler.h"
#include <dirent.h>
#include <string>
#include <vector>
#include <iostream>
#include <QtQuick>
#include <algorithm>

const std::string configDir = "/opt/etc/draft";

AppsView::AppsView(QQmlApplicationEngine* engine, QGuiApplication* app, QQuickItem* appsView)
: engine(engine),
  app(app),
  appsView(appsView)
{
    load();
}
void AppsView::load(){
    std::vector<std::string> filenames;

    // If the config directory doesn't exist,
    // then print an error and stop.
    if(!AppsView::read_directory(configDir, filenames)) {
        AppsView::error("Failed to read directory - it does not exist.");
        return;
    }

    std::sort(filenames.begin(), filenames.end());

    for(std::string f : filenames) {
        std::cout << "Parsing file " << f << std::endl;
        QFile file((configDir + "/" + f).c_str());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            AppsView::error("Couldn't find the file " + f + ".");
            break;
        }
        QTextStream in(&file);
        OptionItem opt;
        while (!in.atEnd()) {
            std::string line = in.readLine().toStdString();
            if(line.length() > 0) {
                size_t sep = line.find("=");
                if(sep != line.npos) {
                    std::string lhs = line.substr(0,sep);
                    std::string rhs = line.substr(sep+1);

                    if(lhs == "name"){
                        opt.name = rhs;
                    }else if(lhs == "desc"){
                        opt.desc = rhs;
                    }else if(lhs == "imgFile"){
                        opt.imgFile = rhs;

                    }else if(lhs == "call"){
                        opt.call = rhs;
                    }else if(lhs == "term"){
                        opt.term = rhs;
                    }else{
                        std::cout
                            << "Ignoring unknown parameter \"" << line
                            << "\" in file \"" << f << "\"" << std::endl;
                    }
                }
            }else{
                std::cout
                    << "Ignoring malformed line \""
                    << line << "\" in file \"" << f
                    << "\"" << std::endl;
            }
        }

        if(opt.call == "" || opt.term == ""){
            continue;
        }
        createOption(opt, appsList.size());
        appsList.push_back(opt);
        std::cout << "Finished parsing files" << std::endl;
    }
}
void AppsView::createOption(OptionItem &option, size_t index) {

    QQuickView* opt = new QQuickView();
    opt->setSource(QUrl("qrc:/MenuItem.qml"));
    opt->show();

    QQuickItem* root = opt->rootObject();
    root->setProperty("itemNumber", QVariant(index));
    root->setProperty("t_name", QVariant(option.name.c_str()));
    root->setProperty("t_desc", QVariant(option.desc.c_str()));
    std::string iconPath("file://" +configDir+"/icons/"+option.imgFile+".png");
    root->setProperty("t_imgFile",QVariant(iconPath.c_str()));

    QObject* mouseArea = root->children().at(0);
    Handler* handler = new Handler(this, option.call, option.term, app, engine, mouseArea);
    root->children().at(0)->installEventFilter(handler);
    root->setParentItem(appsView);

    option.object = opt;
    option.handler = handler;
}

void AppsView::error(std::string text) {
    std::cout << "!! Error: " << text << std::endl;
}


// Stolen shamelessly from Martin Broadhurst
bool AppsView::read_directory(
    const std::string name, std::vector<std::string>& filenames)
{
    DIR* dirp = opendir(name.c_str());
    if(dirp == nullptr) {
        return false;
    }
    struct dirent * dp;
    while((dp = readdir(dirp)) != NULL){
        std::string dn = dp->d_name;

        if(dn == "." || dn == ".."|| dn == "icons") continue;

        filenames.push_back(dn);
    }
    closedir(dirp);
    return true;
}

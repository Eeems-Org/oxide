#include "cat.h"

#include <QDir>
#include <QUrl>

int Cat::arguments(QCommandLineParser& parser){
    parser.addPositionalArgument("location", "Locations to concatenate", "LOCATION...");
    return EXIT_SUCCESS;
}

#define GIO_ERROR(url, path, error) \
    qDebug() \
        << "gio:" \
        << url.toString().toStdString().c_str() \
        << ": Error opening file " \
        << QFileInfo(path).absoluteFilePath().toStdString().c_str() \
        << ": " \
        << error; \

int Cat::command(const QStringList& args){
    for(const auto& path : args){
        auto url = QUrl::fromUserInput(path, QDir::currentPath(), QUrl::AssumeLocalFile);
        if(url.scheme().isEmpty()){
            url.setScheme("file");
        }
        if(!url.isLocalFile()){
            GIO_ERROR(url, path, "No such file or directory");
            continue;
        }
        QFile file(path);
        if(!file.open(QFile::ReadOnly)){
            GIO_ERROR(url, path, "Permission denied");
            continue;
        }
        while(!file.atEnd()){
            qStdOut() << file.read(1);
        }
        file.close();
    }
    return EXIT_SUCCESS;
}

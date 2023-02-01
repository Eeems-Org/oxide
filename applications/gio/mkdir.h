#pragma once

#include "common.h"

#include <QDir>
#include <QUrl>
#include <QCommandLineOption>

// [OPTION...] SOURCE... DESTINATION
class MkdirCommand : ICommand{
    O_COMMAND(
        MkdirCommand, "mkdir",
        "Creates directories."
    )
    int arguments() override{
        parser->addOption(parentOption);
        parser->addPositionalArgument("LOCATION", "The directories to create", "LOCATION...");
        return EXIT_SUCCESS;
    }
    int command(const QStringList& args) override{
        auto mkdirArgs = QStringList();
        if(parser->isSet(parentOption)){
            mkdirArgs.append("-p");
        }
        auto* p = new QProcess();
        p->setInputChannelMode(QProcess::ForwardedInputChannel);
        p->setProcessChannelMode(QProcess::ForwardedChannels);
        bool failed = false;
        for(const auto& path : args){
            auto url = urlFromPath(path);
            if(!url.isLocalFile()){
                GIO_ERROR(url, path, "Error while parsing path", "Path is not a local file");
                failed = true;
                continue;
            }
            QFileInfo info(path);
            if(info.exists()){
                GIO_ERROR(url, path, "Error creating directory", "File exists");
                failed = true;
                continue;
            }

            p->start("mkdir", QStringList() << mkdirArgs << path);
            qApp->processEvents();
            p->waitForFinished();
            if(p->exitCode()){
                failed = true;
            }
        }
        p->deleteLater();
        return failed ? EXIT_FAILURE : EXIT_SUCCESS;
    }

private:
    QCommandLineOption parentOption = QCommandLineOption(
        {"p", "parent"},
        "Create parent directories when necessary."
    );
};

#pragma once

#include "common.h"

#include <QDir>
#include <QUrl>
#include <QCommandLineOption>

// [OPTION...] SOURCE... DESTINATION
class RemoveCommand : ICommand{
    O_COMMAND(
        RemoveCommand, "remove",
        "Deletes each given file."
    )
    int arguments() override{
        parser->addOption(forceOption);
        parser->addPositionalArgument("LOCATION", "The locations to remove", "LOCATION...");
        return EXIT_SUCCESS;
    }
    int command(const QStringList& args) override{
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
            if(!info.exists()){
                if(!parser->isSet(forceOption)){
                    GIO_ERROR(url, path, "Error removing file", "No such file or directory");
                    failed = true;
                }
                continue;
            }
            QStringList rmArgs;
            if(info.isDir()){
                rmArgs.append("rmdir");
            }else{
                rmArgs.append("rm");
                rmArgs.append("-f");
            }
            p->start("busybox", QStringList() << rmArgs << path);
            qApp->processEvents();
            p->waitForFinished();
            failed = failed || p->exitCode();
        }
        p->deleteLater();
        return failed ? EXIT_FAILURE : EXIT_SUCCESS;
    }
private:
    QCommandLineOption forceOption = QCommandLineOption(
        {"f", "force"},
        "Ignore non-existent and non-deletable files."
    );
};

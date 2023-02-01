#pragma once

#include "common.h"

#include <QDir>
#include <QUrl>
#include <QCommandLineOption>

// [OPTION...] SOURCE... DESTINATION
class CopyCommand : ICommand{
    O_COMMAND(
        CopyCommand, "copy",
        "Copies one or more files from SOURCE to DESTINATION. If more than one source is specified, the destination must be a directory. "
    )
    int arguments() override{
        parser->addOption(noTargetDirectoryOption);
        parser->addOption(progressOption);
        parser->addOption(interactiveOption);
        parser->addOption(preserveOption);
        parser->addOption(backupOption);
        parser->addOption(noDereferenceOption);
        parser->addOption(defaultPermissionsOption);
        parser->addPositionalArgument("SOURCE", "The source file(s) to copy to the destination", "SOURCE...");
        parser->addPositionalArgument("DESTINATION", "The destination to copy to");
        return EXIT_SUCCESS;
    }
    int command(const QStringList& args) override{
        if(args.length() < 2){
            parser->showHelp(EXIT_FAILURE);
        }
        auto toPath = args.last();
        auto toUrl = urlFromPath(toPath);
        if(!toUrl.isLocalFile()){
            GIO_ERROR(toUrl, toPath, "Error while parsing path", "Path is not a local file");
            return EXIT_FAILURE;
        }
        if(!QFile::exists(toPath)){
            GIO_ERROR(toUrl, toPath, "Error when getting information for file", "No such file or directory");
            return EXIT_FAILURE;
        }
        QFileInfo toInfo(toPath);
        if(toInfo.isFile() && args.length() > 2){
            qDebug() << "gio: Destination" << toPath << "is not a directory";
            parser->showHelp(EXIT_FAILURE);
        }
        bool failed = false;
        for(const auto& path : args.mid(0, args.length() - 1)){
            auto url = urlFromPath(path);
            if(!url.isLocalFile()){
                GIO_ERROR(url, path, "Error while parsing path", "Path is not a local file");
                failed = true;
                continue;
            }
            QFileInfo info(path);
            if(!info.exists()){
                GIO_ERROR(url, path, "Error when getting information for file", "No such file or directory");
                failed = true;
                continue;
            }
            if(info.isDir()){
                qDebug() << "gio:" << url << ": Can’t recursively copy directory";
                failed = true;
                continue;
            }
            if(!info.isReadable()){
                GIO_ERROR(url, path, "Error opening file", "Permission denied");
                failed = true;
                continue;
            }
        }
        if(failed){
            return EXIT_FAILURE;
        }
        auto cpArgs = QStringList() << "cp";
        if(parser->isSet(noTargetDirectoryOption)){
            cpArgs.append("-T");
        }
        if(parser->isSet(interactiveOption)){
            cpArgs.append("-i");
        }
        if(parser->isSet(preserveOption) && !parser->isSet(defaultPermissionsOption)){
            cpArgs.append("-p");
        }
        if(parser->isSet(noDereferenceOption)){
            cpArgs.append("-P");
        }else{
            cpArgs.append("-L");
        }
        auto* p = new QProcess();
        p->setInputChannelMode(QProcess::ForwardedInputChannel);
        p->setProcessChannelMode(QProcess::ForwardedChannels);

        p->start("busybox", cpArgs + args);
        qApp->processEvents();
        p->waitForFinished();
        auto res = p->exitCode();
        p->deleteLater();
        return res;
    }
private:
    QCommandLineOption noTargetDirectoryOption = QCommandLineOption(
        {"T", "no-target-directory"},
        "Don’t copy into DESTINATION even if it is a directory."
    );
    QCommandLineOption progressOption = QCommandLineOption(
        {"p", "progress"},
        "NOT IMPLEMENTED"
    );
    QCommandLineOption interactiveOption = QCommandLineOption(
        {"i", "interactive"},
        "Prompt for confirmation before overwriting files."
    );
    QCommandLineOption preserveOption = QCommandLineOption(
        "preserve",
        "Preserve all attributes of copied files."
    );
    QCommandLineOption backupOption = QCommandLineOption(
        {"b", "backup"},
        "NOT IMPLEMENTED"
    );
    QCommandLineOption noDereferenceOption = QCommandLineOption(
        {"P", "no-dereference"},
        "Never follow symbolic links."
    );
    QCommandLineOption defaultPermissionsOption = QCommandLineOption(
        "default-permissions",
        "Use the default permissions of the current process for the destination file, rather than copying the permissions of the source file."
    );
};

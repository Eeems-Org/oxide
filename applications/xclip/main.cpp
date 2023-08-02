#include <QCommandLineParser>

#include <string>
#include <sys/stat.h>
#include <liboxide.h>
#include <liboxide/tarnish.h>

using namespace Oxide::Sentry;

QTextStream& qStdOut(){
    static QTextStream ts(stdout);
    return ts;
}

int main(int argc, char *argv[]){
    QCoreApplication app(argc, argv);
    sentry_init("xclip", argv);
    app.setOrganizationName("Eeems");
    app.setOrganizationDomain(OXIDE_SERVICE);
    app.setApplicationName("xclip");
    app.setApplicationVersion(APP_VERSION);
    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription("Command line interface for the clipboard");
    parser.applicationDescription();
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption inOption({"i", "in"}, "Read text into the clipboard from standard input or files (default)");
    parser.addOption(inOption);
    QCommandLineOption outOption({"o", "out"}, "Prints the selection to standard out (generally for piping to a file or program)");
    parser.addOption(outOption);
    QCommandLineOption filterOption({"f", "filter"}, "NOT IMPLEMENTED");
    parser.addOption(filterOption);
    QCommandLineOption rmlastnlOption({"r", "rmlastnl"}, "When the last character is a newline character, remove it.");
    parser.addOption(rmlastnlOption);
    QCommandLineOption loopsOption({"l", "loops"}, "NOT IMPLEMENTED");
    parser.addOption(loopsOption);
    QCommandLineOption targetOption({"t", "target"}, "NOT IMPLEMENTED");
    parser.addOption(targetOption);
    QCommandLineOption displayOption({"d", "display"}, "NOT IMPLEMENTED");
    parser.addOption(displayOption);
    QCommandLineOption selectionOption("selection", "NOT IMPLEMENTED");
    parser.addOption(selectionOption);
    QCommandLineOption silentOption("silent", "forks into the background to wait for requests, no informational output, errors only (default)");
    parser.addOption(silentOption);
    QCommandLineOption verboseOption("verbose", "Provide a running commentary of what xclip is doing");
    parser.addOption(verboseOption);
    QCommandLineOption noutf8Option("noutf8", "NOT IMPLEMENTED");
    parser.addOption(noutf8Option);
    parser.addPositionalArgument("files", "File(s) to read from", "[FILE]...");
    parser.process(app);
    if(parser.isSet(verboseOption)){
        qputenv("DEBUG", "1");
    }
    auto system = Oxide::Tarnish::systemApi();
    if(system == nullptr){
        qDebug() << "Failed to get system API";
        return EXIT_FAILURE;
    }
    QStringList args = parser.positionalArguments();
    if(parser.isSet(outOption)){
        auto data = system->clipboard();
        if(parser.isSet(rmlastnlOption) && data.right(1) == "\n"){
            data.chop(1);
        }
        qStdOut() << data;
        return EXIT_SUCCESS;
    }
    if(args.empty()){
        system->setClipboard(QTextStream(stdin).readAll().toUtf8());
        return EXIT_SUCCESS;
    }
    for(auto path : args){
        if(!QFile::exists(path)){
            qDebug().nospace() << "xclip: " << path.toStdString().c_str() << ": No such file or directory";
            return EXIT_FAILURE;
        }
    }
    QByteArray data;
    for(auto path : args){
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)){
            qDebug().nospace() << "xclip: " << path.toStdString().c_str() << ": " << file.errorString();
            return EXIT_FAILURE;
        }
        data += file.readAll();
    }
    system->setClipboard(data);
    return EXIT_SUCCESS;
}

#include <QCommandLineParser>

#include <string>
#include <sys/stat.h>
#include <liboxide.h>
#include <libblight.h>

using namespace Oxide::Sentry;

QTextStream& qStdOut(){
    static QTextStream ts(stdout);
    return ts;
}

void readClipboard(Blight::clipboard_t& clipboard, bool rmlastnlOption){
    QByteArray data;
    data.setRawData(reinterpret_cast<char*>(clipboard.data.get()), clipboard.size);
    if(rmlastnlOption && data.right(1) == "\n"){
        data.chop(1);
    }
    qStdOut() << data;
}
int setClipboard(const QString& selection, QByteArray& data, bool verbose,  bool rmlastnlOption){
    if(verbose){
        qDebug() << "Setting selection:" << selection;
    }
    if(rmlastnlOption && data.right(1) == "\n"){
        data.chop(1);
    }
    auto name = selection.toStdString();
    if(name != "secondary" && name != "clipboard"){
        name = "selection";
    }
    Blight::clipboard_t clipboard(name, (Blight::data_t)data.data(), data.size());
    if(Blight::setClipboard(clipboard)){
        return EXIT_SUCCESS;
    }
    qDebug() << "Failed to get clipboard:" << std::strerror(errno);
    return EXIT_FAILURE;
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
    QCommandLineOption loopsOption({"l", "loops"}, "NOT IMPLEMENTED", "loops");
    parser.addOption(loopsOption);
    QCommandLineOption targetOption({"t", "target"}, "NOT IMPLEMENTED", "target");
    parser.addOption(targetOption);
    QCommandLineOption displayOption({"d", "display"}, "NOT IMPLEMENTED", "display");
    parser.addOption(displayOption);
    QCommandLineOption selectionOption(
        {"sel", "selection"},
        "Specify which selection to use. Options are \"primary\" (default), \"secondary\", and \"clipboard\".",
        "selection",
        "primary"
    );
    parser.addOption(selectionOption);
    QCommandLineOption silentOption("silent", "forks into the background to wait for requests, no informational output, errors only (default)");
    parser.addOption(silentOption);
    QCommandLineOption verboseOption("verbose", "Provide a running commentary of what xclip is doing");
    parser.addOption(verboseOption);
    QCommandLineOption noutf8Option("noutf8", "NOT IMPLEMENTED");
    parser.addOption(noutf8Option);
    parser.addPositionalArgument("files", "File(s) to read from", "[FILE]...");
    parser.process(app);
    auto verbose = parser.isSet(verboseOption);
    if(verbose){
        qputenv("DEBUG", "1");
    }
#ifdef EPAPER
    auto connected = Blight::connect(true);
#else
    auto connected = Blight::connect(false);
#endif
    if(!connected){
        qDebug() << "Failed to connect to DBus";
        return EXIT_FAILURE;
    }
    QStringList args = parser.positionalArguments();
    if(parser.isSet(outOption)){
        auto selection = parser.value(selectionOption);
        if(verbose){
            qDebug() << "Getting selection:" << selection;
        }
        if(selection == "secondary"){
            auto maybe = Blight::secondary();
            if(!maybe.has_value()){
                qDebug() << "Failed to get clipboard:" << std::strerror(errno);
                return EXIT_FAILURE;
            }
            readClipboard(maybe.value(), parser.isSet(rmlastnlOption));
        }else if(selection == "clipboard"){
            auto maybe = Blight::clipboard();
            if(!maybe.has_value()){
                qDebug() << "Failed to get clipboard:" << std::strerror(errno);
                return EXIT_FAILURE;
            }
            readClipboard(maybe.value(), parser.isSet(rmlastnlOption));
        }else{
            auto maybe = Blight::selection();
            if(!maybe.has_value()){
                qDebug() << "Failed to get clipboard:" << std::strerror(errno);
                return EXIT_FAILURE;
            }
            readClipboard(maybe.value(), parser.isSet(rmlastnlOption));
        }
        return EXIT_SUCCESS;
    }
    if(args.empty()){
        auto data = QTextStream(stdin).readAll().toUtf8();
        auto selection = parser.value(selectionOption);
        return setClipboard(selection, data, verbose, parser.isSet(rmlastnlOption));
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
    auto selection = parser.value(selectionOption);
    return setClipboard(selection, data, verbose, parser.isSet(rmlastnlOption));
}

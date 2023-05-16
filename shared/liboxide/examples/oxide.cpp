/*!
 * \file
 */
//! [debugEnabled]
if(Oxide::debugEnabled()){
     qDebug() << "Debugging is enabled";
}
//! [debugEnabled]
//! [dispatchToMainThread]
Oxide::dispatchToMainThread([=]{
     qDebug() << "This is running on the main thread";
});
//! [dispatchToMainThread]
//! [SignalHandler]
SignalHandler::setup_unix_signal_handlers();

connect(signalHandler, &SignalHandler::sigUsr1, [=]{
     qDebug() << "SIGUSR1 recieved";
});
//! [SignalHandler]
//! [getGID]
try{
     auto gid = Oxide::getGID("admin");
     qDebug() << "admin GID is " << gid;
}catch(const std::exception& e){
     qDebug() << "Failed to get group: " << e.what();
}
//! [getGID]
//! [getUID]
try{
     auto gid = Oxide::getUID("root");
     qDebug() << "root UID is " << gid;
}catch(const std::exception& e){
     qDebug() << "Failed to get user: " << e.what();
}
//! [getUID]
//! [EventFilter]
QGuiApplication app(argc, argv);
auto filter = new EventFilter(&app);
app.installEventFilter(filter);
return app.exec();
//! [EventFilter]

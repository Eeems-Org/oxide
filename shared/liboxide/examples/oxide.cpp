/*!
 * \file examples/oxide.cpp
 */
//! [debugEnabled]
if(Oxide::debugEnabled()){
     qDebug() << "Debugging is enabled";
}
//!  [debugEnabled]
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

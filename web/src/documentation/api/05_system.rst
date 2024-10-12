==========
System API
==========

+----------------------+----------------------+----------------------+
| Name                 | Specification        | Description          |
+======================+======================+======================+
| autoSleep            | ``INT32`` property   | Number of minutes of |
|                      | (read/write)         | inactivity before    |
|                      |                      | the system suspends  |
|                      |                      | automatically.       |
|                      |                      | If it's set to ``0`` |
|                      |                      | auto sleep is        |
|                      |                      | disabled.            |
+----------------------+----------------------+----------------------+
| sleepInhibited       | ``BOOLEAN`` property | If system suspend is |
|                      | (read)               | inhibited or not.    |
+----------------------+----------------------+----------------------+
| powerOffInhibited    | ``BOOLEAN`` property | If power off is      |
|                      | (read)               | inhibited or not.    |
+----------------------+----------------------+----------------------+
| s                    | signal               | Signal sent when     |
| leepInhibitedChanged |                      | sleepInhibited       |
|                      | - (out) ``BOOLEAN``  | changes.             |
+----------------------+----------------------+----------------------+
| powe                 | signal               | Signal sent when     |
| rOffInhibitedChanged |                      | powerOffInhibited    |
|                      | - (out) ``BOOLEAN``  | changes.             |
+----------------------+----------------------+----------------------+
| autoSleepChanged     | signal               | Signal sent when     |
|                      |                      | autoSleep changes.   |
|                      | - (out) ``BOOLEAN``  |                      |
+----------------------+----------------------+----------------------+
| deviceSuspending     | signal               | Signal sent when     |
|                      |                      | system is entering a |
|                      |                      | suspended state.     |
|                      |                      | Applications can use |
|                      |                      | this signal to run   |
|                      |                      | cleanup code before  |
|                      |                      | the system suspends. |
+----------------------+----------------------+----------------------+
| deviceResuming       | signal               | Signal sent when the |
|                      |                      | system is resuming   |
|                      |                      | from sleep.          |
|                      |                      | Applications can use |
|                      |                      | this signal to       |
|                      |                      | reinitialise after   |
|                      |                      | the system suspends. |
+----------------------+----------------------+----------------------+
| suspend              | method               | Attempt to suspend   |
|                      |                      | the system.          |
+----------------------+----------------------+----------------------+
| powerOff             | method               | Attempt to power off |
|                      |                      | the system.          |
+----------------------+----------------------+----------------------+
| reboot               | method               | Attempt to reboot    |
|                      |                      | the system.          |
+----------------------+----------------------+----------------------+
| activity             | method               | Reset the auto sleep |
|                      |                      | timer.               |
|                      |                      | Applications should  |
|                      |                      | use this to track    |
|                      |                      | user activity that   |
|                      |                      | can't be normally    |
|                      |                      | tracked through      |
|                      |                      | interaction with the |
|                      |                      | touchscreen, pen, or |
|                      |                      | buttons.             |
+----------------------+----------------------+----------------------+
| inhibitSleep         | method               | Block system sleep   |
|                      |                      | from happening until |
|                      |                      | uninhibitSleep is    |
|                      |                      | called, or the       |
|                      |                      | application exits.   |
+----------------------+----------------------+----------------------+
| uninhibitSleep       | method               | Unblock system       |
|                      |                      | sleep.               |
+----------------------+----------------------+----------------------+
| inhibitPowerOff      | method               | Block system         |
|                      |                      | shutdown or reboot   |
|                      |                      | from happening until |
|                      |                      | uninhibitSleep is    |
|                      |                      | called, or the       |
|                      |                      | application exits.   |
+----------------------+----------------------+----------------------+
| uninhibitPowerOff    | method               | Unblock system       |
|                      |                      | shutdown and reboot. |
+----------------------+----------------------+----------------------+

.. _example-usage-9:

Example Usage
~~~~~~~~~~~~~

.. code:: cpp

   #include <liboxide/dbus.h>

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       QCoreApplication app(argc, argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting system API...";
       QDBusObjectPath path = api.requestAPI("system");
       if(path.path() == "/"){
           qDebug() << "Unable to get system API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the system API!";

       System system(OXIDE_SERVICE, path.path(), bus);
       QObject::connect(&system, &System::leftAction, []{
           qDebug() << "Left button long pressed";
           qApp->exit();
       });
       return app.exec();
   }

.. code:: shell

   #!/bin/bash
   rot --once system listen leftAction
   echo "Left button long pressed!"


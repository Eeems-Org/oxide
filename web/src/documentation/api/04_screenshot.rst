==========
Screen API
==========

+---------------------+----------------------+----------------------+
| Name                | Specification        | Description          |
+=====================+======================+======================+
| screenshots         | `                    | Get the list of      |
|                     | `ARRAY OBJECT_PATH`` | screenshots on the   |
|                     | property (read)      | device.              |
+---------------------+----------------------+----------------------+
| screenshotAdded     | signal               | Signal sent when a   |
|                     | - (out)              | screenshot is added. |
|                     | ``OBJECT_PATH``      |                      |
+---------------------+----------------------+----------------------+
| screenshotRemoved   | signal               | Signal sent when a   |
|                     | - (out)              | screenshot is        |
|                     | ``OBJECT_PATH``      | removed.             |
+---------------------+----------------------+----------------------+
| screenshotModified  | signal               | Signal sent when a   |
|                     | - (out)              | screenshot is        |
|                     | ``OBJECT_PATH``      | modified.            |
+---------------------+----------------------+----------------------+
| addScreenshot       | method               | Add a screenshot     |
|                     | - (in) blob          | taken by an          |
|                     | ``ARRAY BYTE``       | application.         |
|                     | - (out)              |                      |
|                     | ``OBJECT_PATH``      |                      |
+---------------------+----------------------+----------------------+
| drawFullScreenImage | method               | Draw an image to the |
|                     | - (in) path          | screen.              |
|                     | ``STRING``           |                      |
|                     | - (out) ``BOOLEAN``  |                      |
+---------------------+----------------------+----------------------+
| screenshot          | method               | Take a screenshot.   |
|                     | - (out)              |                      |
|                     | ``OBJECT_PATH``      |                      |
+---------------------+----------------------+----------------------+

.. _example-usage-7:

Example Usage
~~~~~~~~~~~~~

.. code:: cpp

   #include <liboxide.h>
   #include "dbusservice_interface.h"
   #include "screenapi_interface.h"

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       Q_UNUSED(argc);
       Q_UNUSED(argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting screen API...";
       QDBusObjectPath path = api.requestAPI("screen");
       if(path.path() == "/"){
           qDebug() << "Unable to get screen API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the screen API!";

       Screen screen(OXIDE_SERVICE, path.path(), bus);
       path = screen.screenshot();
       if(path.path() == "/"){
           qDebug() << "Screenshot failed";
       }else{
           qDebug() << "Screenshot taken";            
       }
       return EXIT_SUCCESS;
   }

.. code:: shell

   #!/bin/bash
   echo -n "Screenshot "
   if [ $(rot screen call screenshot | jq -cr) = "/" ]; then
   	echo "failed"
   else
   	echo "taken"
   fi

Screenshot
~~~~~~~~~~

+----------+----------------------------+----------------------------+
| Name     | Specification              | Description                |
+==========+============================+============================+
| blob     | ``ARRAY BYTE`` property    | The blob data of the       |
|          | (read/write)               | screenshot.                |
+----------+----------------------------+----------------------------+
| path     | ``STRING`` property (read) | The path to the screenshot |
|          |                            | on disk.                   |
+----------+----------------------------+----------------------------+
| modified | signal                     | Signal sent when the       |
|          |                            | screenshot is modified.    |
+----------+----------------------------+----------------------------+
| removed  | signal                     | Signal sent when the       |
|          |                            | screenshot is removed.     |
+----------+----------------------------+----------------------------+
| remove   | method                     | Remove the screenshot from |
|          |                            | the device.                |
+----------+----------------------------+----------------------------+

.. _example-usage-8:

Example Usage
^^^^^^^^^^^^^

.. code:: cpp

   #include <liboxide.h>
   #include "dbusservice_interface.h"
   #include "screenapi_interface.h"
   #include "screenshot_interface.h"

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       Q_UNUSED(argc);
       Q_UNUSED(argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting screen API...";
       QDBusObjectPath path = api.requestAPI("screen");
       if(path.path() == "/"){
           qDebug() << "Unable to get screen API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the screen API!";

       Screen screen(OXIDE_SERVICE, path.path(), bus);
       for(auto path : screen.screenshots()){
           Screenshot(OXIDE_SERVICE, path.path(), bus).remove().waitForFinished();
       }
       qDebug() << "Screenshots removed";
       return EXIT_SUCCESS;
   }

.. code:: shell

   #!/bin/bash
   rot screen get screenshots \
   	| jq -cr 'values | join("\n")' \
   	| sed 's|/codes/eeems/oxide1/||' \
   	| xargs -rI {} rot --object Screenshot:{} screen call remove
   echo "Screenshots removed"

System API
----------

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
| leftAction           | signal               | Signal sent when a   |
|                      |                      | long press on the    |
|                      |                      | left button happens. |
+----------------------+----------------------+----------------------+
| homeAction           | signal               | Signal sent when a   |
|                      |                      | long press on the    |
|                      |                      | home button happens. |
+----------------------+----------------------+----------------------+
| rightAction          | signal               | Signal sent when a   |
|                      |                      | long press on the    |
|                      |                      | right button         |
|                      |                      | happens.             |
+----------------------+----------------------+----------------------+
| powerAction          | signal               | Signal sent when a   |
|                      |                      | long press on the    |
|                      |                      | right button         |
|                      |                      | happens.             |
+----------------------+----------------------+----------------------+
| s                    | signal               | Signal sent when     |
| leepInhibitedChanged | - (out) ``BOOLEAN``  | sleepInhibited       |
|                      |                      | changes.             |
+----------------------+----------------------+----------------------+
| powe                 | signal               | Signal sent when     |
| rOffInhibitedChanged | - (out) ``BOOLEAN``  | powerOffInhibited    |
|                      |                      | changes.             |
+----------------------+----------------------+----------------------+
| autoSleepChanged     | signal               | Signal sent when     |
|                      | - (out) ``BOOLEAN``  | autoSleep changes.   |
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

   #include <liboxide.h>
   #include "dbusservice_interface.h"
   #include "systemapi_interface.h"

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


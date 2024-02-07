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


================
Notification API
================

+----------------------+----------------------+----------------------+
| Name                 | Specification        | Description          |
+======================+======================+======================+
| allNotifications     | `                    | List of all current  |
|                      | `ARRAY OBJECT_PATH`` | applications.        |
|                      | property (read)      |                      |
+----------------------+----------------------+----------------------+
| unownedNotifications | `                    | List of all          |
|                      | `ARRAY OBJECT_PATH`` | applications where   |
|                      | property (read)      | the owning           |
|                      |                      | application is no    |
|                      |                      | longer running.      |
+----------------------+----------------------+----------------------+
| notificationAdded    | signal               | Signal sent when a   |
|                      | - (out)              | notification has     |
|                      | ``OBJECT_PATH``      | been added.          |
+----------------------+----------------------+----------------------+
| notificationRemoved  | signal               | Signal sent when a   |
|                      | - (out)              | notification has     |
|                      | ``OBJECT_PATH``      | been removed.        |
+----------------------+----------------------+----------------------+
| notificationChanged  | signal               | Signal sent when a   |
|                      | - (out)              | notification has     |
|                      | ``OBJECT_PATH``      | been modified.       |
+----------------------+----------------------+----------------------+
| add                  | method               | Add a new            |
|                      | - (in) identifier    | notification.        |
|                      | ``STRING``           | If the application   |
|                      | - (in) application   | doesn't have         |
|                      | ``STRING``           | permission to add    |
|                      | - (in) text          | the notification, or |
|                      | ``STRING``           | it fails to be added |
|                      | - (in) icon          | it will return       |
|                      | ``STRING``           | ``/``.               |
|                      | - (out)              |                      |
|                      | ``OBJECT_PATH``      |                      |
+----------------------+----------------------+----------------------+
| take                 | method               | Take ownership of a  |
|                      | - (in) identifier    | notification.        |
|                      | ``STRING``           |                      |
|                      | - (out) ``BOOLEAN``  |                      |
+----------------------+----------------------+----------------------+
| notifications        | method               | List of all          |
|                      | - (out)              | notifications owned  |
|                      | `                    | by the current       |
|                      | `ARRAY OBJECT_PATH`` | application.         |
+----------------------+----------------------+----------------------+
| get                  | method               | Get the object path  |
|                      | - (in) identifier    | for a notification   |
|                      | ``STRING``           | based on it's        |
|                      | - (out)              | identifier.          |
|                      | ``OBJECT_PATH``      |                      |
+----------------------+----------------------+----------------------+

.. _example-usage-4:

Example Usage
~~~~~~~~~~~~~

.. code:: cpp

   #include <liboxide.h>
   #include "dbusservice_interface.h"
   #include "notificationapi_interface.h"
   #include "notification_interface.h"

   using namespace codes::eeems::oxide1;

   int main(int argc, char *argv[]){
       Q_UNUSED(argc);
       Q_UNUSED(argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting notification API...";
       QDBusObjectPath path = api.requestAPI("notification");
       if(path.path() == "/"){
           qDebug() << "Unable to get notification API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the notification API!";

       Notifications notifications(OXIDE_SERVICE, path.path(), bus);
       qDebug() << "Notificaitons:" << notifications.allNotifications();
       return EXIT_SUCCESS;
   }

.. code:: shell

   #!/bin/bash
   echo -n "Notifications: "
   rot notification get notifications | jq

Notification Object
~~~~~~~~~~~~~~~~~~~

+-------------+--------------------------+--------------------------+
| Name        | Specification            | Description              |
+=============+==========================+==========================+
| identifier  | ``STRING`` property      | Unique identifier for    |
|             | (read)                   | the notification.        |
+-------------+--------------------------+--------------------------+
| application | ``STRING`` property      | Name of the application  |
|             | (read/write)             | that owns this           |
|             |                          | notification.            |
+-------------+--------------------------+--------------------------+
| text        | ``STRING`` property      | Text of the              |
|             | (read/write)             | notification.            |
+-------------+--------------------------+--------------------------+
| icon        | ``STRING`` property      | Icon used for the        |
|             | (read/write)             | notification.            |
+-------------+--------------------------+--------------------------+
| changed     | signal                   | Signal sent when         |
|             | - (out)                  | something on the         |
|             | `                        | notification has         |
|             | `ARRAY{STRING VARIANT}`` | changed.                 |
|             |                          | The first output         |
|             |                          | property contains a map  |
|             |                          | of changed properties    |
|             |                          | and their values.        |
+-------------+--------------------------+--------------------------+
| removed     | signal                   | Signal sent when this    |
|             |                          | notification has been    |
|             |                          | removed.                 |
+-------------+--------------------------+--------------------------+
| displayed   | signal                   | Signal sent when this    |
|             |                          | notification has been    |
|             |                          | displayed.               |
+-------------+--------------------------+--------------------------+
| clicked     | signal                   | Signal sent when this    |
|             |                          | notification has been    |
|             |                          | clicked.                 |
+-------------+--------------------------+--------------------------+
| display     | method                   | Display the notification |
|             |                          | to the user.             |
|             |                          | This will interrupt what |
|             |                          | the user is doing to     |
|             |                          | display the text on the  |
|             |                          | bottom left corner.      |
+-------------+--------------------------+--------------------------+
| remove      | method                   | Remove the notification. |
+-------------+--------------------------+--------------------------+
| click       | method                   | Emit the ``clicked``     |
|             |                          | signal on the            |
|             |                          | notification so that the |
|             |                          | owning application can   |
|             |                          | handle the click.        |
+-------------+--------------------------+--------------------------+

.. _example-usage-5:

Example Usage
^^^^^^^^^^^^^

.. code:: cpp

   #include <QUuid>
   #include <liboxide.h>
   #include "dbusservice_interface.h"
   #include "notificationapi_interface.h"
   #include "notification_interface.h"

   using namespace codes::eeems::oxide1;

   int main(int argc, char *argv[]){
       Q_UNUSED(argc);
       Q_UNUSED(argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting notification API...";
       QDBusObjectPath path = api.requestAPI("notification");
       if(path.path() == "/"){
           qDebug() << "Unable to get notification API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the notification API!";

       Notifications notifications(OXIDE_SERVICE, path.path(), bus);
       auto guid = QUuid::createUuid().toString();
       qDebug() << "Adding notification" << guid;
       path = notifications.add(guid, "codes.eeems.fret", "Hello world!", "");
       if(path.path() == "/"){
           qDebug() << "Failed to add notification";
           return EXIT_FAILURE;
       }

       Notification notification(OXIDE_SERVICE, path.path(), bus);
       qDebug() << "Displaying notification" << guid;
       notification.display().waitForFinished();
       notification.remove();
       return EXIT_SUCCESS;
   }

.. code:: shell

   #!/bin/bash
   uuid=$(cat /proc/sys/kernel/random/uuid)
   path=$(rot notification call add \
           "QString:\"$uuid\"" \
           'QString:"sample-application"' \
           'QString:"Hello world!"' \
           'QString:""' \
   	| jq -cr \
   	| sed 's|/codes/eeems/oxide1/||'
   )
   echo "Displaying notification $uuid"
   rot --object Notification:$path notification call display
   rot --object Notification:$path notification call remove

Power API
---------

+----------------------+----------------------+----------------------+
| Name                 | Specification        | Description          |
+======================+======================+======================+
| state                | ``INT32`` property   | Currently requested  |
|                      | (read/write)         | power state.         |
|                      |                      | Possible values:     |
|                      |                      | - ``0`` Normal       |
|                      |                      | - ``1`` Power Saving |
+----------------------+----------------------+----------------------+
| batteryState         | ``INT32`` property   | Current battery      |
|                      | (read)               | state.               |
|                      |                      | - ``0`` Unknown      |
|                      |                      | - ``1`` Charging     |
|                      |                      | - ``2`` Discharging  |
|                      |                      | - ``3`` Not Present  |
+----------------------+----------------------+----------------------+
| batteryLevel         | ``INT32`` property   | Current battery      |
|                      | (read)               | percentage.          |
+----------------------+----------------------+----------------------+
| batteryTemperature   | ``INT32`` property   | Current battery      |
|                      | (read)               | temperature in       |
|                      |                      | Celsius.             |
+----------------------+----------------------+----------------------+
| chargerState         | ``INT32`` property   | Current charger      |
|                      | (read)               | state.               |
|                      |                      | - ``0`` Unknown      |
|                      |                      | - ``1`` Connected    |
|                      |                      | - ``2`` Not          |
|                      |                      | Connected            |
+----------------------+----------------------+----------------------+
| stateChanged         | signal               | Signal sent when the |
|                      | - (out) ``INT32``    | requested power      |
|                      |                      | state has changed.   |
+----------------------+----------------------+----------------------+
| batteryStateChanged  | signal               | Signal sent when the |
|                      | - (out) ``INT32``    | battery state has    |
|                      |                      | changed.             |
+----------------------+----------------------+----------------------+
| batteryLevelChanged  | signal               | Signal sent when the |
|                      | - (out) ``INT32``    | battery level has    |
|                      |                      | changed.             |
+----------------------+----------------------+----------------------+
| batte                | signal               | Signal sent when the |
| ryTemperatureChanged | - (out) ``INT32``    | battery temperature  |
|                      |                      | has changed.         |
+----------------------+----------------------+----------------------+
| chargerStateChanged  | signal               | Signal sent when the |
|                      | - (out) ``INT32``    | charger state has    |
|                      |                      | changed.             |
+----------------------+----------------------+----------------------+
| batteryWarning       | signal               | Signal sent when a   |
|                      |                      | battery warning has  |
|                      |                      | been detected.       |
+----------------------+----------------------+----------------------+
| batteryAlert         | signal               | Signal sent when a   |
|                      |                      | battery alert has    |
|                      |                      | been detected.       |
+----------------------+----------------------+----------------------+
| chargerWarning       | signal               | Signal sent when a   |
|                      |                      | charger warning has  |
|                      |                      | been detected.       |
+----------------------+----------------------+----------------------+

.. _example-usage-6:

Example Usage
~~~~~~~~~~~~~

.. code:: cpp

   #include <liboxide.h>
   #include "dbusservice_interface.h"
   #include "powerapi_interface.h"

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       QCoreApplication app(argc, argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting power API...";
       QDBusObjectPath path = api.requestAPI("power");
       if(path.path() == "/"){
           qDebug() << "Unable to get power API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the power API!";

       Power power(OXIDE_SERVICE, path.path(), bus);
       qDebug() << "Logging battery level:";
       qDebug() << power.batteryLevel();
       QObject::connect(&power, &Power::batteryLevelChanged, [](int batteryLevel){
           qDebug() << batteryLevel;
       });
       return app.exec();
   }

.. code:: shell

   #!/bin/bash
   echo "Logging battery level:"
   rot power get batteryLevel
   rot power listen batteryLevelChanged


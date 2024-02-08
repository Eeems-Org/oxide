=========
Power API
=========

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
|                      |                      | requested power      |
|                      | - (out) ``INT32``    | state has changed.   |
+----------------------+----------------------+----------------------+
| batteryStateChanged  | signal               | Signal sent when the |
|                      |                      | battery state has    |
|                      | - (out) ``INT32``    | changed.             |
+----------------------+----------------------+----------------------+
| batteryLevelChanged  | signal               | Signal sent when the |
|                      |                      | battery level has    |
|                      | - (out) ``INT32``    | changed.             |
+----------------------+----------------------+----------------------+
| batte                | signal               | Signal sent when the |
| ryTemperatureChanged |                      | battery temperature  |
|                      | - (out) ``INT32``    | has changed.         |
+----------------------+----------------------+----------------------+
| chargerStateChanged  | signal               | Signal sent when the |
|                      |                      | charger state has    |
|                      | - (out) ``INT32``    | changed.             |
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

   #include <liboxide/dbus.h>

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

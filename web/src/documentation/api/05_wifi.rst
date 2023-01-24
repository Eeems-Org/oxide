========
Wifi API
========

This API interacts with the wpa_supplicant D-Bus API. You can find more
context here: https://w1.fi/wpa_supplicant/devel/dbus.html

+------------------+------------------------+------------------------+
| Name             | Specification          | Description            |
+==================+========================+========================+
| state            | ``INT32`` property     | Get the current wifi   |
|                  | (read)                 | state.                 |
|                  |                        | - ``0`` Unknown        |
|                  |                        | - ``1`` Off            |
|                  |                        | - ``2`` Disconnected   |
|                  |                        | - ``3`` Offline        |
|                  |                        | - ``4`` Online         |
+------------------+------------------------+------------------------+
| bSSs             | ``ARRAY OBJECT_PATH``  | The list of known      |
|                  | property (read)        | Basic Service Sets.    |
+------------------+------------------------+------------------------+
| link             | ``INT32`` property     | Link quality of the    |
|                  | (read)                 | current connection as  |
|                  |                        | a percentage (rM1      |
|                  |                        | only).                 |
+------------------+------------------------+------------------------+
| rssi             | ``INT32`` property     | Link quality of the    |
|                  | (read)                 | current connection in  |
|                  |                        | dBm.                   |
+------------------+------------------------+------------------------+
| network          | ``OBJECT_PATH``        | Currently connected    |
|                  | property (read)        | network.               |
|                  |                        | Will return ``/`` if   |
|                  |                        | no network is          |
|                  |                        | connected.             |
+------------------+------------------------+------------------------+
| networks         | ``ARRAY OBJECT_PATH``  | List of known          |
|                  | property (read)        | Networks.              |
+------------------+------------------------+------------------------+
| scanning         | ``BOOLEAN`` property   | If the device is       |
|                  | (read)                 | currently scanning for |
|                  |                        | networks.              |
+------------------+------------------------+------------------------+
| stateChanged     | signal                 | Signal sent when the   |
|                  | - (out) ``INT32``      | wifi state changes.    |
+------------------+------------------------+------------------------+
| linkChanged      | signal                 | Signal sent when the   |
|                  | - (out) ``INT32``      | wifi link quality      |
|                  |                        | changes.               |
+------------------+------------------------+------------------------+
| networkAdded     | signal                 | Signal sent when a     |
|                  | - (out)                | Network is added.      |
|                  | ``OBJECT_PATH``        |                        |
+------------------+------------------------+------------------------+
| networkRemoved   | signal                 | Signal sent when a     |
|                  | - (out)                | Network is removed.    |
|                  | ``OBJECT_PATH``        |                        |
+------------------+------------------------+------------------------+
| networkConnected | signal                 | Signal sent when a     |
|                  | - (out)                | Network has been       |
|                  | ``OBJECT_PATH``        | connected to.          |
+------------------+------------------------+------------------------+
| disconnected     | signal                 | Signal sent when the   |
|                  |                        | device is disconnected |
|                  |                        | from the current       |
|                  |                        | network.               |
+------------------+------------------------+------------------------+
| bssFound         | signal                 | Signal sent when a BSS |
|                  | - (out)                | is found.              |
|                  | ``OBJECT_PATH``        |                        |
+------------------+------------------------+------------------------+
| bssRemoved       | signal                 | Signal sent when a BSS |
|                  | - (out)                | expires from the       |
|                  | ``OBJECT_PATH``        | cache.                 |
+------------------+------------------------+------------------------+
| scanningChanged  | signal                 | Signal sent when       |
|                  | - (out) ``BOOLEAN``    | scanning state         |
|                  |                        | changes.               |
+------------------+------------------------+------------------------+
| enable           | method                 | Enable wifi.           |
|                  | - (out) ``BOOLEAN``    | Will return if it was  |
|                  |                        | successful or not.     |
+------------------+------------------------+------------------------+
| disable          | method                 | Disable wifi.          |
+------------------+------------------------+------------------------+
| addNetwork       | method                 | Add a Network that can |
|                  | - (in) ssid ``STRING`` | be connected to.       |
|                  | - (in) properties      |                        |
|                  | ``A                    |                        |
|                  | RRAY{STRING VARIANT}`` |                        |
|                  | - (out)                |                        |
|                  | ``OBJECT_PATH``        |                        |
+------------------+------------------------+------------------------+
| getNetwork       | method                 | Get a Network that     |
|                  | - (in) properties      | matches all the        |
|                  | ``A                    | supplied properties.   |
|                  | RRAY{STRING VARIANT}`` |                        |
|                  | - (out)                |                        |
|                  | ``OBJECT_PATH``        |                        |
+------------------+------------------------+------------------------+
| getBSS           | method                 | Get a BSS that matches |
|                  | - (in) properties      | all the supplied       |
|                  | ``A                    | properties.            |
|                  | RRAY{STRING VARIANT}`` |                        |
|                  | - (out)                |                        |
|                  | ``OBJECT_PATH``        |                        |
+------------------+------------------------+------------------------+
| scan             | method                 | Scan for networks.     |
|                  | - (in) active          | If the first argument  |
|                  | ``BOOLEAN``            | is ``true``, this will |
|                  |                        | be an active scan.     |
|                  |                        | The first argument     |
|                  |                        | defaults to ``false``. |
+------------------+------------------------+------------------------+
| reconnect        | method                 | Reconnect to a known   |
|                  |                        | Network.               |
+------------------+------------------------+------------------------+
| reassosiate      | method                 | Reassosiate with the   |
|                  |                        | currently connected    |
|                  |                        | Network.               |
+------------------+------------------------+------------------------+
| disconnect       | method                 | Disconnect from the    |
|                  |                        | current Network.       |
+------------------+------------------------+------------------------+
| flushBSSCache    | method                 | Flush all BSS items    |
|                  | - (in) age ``UINT32``  | from the cache older   |
|                  |                        | than a certain age.    |
|                  |                        | If age is ``0``, all   |
|                  |                        | BSS items will be      |
|                  |                        | removed.               |
+------------------+------------------------+------------------------+
| addBlob          | method                 | Add a blob to the      |
|                  | - (in) name ``STRING`` | wireless interface.    |
|                  | - (in) blob            |                        |
|                  | ``ARRAY BYTE``         |                        |
+------------------+------------------------+------------------------+
| removeBlob       | method                 | Remove a blob from the |
|                  | - (in) name ``STRING`` | wireless interface.    |
+------------------+------------------------+------------------------+
| getBlob          | method                 | Get a blob from the    |
|                  | - (in) name ``STRING`` | wireless interface.    |
|                  | - (out) ``ARRAY BYTE`` |                        |
+------------------+------------------------+------------------------+

.. _example-usage-10:

Example Usage
~~~~~~~~~~~~~

.. code:: cpp

   #include <liboxide.h>
   #include "dbusservice_interface.h"
   #include "wifiapi_interface.h"

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       QCoreApplication app(argc, argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting wifi API...";
       QDBusObjectPath path = api.requestAPI("wifi");
       if(path.path() == "/"){
           qDebug() << "Unable to get wifi API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the wifi API!";

       Wifi wifi(OXIDE_SERVICE, path.path(), bus);
       qDebug() << "Monitoring wifi state";
       QObject::connect(&wifi, &Wifi::stateChanged, [](int state){
           qDebug() << state;
       });
       return app.exec();
   }

.. code:: shell

   #!/bin/bash
   echo "Monitoring wifi state"
   rot wifi listen state

BSS
~~~

+-------------------+-----------------------+-----------------------+
| Name              | Specification         | Description           |
+===================+=======================+=======================+
| bssid             | ``STRING`` property   | Basic Service Set     |
|                   | (read)                | Identifier            |
+-------------------+-----------------------+-----------------------+
| ssid              | ``STRING`` property   | Service Set           |
|                   | (read)                | Identifier            |
+-------------------+-----------------------+-----------------------+
| privacy           | ``BOOLEAN`` property  | Indicates if the BSS  |
|                   | (read)                | supports privacy      |
+-------------------+-----------------------+-----------------------+
| frequency         | ``UINT16`` property   | Frequency n MHz       |
|                   | (read)                |                       |
+-------------------+-----------------------+-----------------------+
| network           | ``OBJECT_PATH``       | Network for this BSS. |
|                   | property (read)       | ``/`` if there is no  |
|                   |                       | Network.              |
+-------------------+-----------------------+-----------------------+
| key_mgmt          | ``ARRAY STRING``      | Key management suite. |
|                   | property (read)       |                       |
+-------------------+-----------------------+-----------------------+
| removed           | signal                | Signal sent when the  |
|                   |                       | BSS is removed from   |
|                   |                       | the cache.            |
+-------------------+-----------------------+-----------------------+
| propertiesChanged | signal                | Signal sent when      |
|                   | - (out)               | properties change on  |
|                   | ``                    | the BSS.              |
|                   | ARRAY{STRING VALUE}`` |                       |
+-------------------+-----------------------+-----------------------+
| connect           | method                | Attempt to connect to |
|                   | - (out)               | a BSS.                |
|                   | ``OBJECT_PATH``       | Returns the           |
|                   |                       | ``OBJECT_PATH`` for   |
|                   |                       | the network.          |
|                   |                       | If none exists a new  |
|                   |                       | network will be       |
|                   |                       | created, assuming a   |
|                   |                       | blank password.       |
+-------------------+-----------------------+-----------------------+

Network
~~~~~~~

+-------------------+-----------------------+-----------------------+
| Name              | Specification         | Description           |
+===================+=======================+=======================+
| enabled           | ``BOOLEAN`` property  | If this network is    |
|                   | (read/write)          | enabled or not.       |
|                   |                       | If it's not enable    |
|                   |                       | wpa_supplicant will   |
|                   |                       | not try to connect to |
|                   |                       | it.                   |
+-------------------+-----------------------+-----------------------+
| ssid              | ``STRING`` property   | Service Set           |
|                   | (read)                | Identifier            |
+-------------------+-----------------------+-----------------------+
| bSSs              | ``ARRAY OBJECT_PATH`` | Basic Service Sets    |
|                   | property (read)       | for this Network.     |
+-------------------+-----------------------+-----------------------+
| password          | ``STRING`` property   | Password used to      |
|                   | (read/write)          | connect to this       |
|                   |                       | Network.              |
+-------------------+-----------------------+-----------------------+
| protocol          | ``STRING`` property   | Protocol used to      |
|                   | (read/write)          | communicate with this |
|                   |                       | network.              |
|                   |                       | - ``psk``             |
|                   |                       | - ``eap``             |
|                   |                       | - ``sae``             |
+-------------------+-----------------------+-----------------------+
| properties        | ``AR                  | Properties of the     |
|                   | RAY{STRING VARIANT}`` | configured network.   |
|                   | property (read)/write | Dictionary contains   |
|                   |                       | entries from          |
|                   |                       | "network" block of    |
|                   |                       | wpa_supplicant        |
|                   |                       | configuration file.   |
+-------------------+-----------------------+-----------------------+
| stateChanged      | signal                | Signal sent when the  |
|                   | - (out) ``BOOLEAN``   | enabled property      |
|                   |                       | changes.              |
+-------------------+-----------------------+-----------------------+
| propertiesChanged | signal                | Signal sent when the  |
|                   | - (out)               | properties of the     |
|                   | ``AR                  | Network change.       |
|                   | RAY{STRING VARIANT}`` |                       |
+-------------------+-----------------------+-----------------------+
| connected         | signal                | Signal sent when the  |
|                   |                       | device connects to    |
|                   |                       | the Network.          |
+-------------------+-----------------------+-----------------------+
| disconnected      | signal                | Signal sent when the  |
|                   |                       | device disconnects to |
|                   |                       | the Network.          |
+-------------------+-----------------------+-----------------------+
| removed           | signal                | Signal sent when the  |
|                   |                       | Network is removed.   |
+-------------------+-----------------------+-----------------------+
| connect           | method                | Attempt to connect to |
|                   |                       | the Network.          |
+-------------------+-----------------------+-----------------------+
| remove            | method                | Remove the Network.   |
+-------------------+-----------------------+-----------------------+

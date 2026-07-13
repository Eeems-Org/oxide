==============
Frontlight API
==============

.. note::

  The Frontlight API is only available on devices with a frontlight
  (reMarkable Paper Pro and reMarkable Paper Pro Move).

+------------------------+----------------------+----------------------+
| Name                   | Specification        | Description          |
+========================+======================+======================+
| brightness             | ``INT32`` property   | Current frontlight   |
|                        | (read/write)         | brightness level.    |
|                        |                      | Range is ``0`` to    |
|                        |                      | ``100``. Setting     |
|                        |                      | to ``0`` disables    |
|                        |                      | the frontlight.      |
+------------------------+----------------------+----------------------+
| hasFrontlight          | ``BOOLEAN`` property | Whether the device   |
|                        | (read)               | has a frontlight.    |
+------------------------+----------------------+----------------------+
| extraBrightness        | ``BOOLEAN`` property | Whether extra        |
|                        | (read/write)         | brightness mode is   |
|                        |                      | enabled.             |
+------------------------+----------------------+----------------------+
| brightnessChanged      | signal               | Signal sent when the |
|                        |                      | brightness has       |
|                        | - (out) ``INT32``    | changed.             |
+------------------------+----------------------+----------------------+
| extraBrightnessChanged | signal               | Signal sent when the |
|                        |                      | extraBrightness has  |
|                        | - (out) ``BOOLEAN``  | changed.             |
+------------------------+----------------------+----------------------+

.. note::

  All methods and property writes on the Frontlight API require the
  ``frontlight`` permission.

.. _example-usage-frontlight:

Example Usage
~~~~~~~~~~~~~

.. code:: cpp

   #include <liboxide/dbus.h>

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       QCoreApplication app(argc, argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting frontlight API...";
       QDBusObjectPath path = api.requestAPI("frontlight");
       if(path.path() == "/"){
           qDebug() << "Unable to get frontlight API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the frontlight API!";

       Frontlight frontlight(OXIDE_SERVICE, path.path(), bus);
       if(!frontlight.hasFrontlight()){
           qDebug() << "Device does not have a frontlight";
           return EXIT_FAILURE;
       }
       qDebug() << "Current brightness:" << frontlight.brightness();
       frontlight.setBrightness(50);
       return app.exec();
   }

.. code:: shell

   #!/bin/bash
   echo "Current brightness:"
   rot frontlight get brightness
   echo "Setting brightness to 50:"
   rot frontlight set brightness 50

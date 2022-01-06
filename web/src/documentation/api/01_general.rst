===========
General API
===========

The general API is the main entrypoint into the rest of the API. It is
unavailable for scripting through ``rot``, as ``rot`` abstracts all
usage of the API away.

+----------------+-------------------------+-------------------------+
| Name           | Specification           | Description             |
+================+=========================+=========================+
| tarnishPid     | ``INT32`` property      | The PID of the system   |
|                | (read)                  | service. This is useful |
|                |                         | if you need to ack      |
|                |                         | ``SIGUSR1`` or          |
|                |                         | ``SIGUSR2`` signals.    |
+----------------+-------------------------+-------------------------+
| apiAvailable   | signal                  | Signal sent when an API |
|                | - (out) api             | has been successfully   |
|                | ``OBJECT_PATH``         | requested by something. |
+----------------+-------------------------+-------------------------+
| apiUnavailable | signal                  | Signal sent when an API |
|                | - (out) api             | is no longer available. |
|                | ``OBJECT_PATH``         | This can happen for one |
|                |                         | of the following        |
|                |                         | reasons:                |
|                |                         | 1. All applications     |
|                |                         | that have requested the |
|                |                         | API have released it    |
|                |                         | (or exited).            |
|                |                         | 2. Tarnish is shutting  |
|                |                         | down and releasing all  |
|                |                         | the APIs manually       |
+----------------+-------------------------+-------------------------+
| aboutToQuit    | signal                  | Signal sent when        |
|                |                         | tarnish is about to     |
|                |                         | exit. This allows       |
|                |                         | background applications |
|                |                         | to gracefully exit.     |
+----------------+-------------------------+-------------------------+
| requestAPI     | method                  | Request access to an    |
|                | - (in) name ``STRING``  | API.                    |
|                | - (out) ``OBJECT_PATH`` | Refused requests will   |
|                |                         | return an               |
|                |                         | ``OBJECT_PATH`` of      |
|                |                         | ``/``.                  |
|                |                         | Successful requests     |
|                |                         | will return the         |
|                |                         | ``OBJECT_PATH`` to the  |
|                |                         | D-Bus interface for the |
|                |                         | API.                    |
+----------------+-------------------------+-------------------------+
| releaseAPI     | method                  | Release your request    |
|                | - (in) name ``STRING``  | for access to an API.   |
|                |                         | This allows Tarnish to  |
|                |                         | unregister the API if   |
|                |                         | nothing is using it.    |
+----------------+-------------------------+-------------------------+
| APIs           | method                  | Request the list of     |
|                | - (out)                 | available APIs on the   |
|                | ``ARRA                  | system.                 |
|                | Y{STRING OBJECT_PATH}`` |                         |
+----------------+-------------------------+-------------------------+

.. _example-usage-1:

Example Usage:
~~~~~~~~~~~~~~

.. code:: cpp

   #include "dbussettings.h"
   #include "dbusservice_interface.h"
   #include "systemapi_interface.h"

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       Q_UNUSED(argc);
       Q_UNUSED(argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Tarnish PID:" << api.tarnishPid();
       qDebug() << "Requesting system API...";
       QDBusObjectPath path = api.requestAPI("system");
       if(path.path() == "/"){
           qDebug() << "Unable to get system API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the system API!";

       qDebug() << "Releasing system API...";
       api.releaseAPI("system");
       return EXIT_SUCCESS;
   }


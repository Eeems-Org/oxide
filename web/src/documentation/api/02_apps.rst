========
Apps API
========

+-------------------------+-------------------------------+----------------------+
| Name                    | Specification                 | Description          |
+=========================+===============================+======================+
| startupApplication      | ``OBJECT_PATH``               | The default          |
|                         | property                      | application to       |
|                         | (read/write)                  | display after the    |
|                         |                               | user has logged in.  |
|                         |                               | Also known as the    |
|                         |                               | launcher.            |
+-------------------------+-------------------------------+----------------------+
| lockscreenApplication   | ``OBJECT_PATH``               | The lock screen      |
|                         | property                      | application in use.  |
|                         | (read/write)                  |                      |
+-------------------------+-------------------------------+----------------------+
| applications            | ``ARRAY{STRING OBJECT_PATH}`` | The list of all the  |
|                         |                               | applications         |
|                         | property (read)               | registered with      |
|                         |                               | tarnish.             |
+-------------------------+-------------------------------+----------------------+
| previousApplications    | ``ARRAY{STRING}``             | List of previous     |
|                         | property (read)               | applications that    |
|                         |                               | have been opened.    |
|                         |                               | This list drives the |
|                         |                               | ``p                  |
|                         |                               | reviousApplication`` |
|                         |                               | method.              |
+-------------------------+-------------------------------+----------------------+
| currentApplication      | ``OBJECT_PATH``               | The currently        |
|                         | property (read)               | running foreground   |
|                         |                               | application.         |
+-------------------------+-------------------------------+----------------------+
| runningApplications     | ``ARRAY{STRING OBJECT_PATH}`` | The list of          |
|                         | property (read)               | currently running    |
|                         |                               | applications.        |
|                         |                               | The includes all the |
|                         |                               | running background,  |
|                         |                               | and paused           |
|                         |                               | foreground           |
|                         |                               | applications.        |
+-------------------------+-------------------------------+----------------------+
| pausedApplications      | ``ARRAY{STRING                | The list of paused   |
|                         | OBJECT_PATH}``                | foreground           |
|                         | property (read)               | applications.        |
+-------------------------+-------------------------------+----------------------+
| applicationRegistered   | signal                        | Signal sent when a   |
|                         |                               | new application is   |
|                         | - (out) ``OBJECT_PATH``       | registered with      |
|                         |                               | tarnish.             |
+-------------------------+-------------------------------+----------------------+
| applicationLaunched     | signal                        | Signal sent when an  |
|                         |                               | application has been |
|                         | - (out) ``OBJECT_PATH``       | launched.            |
+-------------------------+-------------------------------+----------------------+
| applicationUnregistered | signal                        | Signal sent when an  |
|                         |                               | application has been |
|                         | - (out) ``OBJECT_PATH``       | removed from         |
|                         |                               | tarnish.             |
+-------------------------+-------------------------------+----------------------+
| applicationPaused       | signal                        | Signal sent when an  |
|                         |                               | application has been |
|                         | - (out) ``OBJECT_PATH``       | paused.              |
+-------------------------+-------------------------------+----------------------+
| applicationResumed      | signal                        | Signal sent when an  |
|                         |                               | application has been |
|                         | - (out) ``OBJECT_PATH``       | resumed.             |
+-------------------------+-------------------------------+----------------------+
| applicationExited       | signal                        | Signal sent when an  |
|                         |                               | application has      |
|                         | - (out) ``OBJECT_PATH``       | exited.              |
|                         | - (out) ``INT32``             | The second output    |
|                         |                               | parameter contains   |
|                         |                               | the exit code.       |
+-------------------------+-------------------------------+----------------------+
| openDefaultApplication  | method                        | Launch or resume the |
|                         |                               | application defined  |
|                         |                               | by                   |
|                         |                               | ``                   |
|                         |                               | startupApplication`` |
+-------------------------+-------------------------------+----------------------+
| openTaskManager         | method                        | Launch or resume the |
|                         |                               | process manager.     |
+-------------------------+-------------------------------+----------------------+
| openLockScreen          | method                        | Launch or resume the |
|                         |                               | application defined  |
|                         |                               | by                   |
|                         |                               | ``loc                |
|                         |                               | kscreenApplication`` |
+-------------------------+-------------------------------+----------------------+
| registerApplication     | method                        | Register a new       |
|                         |                               | application with     |
|                         | - (in) properties             | tarnish.             |
|                         |   ``ARRAY{STRING VARIANT}``   |                      |
|                         | - (out) ``OBJECT_PATH``       |                      |
|                         |                               |                      |
+-------------------------+-------------------------------+----------------------+
| unregisterApplication   | method                        | Remove an            |
|                         |                               | application from     |
|                         | - (out) ``OBJECT_PATH``       | tarnish.             |
+-------------------------+-------------------------------+----------------------+
| reload                  | method                        | Reload applications  |
|                         |                               | registered with      |
|                         |                               | tarnish from disk.   |
+-------------------------+-------------------------------+----------------------+
| getApplicationPath      | method                        | Returns the D-Bus    |
|                         |                               | path for an          |
|                         | - (in) name ``STRING``        | application based on |
|                         | - (out) ``OBJECT_PATH``       | it's name.           |
|                         |                               | Will return ``/`` if |
|                         |                               | the application does |
|                         |                               | not exist.           |
+-------------------------+-------------------------------+----------------------+
| previousApplication     | method                        | Launch or resume the |
|                         |                               | previous application |
|                         | - (out) ``BOOLEAN``           | from                 |
|                         |                               | ``pre                |
|                         |                               | viousApplications``. |
|                         |                               | Will also remove the |
|                         |                               | application from the |
|                         |                               | list.                |
+-------------------------+-------------------------------+----------------------+

.. _example-usage-2:

Example Usage
~~~~~~~~~~~~~

.. code:: cpp

   #include <liboxide/dbus.h>

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       Q_UNUSED(argc);
       Q_UNUSED(argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting apps API...";
       QDBusObjectPath path = api.requestAPI("apps");
       if(path.path() == "/"){
           qDebug() << "Unable to get apps API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the apps API!";

       Apps appsApi(OXIDE_SERVICE, path.path(), bus);
       qDebug() << "Available applications" << appsApi.applications().keys();
       qDebug() << "Running applications" << appsApi.runningApplications().keys();
       return EXIT_SUCCESS;
   }

.. code:: shell

   #!/bin/bash
   echo "Available applications"
   rot apps get applications | jq 'keys'
   echo "Running applications"
   rot apps get runningApplications | jq 'keys'

Application Object
~~~~~~~~~~~~~~~~~~

+----------------------+-----------------------------+----------------------+
| Name                 | Specification               | Description          |
+======================+=============================+======================+
| name                 | ``STRING`` property         | Unique name used to  |
|                      | (read)                      | reference the        |
|                      |                             | application.         |
+----------------------+-----------------------------+----------------------+
| processId            | ``INT32`` property          | Process Id of the    |
|                      | (read)                      | application if it's  |
|                      |                             | running.             |
|                      |                             | Will return ``0`` if |
|                      |                             | the application is   |
|                      |                             | not running.         |
+----------------------+-----------------------------+----------------------+
| permissions          | ``ARRAY STRING``            | List of permissions  |
|                      | property                    | that the process     |
|                      | (read/write)                | has.                 |
+----------------------+-----------------------------+----------------------+
| displayName          | ``STRING`` property         | Name for the         |
|                      | (read/write)                | application to       |
|                      |                             | display to the user. |
+----------------------+-----------------------------+----------------------+
| description          | ``STRING`` property         | Description of the   |
|                      | (read/write)                | application.         |
+----------------------+-----------------------------+----------------------+
| bin                  | ``STRING`` property         | Path to the binary   |
|                      | (read)                      | file used to launch  |
|                      |                             | the application.     |
+----------------------+-----------------------------+----------------------+
| onPause              | ``STRING`` property         | Simple script to run |
|                      | (read/write)                | when pausing the     |
|                      |                             | application.         |
+----------------------+-----------------------------+----------------------+
| onResume             | ``STRING`` property         | Simple script to run |
|                      | (read/write)                | when resuming the    |
|                      |                             | application.         |
+----------------------+-----------------------------+----------------------+
| onStop               | ``STRING`` property         | Simple script to run |
|                      | (read/write)                | when stopping the    |
|                      |                             | application.         |
+----------------------+-----------------------------+----------------------+
| autoStart            | ``BOOLEAN`` property        | If this application  |
|                      | (read/write)                | should be            |
|                      |                             | automatically        |
|                      |                             | started when tarnish |
|                      |                             | starts up.           |
+----------------------+-----------------------------+----------------------+
| type                 | ``INT32`` property          | Type of application. |
|                      | (read)                      | - ``0`` Foreground   |
|                      |                             | application          |
|                      |                             | - ``1`` Background   |
|                      |                             | application          |
|                      |                             | - ``2``              |
|                      |                             | Backgroundable       |
|                      |                             | application          |
+----------------------+-----------------------------+----------------------+
| state                | ``INT32`` property          | Current state of the |
|                      | (read)                      | application.         |
|                      |                             | - ``0`` Inactive     |
|                      |                             | - ``1`` Application  |
|                      |                             | is in the Foreground |
|                      |                             | - ``2`` Application  |
|                      |                             | is in the Background |
|                      |                             | - ``3`` Application  |
|                      |                             | is paused            |
+----------------------+-----------------------------+----------------------+
| systemApp            | ``BOOLEAN`` property        | If this application  |
|                      | (read)                      | is a system app or   |
|                      |                             | not.                 |
+----------------------+-----------------------------+----------------------+
| hidden               | ``BOOLEAN`` property        | If this application  |
|                      | (read)                      | should be hidden     |
|                      |                             | from the user on any |
|                      |                             | UI.                  |
+----------------------+-----------------------------+----------------------+
| icon                 | ``STRING`` property         | Path to the icon     |
|                      | (read/write)                | used to represent    |
|                      |                             | this application.    |
+----------------------+-----------------------------+----------------------+
| environment          | ``AR                        | Map of environment   |
|                      | RAY{STRING STRING}``        | variables to set for |
|                      | property (read)             | the process.         |
+----------------------+-----------------------------+----------------------+
| workingDirectory     | ``STRING`` property         | Directory to set as  |
|                      | (read/write)                | the current working  |
|                      |                             | directory for the    |
|                      |                             | application.         |
+----------------------+-----------------------------+----------------------+
| user                 | ``STRING`` property         | User the application |
|                      | (read)                      | will be run as.      |
+----------------------+-----------------------------+----------------------+
| group                | ``STRING`` property         | Group the            |
|                      | (read)                      | application will be  |
|                      |                             | run as.              |
+----------------------+-----------------------------+----------------------+
| launched             | signal                      | Signal sent when the |
|                      |                             | application starts.  |
+----------------------+-----------------------------+----------------------+
| paused               | signal                      | Signal sent when the |
|                      |                             | application is       |
|                      |                             | paused.              |
+----------------------+-----------------------------+----------------------+
| resumed              | signal                      | Signal sent when the |
|                      |                             | application is       |
|                      |                             | resumed.             |
+----------------------+-----------------------------+----------------------+
| unregistered         | signal                      | Signal sent when the |
|                      |                             | application is       |
|                      |                             | removed from         |
|                      |                             | tarnish.             |
+----------------------+-----------------------------+----------------------+
| exited               | signal                      | Signal sent when the |
|                      |                             | application exits.   |
|                      | - (out) ``INT32``           | First signal         |
|                      |                             | parameter is the     |
|                      |                             | exit code of the     |
|                      |                             | application.         |
+----------------------+-----------------------------+----------------------+
| permissionsChanged   | signal                      | Signal sent when the |
|                      |                             | permissions of the   |
|                      | - (out)                     | application changes. |
|                      |   ``ARRAY STRING``          |                      |
+----------------------+-----------------------------+----------------------+
| displayNameChanged   | signal                      | Signal sent when the |
|                      |                             | displayName of the   |
|                      | - (out) ``STRING``          | application changes. |
+----------------------+-----------------------------+----------------------+
| onPauseChanged       | signal                      | Signal sent when the |
|                      |                             | onPause of the       |
|                      | - (out) ``STRING``          | application changes. |
+----------------------+-----------------------------+----------------------+
| onResumeChanged      | signal                      | Signal sent when the |
|                      |                             | onResume of the      |
|                      | - (out) ``STRING``          | application changes. |
+----------------------+-----------------------------+----------------------+
| onStopChanged        | signal                      | Signal sent when the |
|                      | - (out) ``STRING``          | onStop of the        |
|                      |                             | application changes. |
+----------------------+-----------------------------+----------------------+
| autoStartChanged     | signal                      | Signal sent when     |
|                      |                             | autoStart for the    |
|                      | - (out) ``BOOLEAN``         | application chagnes. |
+----------------------+-----------------------------+----------------------+
| iconChanged          | signal                      | Signal sent when the |
|                      |                             | icon of the          |
|                      | - (out) ``STRING``          | application changes. |
+----------------------+-----------------------------+----------------------+
| environmentChanged   | signal                      | Signal sent when the |
|                      |                             | environment of the   |
|                      | - (out)                     | application changes. |
|                      |   ``ARRAY STRING``          |                      |
+----------------------+-----------------------------+----------------------+
| wor                  | signal                      | Signal sent when the |
| kingDirectoryChanged |                             | working directory of |
|                      | - (out) ``STRING``          | the application      |
|                      |                             | changes.             |
+----------------------+-----------------------------+----------------------+
| directoriesChanged   | signal                      | Signal sent when the |
|                      |                             | directories of the   |
|                      | - (out)                     | application changes. |
|                      |   ``ARRAY STRING``          |                      |
+----------------------+-----------------------------+----------------------+
| launch               | method                      | Launch or resume the |
|                      |                             | application.         |
+----------------------+-----------------------------+----------------------+
| pause                | method                      | Pause the            |
|                      |                             | application.         |
|                      |                             | If the application   |
|                      |                             | is backgroundable it |
|                      |                             | will be moved into   |
|                      |                             | the background.      |
+----------------------+-----------------------------+----------------------+
| resume               | method                      | Resume an            |
|                      |                             | application.         |
|                      |                             | If the application   |
|                      |                             | is backgroundable    |
|                      |                             | and in the           |
|                      |                             | background it will   |
|                      |                             | be moved into the    |
|                      |                             | foreground.          |
+----------------------+-----------------------------+----------------------+
| stop                 | method                      | Stop the             |
|                      |                             | application.         |
+----------------------+-----------------------------+----------------------+
| unregister           | method                      | Remove the           |
|                      |                             | application from     |
|                      |                             | tarnish.             |
+----------------------+-----------------------------+----------------------+
| setEnvironment       | method                      | Change the           |
|                      |                             | environment of the   |
|                      | - (in) environment          | application.         |
|                      |   ``ARRAY{STRING VARIANT}`` | Changes will be      |
|                      |                             | applied after the    |
|                      |                             | application          |
|                      |                             | restarts.            |
+----------------------+-----------------------------+----------------------+

.. _example-usage-3:

Example Usage
^^^^^^^^^^^^^

.. code:: cpp

   #include <liboxide/dbus.h>

   using namespace codes::eeems::oxide1;

   int main(int argc, char* argv[]){
       Q_UNUSED(argc);
       Q_UNUSED(argv);

       auto bus = QDBusConnection::systemBus();
       General api(OXIDE_SERVICE, OXIDE_SERVICE_PATH, bus);
       qDebug() << "Requesting apps API...";
       QDBusObjectPath path = api.requestAPI("apps");
       if(path.path() == "/"){
           qDebug() << "Unable to get apps API";
           return EXIT_FAILURE;
       }
       qDebug() << "Got the apps API!";

       Apps appsApi(OXIDE_SERVICE, path.path(), bus);
       path = appsApi.currentApplication();
       Application app(OXIDE_SERVICE, path.path(), bus);
       qDebug() << "Current application:" << app.displayName();
       QString name("test");
       qDebug() << "Removing application: " << name;
       path = appsApi.getApplicationPath(name);
       if(path.path() == "/"){
            qDebug() << "Failed to find application: " << name;
            return EXIT_FAILURE;
       }
       if(!appsApi.unregisterApplication(path)){
            qDebug() << "Failed to unregister application: " << name;
            return EXIT_FAILURE;
       }
       return EXIT_SUCCESS;
   }

.. code:: shell

   #!/bin/bash
   echo -n "Current application: "
   rot apps get currentApplication \
     | jq -cr | sed 's|/codes/eeems/oxide1/||' \
     | xargs -I {} rot --object Application:{} apps get displayName \
     | jq -cr
   
   name="test"
   echo "Removing Application: $name"
   rot apps call getApplicationPath "QString:\"$name\"" \
     | jq -cr | sed 's|/codes/eeems/oxide1/||' \
     | xargs -I {} rot --object Application:{} apps call unregister

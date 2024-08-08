======================================
Application Registration Specification
======================================

Oxide application registration files are used to indicate to the Oxide
system service (tarnish) that an application is installed on the system.
It's meant to facilitate package managers adding applications and having
them show up in the launcher while tarnish is not running. These files
must have a file extension of ``oxide`` and must be located in
``/opt/usr/share/applications``. The file format is currently JSON.
Below is an example file:

.. code:: json

   {
     "displayName": "Sample application",
     "description": "This is a sample application",
     "bin": "/opt/bin/sample-application",
     "flags": ["chroot"],
     "type": "foreground",
     "permissions": ["power"],
     "directories": ["/home/root"],
     "workingDirectory": "/home/root",
     "environment": {
       "QDBUS_DEBUG": "1"
     },
     "events": {
       "resume": "touch /tmp/.sample-running",
       "pause": "rm /tmp/.sample-running",
       "stop": "rm /tmp/.sample-running"
     }
   }

Properties
==========

bin
---
Required string

The path to the application. If this file doesn't exist, the application will not be registered.

type
----
Required string

The type of application this is.

- ``"background"``: This application runs in the background and does not display anything on the screen.
- ``"foreground"``: This application runs in the foreground and must be paused when it's not the active application.
- ``"backgroundable"``: This application runs in both the foreground and the background and supports being notified with ``SIGUSR1`` and ``SIGUSR2`` when being swapped between the two states by the user.

flags
-----
Optional string array

An array of flags for the application.

- ``"hidden"``: Do not display this application to the user.
- ``"autoStart"``: Start this application automatically when tarnish starts up. Should only be used on applications with a type of ``"backgroundable"`` or ``"background"``.
- ``"system"``: This is a system application and cannot be removed by the user. This is set by default as applications using an application registration file are intended to be removed by a package manager.
- ``"nopreload"``: Do not automatically add any of the preload libraries to ``LD_PRELOAD`` when running this application.
- ``"nopreload.compositor"``: Do not automatically add ``libblight_client.so`` to ``LD_PRELOAD`` when running this application.
- ``"nopreload.sysfs"``: Do not automatically add ``libsysfs_preload.so`` to ``LD_PRELOAD`` when running this application.
- ``"exclusive"``: Application is to be run in exclusive mode. This means that the compositor will not be sending input to the application, or handling displaying anything to the screen while this application is in the foreground.

displayName
-----------

Optional string

The human readable name to display to the user.

description
-----------

Optional string

Description of the app meant to be displayed to the user.

icon
----

Optional string

Path to an image file to use as the icon for this application, or an :ref:`icon-spec`.

user
----

Optional string

User to run this application as.

Default is ``root``.

group
-----

Optional string

Group to run this application in.

Default is ``root``.

workingDirectory
----------------

Optional string

Directory to set as the current working directory of the application.

Default is ``/``.

permissions
-----------

Optional string array

API permissions to grant this application. Without these, any calls to the API will be refused or return incorrect results.

- ``"permissions"``: Application can modify applications settings/permissions in the Apps API.
- ``"apps"``: Apps API access
- ``"notification"``: Notification API access
- ``"power"``: Power API access
- ``"screen"``: Screen API access
- ``"system"``: System API access
- ``"wifi"``: Wifi API access

environment
-----------

Optional string map

Extra environment variables to set before running the application.

events
------

Optional string map

Custom commands to run on an event.

- ``"resume"``: Run when the application is resumed
- ``"pause"``: Run when the application is pause
- ``"stop"``: Run when the application is stopped

.. _icon-spec:

Icon Spec
=========

Icon specifications can be in the following format: ``[theme:][context:]{name}-{size}``

Some examples:

- ``oxide:splash:oxide-702``
- ``oxide:apps:xochitl-48``
- ``oxide:xochitl-48``
- ``xochitl-48``

You can find available icons in ``/opt/usr/share/icons``. The default theme is
hicolor, and the default context is apps.

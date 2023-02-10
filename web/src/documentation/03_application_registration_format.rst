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

+------------------+--------------+----------+-----------------------+
| Property         | Type         | Required | Description           |
+==================+==============+==========+=======================+
| bin              | string       | Yes      | The path to the       |
|                  |              |          | application. If       |
|                  |              |          | this file doesn't     |
|                  |              |          | exist, the            |
|                  |              |          | application will      |
|                  |              |          | not be registered.    |
+------------------+--------------+----------+-----------------------+
| type             | string       | Yes      | The type of           |
|                  |              |          | application this      |
|                  |              |          | is.                   |
|                  |              |          |                       |
|                  |              |          | ``"background"``:     |
|                  |              |          | This application      |
|                  |              |          | runs in the           |
|                  |              |          | background and        |
|                  |              |          | does not display      |
|                  |              |          | anything on the       |
|                  |              |          | screen.               |
|                  |              |          |                       |
|                  |              |          | ``"foreground"``:     |
|                  |              |          | This application      |
|                  |              |          | runs in the           |
|                  |              |          | foreground and        |
|                  |              |          | must be paused        |
|                  |              |          | when it's not the     |
|                  |              |          | active                |
|                  |              |          | application.          |
|                  |              |          |                       |
|                  |              |          | ``"backgroundable"``: |
|                  |              |          | This application      |
|                  |              |          | runs in both the      |
|                  |              |          | foreground and the    |
|                  |              |          | background and        |
|                  |              |          | supports being        |
|                  |              |          | notified with         |
|                  |              |          | ``SIGUSR1`` and       |
|                  |              |          | ``SIGUSR2`` when      |
|                  |              |          | being swapped         |
|                  |              |          | between the two       |
|                  |              |          | states by the         |
|                  |              |          | user.                 |
+------------------+--------------+----------+-----------------------+
| flags            | string array | No       | An array of flags     |
|                  |              |          | for the               |
|                  |              |          | application.          |
|                  |              |          |                       |
|                  |              |          | ``"hidden"``:         |
|                  |              |          | Do not display        |
|                  |              |          | this application      |
|                  |              |          | to the                |
|                  |              |          | user.                 |
|                  |              |          |                       |
|                  |              |          | ``"autoStart"``:      |
|                  |              |          | Start this            |
|                  |              |          | application           |
|                  |              |          | automatically when    |
|                  |              |          | tarnish starts up.    |
|                  |              |          | Should only be        |
|                  |              |          | used on               |
|                  |              |          | applications with     |
|                  |              |          | a type of             |
|                  |              |          | ``"backgroundable"``  |
|                  |              |          | or                    |
|                  |              |          | ``"background"``.     |
|                  |              |          |                       |
|                  |              |          | ``"chroot"``:         |
|                  |              |          | Run this              |
|                  |              |          | application in a      |
|                  |              |          | chroot.               |
|                  |              |          |                       |
|                  |              |          | ``"system"``:         |
|                  |              |          | This is a system      |
|                  |              |          | application and       |
|                  |              |          | cannot be removed     |
|                  |              |          | by the user. This     |
|                  |              |          | is set by default     |
|                  |              |          | as applications       |
|                  |              |          | using an              |
|                  |              |          | application           |
|                  |              |          | registration file     |
|                  |              |          | are intended to be    |
|                  |              |          | removed by a          |
|                  |              |          | package manager.      |
+------------------+--------------+----------+-----------------------+
| displayName      | string       | No       | The human readable    |
|                  |              |          | name to display to    |
|                  |              |          | the user.             |
+------------------+--------------+----------+-----------------------+
| description      | string       | No       | Description of the    |
|                  |              |          | app meant to be       |
|                  |              |          | displayed to the      |
|                  |              |          | user.                 |
+------------------+--------------+----------+-----------------------+
| icon             | string       | No       | Path to an image      |
|                  |              |          | file to use as the    |
|                  |              |          | icon for this         |
|                  |              |          | application. Or an    |
|                  |              |          | icon spec.            |
+------------------+--------------+----------+-----------------------+
| splash           | string       | No       | Path to an image      |
|                  |              |          | file to use as the    |
|                  |              |          | splash screen for     |
|                  |              |          | this application. Or  |
|                  |              |          | an icon spec.         |
+------------------+--------------+----------+-----------------------+
| user             | string       | No       | User to run this      |
|                  |              |          | application as.       |
|                  |              |          | Default is            |
|                  |              |          | ``root``.             |
+------------------+--------------+----------+-----------------------+
| group            | string       | No       | Group to run this     |
|                  |              |          | application in.       |
|                  |              |          | Default is            |
|                  |              |          | ``root``.             |
+------------------+--------------+----------+-----------------------+
| workingDirectory | string       | No       | Directory to set      |
|                  |              |          | as the current        |
|                  |              |          | working directory     |
|                  |              |          | of the                |
|                  |              |          | application.          |
|                  |              |          | Default is ``/``.     |
+------------------+--------------+----------+-----------------------+
| directories      | string array | No       | When an               |
|                  |              |          | application is        |
|                  |              |          | running in a          |
|                  |              |          | chroot, also map      |
|                  |              |          | these directories     |
|                  |              |          | into the chroot       |
|                  |              |          | with read/write       |
|                  |              |          | privileges.           |
+------------------+--------------+----------+-----------------------+
| permissions      | string array | No       | API permissions to    |
|                  |              |          | grant this            |
|                  |              |          | application.          |
|                  |              |          | Without these, any    |
|                  |              |          | calls to the API      |
|                  |              |          | will be refused or    |
|                  |              |          | return incorrect      |
|                  |              |          | results.              |
|                  |              |          |                       |
|                  |              |          | ``"permissions"``:    |
|                  |              |          | Application can       |
|                  |              |          | modify                |
|                  |              |          | applications          |
|                  |              |          | se                    |
|                  |              |          | ttings/permissions    |
|                  |              |          | in the Apps API.      |
|                  |              |          |                       |
|                  |              |          | ``"apps"``:           |
|                  |              |          | Apps API              |
|                  |              |          | access                |
|                  |              |          |                       |
|                  |              |          | ``"notification"``:   |
|                  |              |          | Notification API      |
|                  |              |          | access                |
|                  |              |          |                       |
|                  |              |          | ``"power"``:          |
|                  |              |          | Power API             |
|                  |              |          | access                |
|                  |              |          |                       |
|                  |              |          | ``"screen"``:         |
|                  |              |          | Screen API            |
|                  |              |          | access                |
|                  |              |          |                       |
|                  |              |          | ``"system"``:         |
|                  |              |          | System API            |
|                  |              |          | access                |
|                  |              |          |                       |
|                  |              |          | ``"wifi"``:           |
|                  |              |          | Wifi API access       |
+------------------+--------------+----------+-----------------------+
| environment      | string map   | No       | Extra environment     |
|                  |              |          | variables to set      |
|                  |              |          | before running the    |
|                  |              |          | application.          |
+------------------+--------------+----------+-----------------------+
| events           | string map   | No       | Custom commands to    |
|                  |              |          | run on an             |
|                  |              |          | event.                |
|                  |              |          |                       |
|                  |              |          | ``"resume"``:         |
|                  |              |          | Run when the          |
|                  |              |          | application is        |
|                  |              |          | resumed               |
|                  |              |          |                       |
|                  |              |          | ``"pause"``:          |
|                  |              |          | Run when the          |
|                  |              |          | application is        |
|                  |              |          | pause                 |
|                  |              |          |                       |
|                  |              |          | ``"stop"``:           |
|                  |              |          | Run when the          |
|                  |              |          | application is        |
|                  |              |          | stopped               |
+------------------+--------------+----------+-----------------------+

Icon Spec
=========

Icon specifications can be in the following format: ``[theme:][context:]{name}-{size}``

Some examples:

- ``oxide:splash:xochitl-702``
- ``oxide:apps:xochitl-48``
- ``oxide:xochitl-48``
- ``xochitl-48``

You can find available icons in ``/opt/usr/share/icons``. The default theme is
hicolor, and the default context is apps.

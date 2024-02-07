==============
Compositor API
==============

This API handles communication with the display server. If possible it
is recommended to use `use libblight instead <../../libblight/index.html>`__

+-------------------+------------------------+-----------------------------------+
| Name              | Specification          | Description                       |
+===================+========================+===================================+
| pid               | ``INT32`` property     | Get the PID of the display server |
|                   | (read)                 | process.                          |
+-------------------+------------------------+-----------------------------------+
| clipboard         | ``ARRAY BYTE``         | Get the contents of the clipboard |
|                   | property (read)        |                                   |
+-------------------+------------------------+-----------------------------------+
| selection         | ``ARRAY BYTE``         | Get the contents of the primary   |
|                   | property (read)        | selection                         |
+-------------------+------------------------+-----------------------------------+
| secondary         | ``ARRAY BYTE``         | Get the contents of the secondary |
|                   | property (read)        | selection                         |
+-------------------+------------------------+-----------------------------------+
| clipboardChanged  | signal                 | The clipboard contents changed    |
|                   |                        |                                   |
|                   | - (out) ``ARRAY BYTE`` |                                   |
+-------------------+------------------------+-----------------------------------+
| selectionChanged  | signal                 | The primary selection contents    |
|                   |                        | changed                           |
|                   | - (out) ``ARRAY BYTE`` |                                   |
+-------------------+------------------------+-----------------------------------+
| secondaryChanged  | signal                 | The secondary selection contents  |
|                   |                        | changed                           |
|                   | - (out) ``ARRAY BYTE`` |                                   |
+-------------------+------------------------+-----------------------------------+
| open              | method                 | Get the socket descriptor for the |
|                   |                        | current process' display server   |
|                   | - (out) ``UNIX_FD``    | connection                        |
+-------------------+------------------------+-----------------------------------+
| openInput         | method                 | Get the socket descriptor for the |
|                   |                        | current process' input event      |
|                   | - (out) ``UNIX_FD``    | stream                            |
|                   |                        |                                   |
+-------------------+------------------------+-----------------------------------+
| addSurface        | method                 | Add a surface to the display      |
|                   |                        | server                            |
|                   | - (out) ``UINT16``     |                                   |
|                   | - (in) fd ``UNIX_FD``  |                                   |
|                   | - (in) x ``INT32``     |                                   |
|                   | - (in) y ``INT32``     |                                   |
|                   | - (in) width ``INT32`` |                                   |
|                   | - (in) height ``INT32``|                                   |
|                   | - (in) stride ``INT32``|                                   |
|                   | - (in) format ``INT32``|                                   |
+-------------------+------------------------+-----------------------------------+
| repaint           | method                 | Repaint a surface or all the      |
|                   |                        | surfaces for a connection.        |
|                   | - (in)  ``STRING``     |                                   |
+-------------------+------------------------+-----------------------------------+
| getSurface        | method                 | Get the file descriptor for the   |
|                   |                        | buffer of a surface               |
|                   | - (in) identifier      |                                   |
|                   |    ``UINT16``          |                                   |
|                   | - (out) ``UINT16``     |                                   |
+-------------------+------------------------+-----------------------------------+
| setFlags          | method                 | Set flags on a surface or         |
|                   |                        | connection                        |
|                   | - (in) identifier      |                                   |
|                   |    ``STRING``          |                                   |
|                   | - (in) flags           |                                   |
|                   |   ``STRING ARRAY``     |                                   |
+-------------------+------------------------+-----------------------------------+
| getSurfaces       | method                 | Get a list of all surfaces        |
|                   |                        | currently on the display server   |
|                   | - (out) ``STRING       |                                   |
|                   |   ARRAY``              |                                   |
+-------------------+------------------------+-----------------------------------+
| frameBuffer       | method                 | Get the file descriptor of the    |
|                   |                        | device frame buffer               |
|                   | - (out) ``UNIX_FD``    |                                   |
+-------------------+------------------------+-----------------------------------+
| lower             | method                 | Lower a surface or all the        |
|                   |                        | surfaces for a connection.        |
|                   | - (in)  ``STRING``     |                                   |
+-------------------+------------------------+-----------------------------------+
| raise             | method                 | Raise a surface or all the        |
|                   |                        | surfaces for a connection.        |
|                   | - (in)  ``STRING``     |                                   |
+-------------------+------------------------+-----------------------------------+
| focus             | method                 | Focus a connection so that it     |
|                   |                        | recieves input events             |
|                   | - (in)  ``STRING``     |                                   |
+-------------------+------------------------+-----------------------------------+
| focus             | method                 | Focus a connection so that it     |
+-------------------+------------------------+-----------------------------------+
| waitForNoRepaints | method                 | Wait for all pending repaints to  |
|                   |                        | complete                          |
+-------------------+------------------------+-----------------------------------+
|enterExclusiveMode | method                 | Enter exlcusive mode              |
+-------------------+------------------------+-----------------------------------+
| exitExclusiveMode | method                 | Exit exclusive move               |
+-------------------+------------------------+-----------------------------------+
|exlusiveModeRepaint| method                 | Repaint the framebuffer           |
+-------------------+------------------------+-----------------------------------+

.. _example-usage-11:

Example Usage
~~~~~~~~~~~~~

.. code:: cpp

   #include <QDebug>
   #include <liboxide/dbus.h>

   using namespace codes::eeems::blight1;

   int main(int argc, char* argv[]){
       auto bus = QDBusConnection::systemBus();
       Compositor compositor(BLIGHT_SERVICE, "/", bus);
       qDebug() << "Display server PID:" << compositor.pid();
       return EXIT_SUCCESS;
   }

.. code:: shell

   #!/bin/bash
   echo "Display server PID: $(rot compositor get pid)"
   echo "Display server surfaces:"
   rot compositor call getSurfaces | jq -r '.[]'

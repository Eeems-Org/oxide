========
Overview
========

You can find the DBus API Specification
`here <https://gist.github.com/Eeems/728d4ec836b156d880ce521ab50e5d40#file-01-overview-md>`__.
While this was the initial specification that was used for development,
the actual implementation may not have been completely faithful to it
due to technical issues that had to be resolved. You can find the
technical DBus specification
`here <https://github.com/Eeems/oxide/tree/master/interfaces>`_, and
examples of it's consumption in some of the embedded applications (e.g.
`fret <https://github.com/Eeems/oxide/tree/master/applications/screenshot-tool>`_)

Further reading:

-  https://eeems.website/writing-a-simple-oxide-application/

-  https://dbus.freedesktop.org/doc/dbus-specification.html

Options for interacting with the API
====================================

There are a number of options for interacting with the API.

  1. Using the ``rot`` command line tool and :doc:`../02_oxide-utils`.
  2. Making calls using `dbus-send <https://man.archlinux.org/man/dbus-send.1.en>`_.
  3. Generating classes using the
     `automatic interface xml files <https://github.com/Eeems/oxide/tree/master/interfaces>`_.
  4. Making D-Bus calls manually in your language of choice.
  5. Using the python wrapper for the ``rot`` command line `python-liboxide <https://github.com/Eeems-Org/python-liboxide>`_

For the purposes of this documentation we will document using :ref:`rot` as
well as using Qt generated classes.

Liboxide Shared Library
=======================

Oxide also produces a shared library that can be used for application development. This library does
not yet fully encompass the full API provide through D-Bus, but will slowly work towards feature parity.

`You can find the documentation here <../../liboxide/index.html>`__


Libblight Shared Library
========================

Oxide provides a shared library for interacting with the display server.

`You can find the documentation here <../../libblight/index.html>`__

Oxide Qt Platform Abstraction (QPA)
===================================

Oxide ships a QPA for Qt applications to use. This will automatically use the display server for updating the screen, as well as user input.

If you are using liboxide, a call to `deviceSettings.setupQtEnvironment()` will setup the application to use the Oxide QPA. To manualy use the QPA, you will need to use the following code:

.. code:: cpp

   void setup(){
     QCoreApplication::addLibraryPath("/opt/usr/lib/plugins");
     qputenv("QMLSCENE_DEVICE", "software");
     qputenv("QT_QUICK_BACKEND","software");
     QString platform(
       "oxide:enable_fonts:freetype:freetype:rotate=180"
     );
     if(is_on_rm2){
       platform += ":invertx"
     }
     qputenv("QT_QPA_PLATFORM", platform);
   }

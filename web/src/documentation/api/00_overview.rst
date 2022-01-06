========
Overview
========

You can find the DBus API Specification
`here <https://gist.github.com/Eeems/728d4ec836b156d880ce521ab50e5d40#file-01-overview-md>`__.
While this was the initial specification that was used for development,
the actual implementation may not have been completely faithful to it
due to technical issues that had to be resolved. You can find the
technical DBus specification
`https://github.com/Eeems/oxide/tree/master/interfaces <here>`__, and
examples of it's consumption in some of the embedded applications (e.g.
`fret <https://github.com/Eeems/oxide/tree/master/applications/screenshot-tool>`__)

Further reading:

-  https://eeems.website/writing-a-simple-oxide-application/

-  https://dbus.freedesktop.org/doc/dbus-specification.html

Options for interacting with the API
====================================

There are a number of options for interacting with the API.

.. code:: 

   1. Using the `rot` command line tool.
   2. Generating classes using the automatic interface xml files.
   3. Making calls using dbus-send.
   4. Making D-Bus calls manually in your language of choice.

For the purposes of this documentation we will document using ``rot`` as
well as using Qt generated classes.


===========
Oxide-Utils
===========

As of version 2.6, Oxide ships with several new command line utilities meant to mimic common
existing linux command line tools meant for dealing with desktop environments. ``notify-send``
was added in version 2.5.

desktop-file-validate
=====================

https://man.archlinux.org/man/desktop-file-validate.1.en

update-desktop-database
=======================

https://man.archlinux.org/man/update-desktop-database.1.en

xdg-desktop-menu
================

https://man.archlinux.org/man/xdg-desktop-menu.1

xdg-desktop-icon
================

https://man.archlinux.org/man/xdg-desktop-icon.1

xdg-open
========

https://man.archlinux.org/man/xdg-open.1

xdg-settings
============

https://man.archlinux.org/man/xdg-settings.1.en

gio
===

https://man.archlinux.org/man/gio.1

notify-send
===========

https://man.archlinux.org/man/notify-send.1.en

xclip
=====

https://man.archlinux.org/man/xclip.1.en

inject_evdev
============

This application allows you to script input events. This can be used for things like injecting screen swipes from a script. It functions by taking a string on stdin and parsing the `linux event codes <https://www.kernel.org/doc/html/v5.4/input/event-codes.html>`_ and then passing them to the specified input device.

.. code:: shell

  Usage: inject_evdev [options] device
  Inject evdev events.

  Options:
    -h, --help     Displays help on commandline options.
    --help-all     Displays help including Qt specific options.
    -v, --version  Displays version information.

  Arguments:
    device         Device to emit events to. See /dev/input for possible values.


Example usage:

.. code:: bash

  inject_evdev event2 << EOF
  EV_ABS ABS_MT_TRACKING_ID 123
  EV_SYN SYN_MT_REPORT
  EV_SYN SYN_REPORT
  EV_ABS ABS_MT_TRACKING_ID -1
  EV_SYN SYN_MT_REPORT
  EV_SYN SYN_REPORT
  EOF

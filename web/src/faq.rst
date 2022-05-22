==========================
Frequently Asked Questions
==========================

What kind of telemetry do you have?
===================================

As of 2.4, Oxide has basic telemetry enabled out of box. This feature can be disabled at runtime,
or completely removed when compiling the application manually. The telemetry is used for the following:

- Automated crash reporting.
- Determining how quickly new versions are used by users.
- Determining what older versions are still in use.

What kind of information is collected by telemetry?
===================================================

- The device `machine-id <https://man7.org/linux/man-pages/man5/machine-id.5.html>`_, but anonymised through
    `sd_id128_get_boot_app_specific <https://man7.org/linux/man-pages/man3/sd_id128_get_machine_app_specific.3.html>`_.
    This is used to uniquely identify the devices connecting so that I can properly determine what
    percentage of devices are on specific versions. It can also allow me to determine what crash
    reports are sent by a user if they are willing to share the anonymised value from their devices.
- OS version via the contents of ``/etc/version`` on the device.
    This helps ensure that the OS version is properly taken into account when investigating a crash.
- Name of application and version. i.e. ``rot@2.4``
    This ensures that the correct application is investigated for a crash.
- `Minidump <https://docs.sentry.io/platforms/native/guides/minidumps/>`_ of the crash.
    This hopefully provides enough information that a crash can be replicated and the specific
    piece of code that is causing issues can be identified.

How can I disable the telemetry?
================================

.. code:: bash

  rot settings set telemetry false
  rot settings set crashReport false
  rot settings set applicationUsage false

Or you can compile the applications manually without the ``sentry`` feature enabled.

How can I get the time to display in my timezone?
=================================================

You can use `timedatectl <https://www.freedesktop.org/software/systemd/man/timedatectl.html>`_
to change your timezone. You can see available timezones by looking into ``/usr/share/zoneinfo/``.
Do not trust the output of ``timedatectl get-timezones`` as it reports more timezones than are
actually installed on the device by default. You can install more timezones on the device through
`Toltec <https://toltec-dev.org>`_ by installing one of the ``zoneinfo-*`` packages.

.. code:: bash

  timedatectl set-timezone America/Denver


I'm installing without `Toltec <https://toltec-dev.org>`_ and nothing displays?
===============================================================================

Oxide (and most other applications) on the reMarkable 2 requires
`rm2fb <https://github.com/ddvk/remarkable2-framebuffer>`_ do display to the screen.

How do I change my pin after I've set it?
=========================================

There is no way to currently trigger a pin change, but you can wipe your current pin, and trigger
the pin setting dialog by doing the following:

.. code:: bash

  systemctl stop tarnish
  rm /home/root/.config/Eeems/decay.conf
  systemctl start tarnish


Not all of my applications are listed?
======================================

Oxide doesn't import draft applications automatically, you can import them by using the menu on the
top left of the launcher. If your application is still not listed, you may need to review the device
logs to determine why it's failing to load. If an application is configured in draft to pass arguments
in the ``call=`` line, it will fail to import as this is not supported by Oxide.

How do I review my device logs?
===============================

Most logs on the device are accessable from the command line through
`journalctl <https://www.freedesktop.org/software/systemd/man/journalctl.html>`_. To get at the logs
for Oxide's programs, and any application you run through Oxide, you can run the following:

.. code:: bash

  journalctl -eau tarnish

Where are the configuration files?
==================================

The primary configuration file can be found in one of the following locations:

  1. ``/etc/oxide.conf``
  2. ``/opt/etc/oxide.conf``
  3. ``/home/root/.config/oxide.conf``

Other configuration files can be found in ``/home/root/.config/Eeems/``.

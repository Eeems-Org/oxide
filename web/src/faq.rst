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

The following types of information is collected:

- The device `machine-id <https://man7.org/linux/man-pages/man5/machine-id.5.html>`_, but anonymised through
  `sd_id128_get_boot_app_specific <https://man7.org/linux/man-pages/man3/sd_id128_get_machine_app_specific.3.html>`_
  This is used to uniquely identify the devices connecting so that I can properly determine what
  percentage of devices are on specific versions. It can also allow me to determine what crash
  reports are sent by a user if they are willing to share the anonymised value from their devices.
- OS version via the contents of ``/etc/version`` on the device.
- Name of application (rot, erode, oxide, tarnish, etc) and what version of that application is running.

How can I disable the telemetry?
================================

..code::bash

  rot settings set telemetry false

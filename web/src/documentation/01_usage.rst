=====
Usage
=====

Lockscreen (decay)
==================

If xochitl has a pin set, you will be prompted to import it. If you don't have a pin set, or decide not to import one from xochtil, you will be prompted to create a pin. You may choose to use the device with no pin.

Launcher (oxide)
================

You can access the power menu from the power button on the top right of the screen. This will allow you to put the device to sleep, or to power off the device.

The menu button on the top left side of the screen will open up the tools menu. This allows you to refresh the grid, import applications from draft, or to open up the options dialog. Importing applications will only import applications that contain valid configuration for Oxide. This means that draft configuration that attempts to pass arguments to the ``call=`` setting will not be imported.

Tapping on the wifi icon open up the wifi menu.

The options dialog will allow you to configure various settings in the application. You have the option to reload the settings from the ``conf`` file on disk, or to close the options dialog and save the current settings to disk.

Press on an application to launch, or return to an application. Long press on it to see more information about the application. If the application is running you will also be presented with a button to kill the application. There will be a button to toggle if this application should be launched on startup. This will automatically disable launch on startup from another application.

Application Switcher (corrupt)
==============================

* Swipe up from the bottom of the screen to open the application switcher.
* You can select ``Home`` to return to the launcher.
* You can select any running application to switch to that application.
* You can long press any running application to close that application.
* You can tap above the application switcher to return to the currently running application.
* You can swipe from the left edge of the screen, or long press the left button to return to the currently running application.
* You can select the left or right arrow if they are enabled to scroll through running applications.

Background Service (tarnish)
============================

* Press and hold the left button, or swipe from the left edge of the screen to return to the previous application.
* Press and hold the middle button to launch the process manager.
* Press the power button to suspend the device.
* Press and hold the right button, or swipe from the right edge of the screen to take a screenshot.
* Swipe from the top of the screen to enable/disable swipe gestures.

Process Manager (erode)
=======================

* There is a reload button on the top right of the screen that will refresh the list.
* You can click on the column headers to sort the list by that column.
* If you click on a kill button for a process you will be prompted on if you really want to kill the process. In that prompt you will have the option to kill the process (``SIGTERM``), or to force quit (``SIGKILL``) the process.
* You can exit the application by pressing the left arrow on the top left of the toolbar.
* If the process list is long enough, you can scroll it by swiping up or down on the list. After you have swiped the list will scroll.

Screenshot Tool (fret)
======================

The screenshot tool will automatically start up and wait for right button long presses, at which time it will request a screenshot be taken and stored to ``/home/root/screenshots``. It will then execute ``/tmp/.screenshot`` if it exists. Due to how the reMarkable preserves colour information in the framebuffer used to generate the screenshot, there may be colour in the screenshots that are not visible on the reMarkable itself.

Screenshot Viewer (anxiety)
===========================

* The arrow on the top right of the screen will exit the application.
* The gear on the top right of the screen will allow changing preview sizes in the screenshots list.
* Long press on a screenshot to delete it.
* Select a screenshot to view it.
* When a screenshot is open, the arrow on the top left of the screen will return to the screenshots list.
* When a screenshot is open, the delete button on the top right of the screen will delete the screenshot, and return to the screenshots list.

CLI Tool (rot)
==============

Using the CLI Tool (rot)
------------------------

Usage from the command line only, run ``rot --help`` to see possible options. For more information on possible API values see the `dbus specification files <https://github.com/Eeems/oxide/tree/master/interfaces>`_, the `high level API design <https://gist.github.com/Eeems/728d4ec836b156d880ce521ab50e5d40>`_, or :ref:`api`.

Usage
-----

.. code-block:: shell

  Usage: rot [options] api action
  Oxide settings tool

  Options:
    -h, --help             Displays help on commandline options.
    --help-all             Displays help including Qt specific options.
    -v, --version          Displays version information.
    -o, --object <object>  Object to act on, e.g.
                           Network:network/94d5caa2d4345ab7be5254dfb9678cd7
    --once                 Exit on the first signal when listening.

  Arguments:
    api                    settings
                           wifi
                           power
                           apps
                           system
                           screen
                           notification
    action                 get
                           set
                           listen
                           call

Get
___

.. code-block:: shell

  rot [options] api get name

  Options:
    -o, --object <object>  Object to act on, e.g.
                           Network:network/94d5caa2d4345ab7be5254dfb9678cd7

  Arguments:
    api                    settings
                           wifi
                           power
                           apps
                           system
                           screen
                           notification
    name                   Property to get.

  Example:

    rot power get batteryLevel

Set
___

.. parsed-literal::

  rot [options] api set name value

  Options:
    -o, --object <object>  Object to act on, e.g.
                           Network:network/94d5caa2d4345ab7be5254dfb9678cd7

  Arguments:
    api                    settings
                           wifi
                           power
                           apps
                           system
                           screen
                           notification
    name                   Property to get.
    value                  Value to set the property to.

  Example:

    rot system set autoSleep 5

Listen
______

.. parsed-literal::

  rot [options] api listen name

  Options:
    -o, --object <object>  Object to act on, e.g.
                           Network:network/94d5caa2d4345ab7be5254dfb9678cd7
    --once                 Exit on the first signal when listening.

  Arguments:
    api                    settings
                           wifi
                           power
                           apps
                           system
                           screen
                           notification
    name                   Signal to listen to.

  Example:

    rot --once system leftAction

Call
____

.. parsed-literal::

  rot [options] api call name arguments...

  Options:
    -o, --object <object>  Object to act on, e.g.
                           Network:network/94d5caa2d4345ab7be5254dfb9678cd7

  Arguments:
    api                    settings
                           wifi
                           power
                           apps
                           system
                           screen
                           notification
    name                   Signal to listen to.
    arguments              Arguments to pass to the method using the following format: <QVariant>:<Value>. e.g. QString:Test

  Example:

    rot screen call screenshot

Examples of usage
-----------------

These examples assume you have `jq` installed.

.. code-block:: bash

  #!/bin/bash
  # Get list of registered applications
  rot apps get applications | jq 'keys'

  # Get list of running applications
  rot apps get runningApplications | jq 'keys'

  # Get the display name of the current application
  rot apps get currentApplication \
    | jq -cr | sed 's|/codes/eeems/oxide1/||' \
    | xargs -I {} rot --object Application:{} apps get displayName \
    | jq -cr

  # Stop an application based on it's registration name
  rot apps get applications \
    | jq -cr '."codes.eeems.fret"' | sed 's|/codes/eeems/oxide1/||' \
    | xargs -I {} rot --object Application:{} apps call stop

  # Start an application based on it's registration name
  rot apps get applications \
    | jq -cr '."xochitl"' | sed 's|/codes/eeems/oxide1/||' \
    | xargs -I {} rot --object Application:{} apps call launch

  # Get list of notifications
  rot notification get notifications | jq

  # Add a notification
  uuid=$(cat /proc/sys/kernel/random/uuid)
  path=$(rot notification call add \
        "QString:\"$uuid\"" \
        'QString:"sample-application"' \
        'QString:"Hello world!"' \
        'QString:""' \
    | jq -cr \
    | sed 's|/codes/eeems/oxide1/||'
  )

  # Display the notification
  rot --object Notification:$path notification call display

  # Remove the notification
  rot --object Notification$path notification call remove

  # Get current battery percentage
  rot power get batteryLevel

  # Output whenever the battery percentage changes
  rot power listen batteryLevelChanged

  # Take a screenshot
  [ $(rot screen call screenshot | jq -cr) = "/" ] && echo "Failed to take screenshot!"

  # Remove all screenshots
  rot screen get screenshots \
    | jq -cr 'values | join("\n")' \
    | sed 's|/codes/eeems/oxide1/||' \
    | xargs -rI {} rot --object Screenshot:{} screen call remove

  # Wait for the leftAction (long press on left button, or swipe from left edge of screen)
  rot --once system listen leftAction

  # Log changes to wifi state
  rot wifi listen state

  # Disable telemetry
  rot settings set telemetry false

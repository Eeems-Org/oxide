====
Home
====

.. raw:: html

   <img src="_static/images/logo.png" alt="logo" class="logo">
   <br/>

Oxide is a `desktop environment <https://en.wikipedia.org/wiki/Desktop_environment>`_ for the `reMarkable tablet <https://remarkable.com/>`_.

Features
========

- Multitasking / application switching
- Notifications
- Wifi managment
- Optional lockscreen
- Homescreen for launching applications
- Process manager
- Take, view, and manage screenshots

Install Oxide
==============

.. raw:: html

  <div class="warning">
    ⚠️ <b>Warning:</b> Since this changes what application is launched on boot, you'll want to make sure you have your SSH password written down, and it's recommended to <a href="https://remarkable.guide/guide/access/ssh.html#setting-up-a-ssh-key">setup an SSH key</a>. This way you wont lose access to SSH if something goes wrong and your device soft-bricks.
  </div>
  <p>
    Oxide is available in
    <a href="https://vellum.delivery/">
      <img alt="vellum" src="https://vellum.delivery/vellum-logo.svg" class="sidebar-logo" style="width:24px;height:24px;"/>
      toltec
    </a>. These instructions assume you already have it installed.
  </p>

1. ``vellum add launcherctl-oxide``
2. ``launcherctl switch-launcher --start oxide``

Uninstall Oxide
===============

1. ``launcherctl switch-launcher --start xochitl``
2. ``vellum del launcherctl-oxide``

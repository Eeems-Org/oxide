====
Home
====

Oxide is a `desktop environment <https://en.wikipedia.org/wiki/Desktop_environment>`_ for the `reMarkable tablet <https://remarkable.com/>`_.

Features
========

- Multitasking / application switching
- Notifications
- Wifi managment
- Chroot for applications that you don't fully trust
- Optional lockscreen
- Homescreen for launching applications
- Process manager
- Take, view, and manage screenshots

Install Oxide
==============

.. raw:: html

  <p>
    Oxide is available in
    <a href="https://toltec-dev.org/#install-toltec">
      <img alt="toltec" src="_static/images/toltec-small.svg" class="sidebar-logo" style="width:24px;height:24px;"/>
      toltec
    </a>. These instructions assume you already have it installed.
  </p>

1. ``opkg install oxide``
2. ``systemctl disable --now xochitl``
3. If you are installing on a reMarkable 2: ``systemctl enable --now rm2fb``
4. ``systemctl enable --now tarnish``


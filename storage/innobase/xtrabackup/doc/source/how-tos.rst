===================
How-tos and Recipes
===================

.. _recipes-ibk:

Recipes for innobackupex
==========================

.. toctree::
   :maxdepth: 1

   howtos/recipes_ibkx_local
   howtos/recipes_ibkx_stream
   howtos/recipes_ibkx_inc
   howtos/recipes_ibkx_compressed
   howtos/recipes_ibkx_partition


.. _recipes-xbk:

Recipes for *xtrabackup*
========================

.. toctree::
   :maxdepth: 1

   howtos/recipes_xbk_full
   howtos/recipes_xbk_inc
   howtos/recipes_xbk_restore

.. _howtos:

How-Tos
=======

.. toctree::
   :maxdepth: 1

   howtos/setting_up_replication
   howtos/backup_verification
   howtos/recipes_ibkx_gtid

.. _aux-guides:

Auxiliary Guides
================

.. toctree::
   :maxdepth: 1

   howtos/enabling_tcp
   howtos/permissions
   howtos/ssh_server

Assumptions in this section
===========================

The context should make the recipe or tutorial understandable. To assure that this is true, a list of the assumptions, names and other objects that appears in this section. This items are specified at the beginning of each recipe or tutorial.

``HOST``

A system with a *MySQL*-based server installed, configured and running. We assume the following about this system:

* The MySQL server is able to :doc:`communicate with others by the  standard TCP/IP port <howtos/enabling_tcp>`;

* An SSH server is installed and configured - see :doc:`here <howtos/ssh_server>` if it is not;

* You have an user account in the system with the appropriate :doc:`permissions <howtos/permissions>`

* You have a MySQL's user account with appropriate :ref:`privileges`.

``USER``

This is a user account with shell access and the appropriate permissions for the task. A guide for checking them is :doc:`here <howtos/permissions>`.

``DB-USER``

This is a user account in the database server with the appropriate privileges for the task. A guide for checking them is :doc:`here <howtos/permissions>`.

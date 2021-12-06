===================
How-tos and Recipes
===================


Assumptions in this section
===========================

Most of the times, the context will make the recipe or tutorial understandable.
To assure that, a list of the assumptions, names and "things" that will appear
in this section is given. At the beginning of each recipe or tutorial they will
be specified in order to make it quicker and more practical.

``HOST``

   A system with a |MySQL|-based server installed, configured and running. We
   will assume the following about this system:

     * the MySQL server is able to :doc:`communicate with others by the
       standard TCP/IP port <howtos/enabling_tcp>`;

     * a SSH server is installed and configured - see :doc:`here
       <howtos/ssh_server>` if it is not;

     * you have an user account in the system with the appropriate
       :doc:`permissions <howtos/permissions>` and

     * you have a MySQL's user account with appropriate :ref:`privileges`.

``USER``
   An user account in the system with shell access and appropriate permissions
   for the task. A guide for checking them is :doc:`here <howtos/permissions>`.

``DB-USER``
   An user account in the database server with appropriate privileges for the
   task. A guide for checking them is :doc:`here <howtos/permissions>`.

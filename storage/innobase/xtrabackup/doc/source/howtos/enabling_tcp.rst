.. _enable-tcpip:


Enable TCP/IP for remote access
==============================================

A common issue when configuring a remote database is that their database instance only listens for local connections. This is the default setting. To reach the server from an external address, you must enable it.

Verify with ``netstat`` on a shell: ::

  $ netstat -lnp | grep mysql
  tcp         0        0 0.0.0.0:3306 0.0.0.0:* LISTEN 2480/mysqld 
  unix 2 [ ACC ] STREAM LISTENING 8101 2480/mysqld /tmp/mysql.sock

You should check the result for the following information:

*  A line starts with ``tcp`` (the server accepts TCP connections)

*  The first address (``0.0.0.0:3306`` in this example) is different than ``127.0.0.1:3306`` (the bind address is not local host address).

Review the ``my.cnf`` file. If you find the option ``skip-networking``, either comment it with the hashtag (#) out or delete it. 

Also, check, in the ``my.cnf`` file, if the `bind_address <https://dev.mysql.com/doc/refman/8.0/en/server-system-variables.html#sysvar_bind_address>`__ variable is set. The default value is ``127.0.0.1`` and only looks for local connections. Change the variable to reference an external IP address. 

.. note::

  For testing purposes, you could reset this variable to either ``*``, ``::``, or ``0.0.0.0``.

Then restart the server and check it again with ``netstat``. 

If the changes had no effect, then look at your distribution's startup scripts (like ``rc.mysqld``). Comment out flags like ``--skip-networking`` and/or change the ``bind-address``.

After the server listens to the remote TCP/IP connections properly, the last thing to do is check that the port (3306 by default) is open. Check your firewall configurations (``iptables -L``) and that you are allowing remote hosts on that port (in ``/etc/hosts.allow``).


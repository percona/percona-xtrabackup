# Installing and configuring a SSH server

Many Linux distributions only install the ssh client by default. If you don’t have the ssh server installed already, the easiest way of doing it is by using your distribution’s packaging system.

Using apt, run the following:

 ```default
 $ sudo apt install openssh-server
 ```

Using Red Hat Linux or a derivative, use the following:

 ```default
 $ sudo yum install openssh-server
 ```

Review your distribution's documentation on how to configure the server.
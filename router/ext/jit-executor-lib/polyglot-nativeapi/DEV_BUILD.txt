This project builds a Java Package with the classes needed to create a version
of the GraalVM Polyglot Library with some enhancements required for the proper
integration of GraalVM as a JavaScript runtime on the MySQL Middleware Clients.

The library is assembled by taking the official code for the GraalVM Polyglot
Library and exporting additional functionality through the use of some patch
files contained in the patches directory.

To work on changes in the enhancements done to the original GraalVM Polyglot
library, the patch files in the patches directory must be updated.

The process to update the enhancements for the MySQL Middleware applications
requires:

- Maven available: verify with mvn --version
- The GraalVM JDK Source Code
- The environment variable GRAALJDK_ROOT set to the path where the GraalVM JDK
  is unpacked.

PREPARATION TO UPDATE THE PROJECT
---------------------------------

In a shell terminal at the location of this README.txt file execute the
following commands:

$ mvn -Ddev process-sources

This command will create some temporary git commits and tags while executing
the next steps:

- 1) Copy the GraalVM Polyglot API sources from GRAALJDK_ROOT to this project.
- 2) Apply the patches in the patches folder to the copied sources.

At this point, you can modify the GraalVM Polyglot API as needed.

BUILDING THE NATIVE POLYGLOT LIBRARY
------------------------------------

Whenever you want to try out the enhanced library, you simply need to build it.
Go to the parent folder and execute:

$ mvn package

This process should repeat every time you want to try out a modification of the
Polyglot Library.


UPDATING THE PATCH FILES FOR THE LIBRARY
----------------------------------------

Once you are done with the changes on the Polyglot Library, it is time to
update the patch files, do it as follows:

$ mvn -Dcommit validate

This command will compile the updates needed on the patch file equivalent to
the changes you did. The temporary commits and tags will be removed and a new
commit will be created with the updated patches.

IMPORTANT: Update the decription of the new commit to properly describe the
changes done.
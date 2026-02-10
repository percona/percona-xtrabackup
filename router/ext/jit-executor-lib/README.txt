This project builds the JIT Executor library based on the GraalVM Polyglot
Native API Library with some enhancements required for the proper integration of
GraalVM as a JavaScript runtime on the MySQL Middleware Clients.

The library is assembled by taking the official code for the GraalVM Polyglot
Native API Library sources and exporting additional functionality through the
use of a patch file contained in the polyglot-nativeapi/patches directory.

For details about how to update this library to meet additional requirements on
the MySQL Middleware clients, refer to polyglot-nativeapi/DEV_BUILD.txt.

BUILD REQUIREMENTS
------------------

To build the JIT Executor library created with this project the following
requirements should be met:

- GraalVM 23.0.1 Installed and in PATH (to execute the native-image command)
- The GraalVM 23.0.1 source code.
- The JAVA_HOME environment variable set to the Home folder of the GraalVM JDK.
- Maven available: verify with mvn --version
- The environment variable GRAALJDK_ROOT set to the path where the GraalVM JDK
  source code is unpacked.

BUILDING THE PROJECT
--------------------

In a shell terminal at the location of this README.txt file execute the
following command:

$ mvn package

This command will execute the following steps:

- 1) Copy the GraalVM Polyglot API sources from GRAALJDK_ROOT to this project.
- 2) Apply the patches in the polyglot-nativeapi/patches folder to the copied
     sources.
- 3) Build the JIT Executor library

The JIT Executor library library will be built at the following path:

polyglot-nativeapi-native-library/target

CLEANING THE BUILD
------------------

Cleaning the build would delete all the compiled binaries but let the patched
source code ready to build again, this is, the code that was extracted from
the original GraalVM sources and then patched will stay ready for a new build.

Thi can be achieved by executing:

$ mvn clean

To completely reset the project to it's initial state, execute:

$ mvn -Ddev clean




# GENESIS

Genesis is the compiler and run-time interpreter/daemon for ColdC, and
is produced by the Cold Project.  You can learn more about helping to
support the Cold Project at:

    http://the-cold-dark.github.io/

Release information can be found in doc/CHANGES.

## INSTALL/COMPILATION IN UNIX

Genesis should compile on most ANSI/ISO C compilers. You will need
CMake, bison and either ndbm or gdbm's ndbm emulation.

To build:

    mkdir build
    cd build
    cmake ..
    make

CMake contains generators for other build systems as well and they
can be used instead of make.

## INSTALL/COMPILATION IN WIN32

You will need an ndbm implementation in Win32. You will then have to
figure out the remaining details and let us know. :)

## FURTHER INFORMATION

*** The mailing lists are long dead. ***

The following email lists are available:

    coldstuff           Generic cold-related list

Further information, Archives and Subscription mechanisms for the lists are
available at http://the-cold-dark.github.io/contact.html

Documentation:

* Genesis:     http://the-cold-dark.github.io/genesis/
* ColdC:       http://ice.cold.org/bin/help?node=coldc

## NOTES

Verified to compile and run on:

* Linux
* macOS

This code has, in the past, compiled on Solaris, FreeBSD and Windows
as well.

# GENESIS

Genesis is the compiler and run-time interpreter/daemon for ColdC, and
is produced by the Cold Project.  You can learn more about helping to
support the Cold Project at:

* [the Cold Project](https://cold.org/coldc/)

Documentation:

* [Genesis](https://cold.org/coldc/genesis.html)
* [ColdC](https://ice.cold.org/bin/help?node=coldc)

Historical release information can be found in doc/CHANGES, but this is
not current as of the 1.2 version.

## INSTALL/COMPILATION IN UNIX

Pre requirements:

    ndbm or gdbm
    bison
    cmake

On Centos/Fedora/Redhat derivatives:

	yum -y install cmake bison gdbm-devel

Genesis should compile on most ANSI/ISO C compilers.

To build, enter source folder and:

    mkdir build
    cd build
    cmake ..
    make

CMake contains generators for other build systems as well and they
can be used instead of make.

## INSTALL/COMPILATION IN WIN32

You will need an ndbm implementation in Win32. You will then have to
figure out the remaining details and let us know. :)

## NOTES

Verified to compile and run on:

* Linux
* Mac OS X

This code has, in the past, compiled on Solaris, FreeBSD and Windows
as well.


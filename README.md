pthread\_workqueues
===================

Installation - Linux
--------------------

    cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib <path to soruce>
    make
    make install

Installation - Red Hat
----------------------

    cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib <path to source>
    make
    cpack -G RPM

Installation - Debian
---------------------

    cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=/lib <path to source>
    make
    cpack -G DEB

Android
----------------------

     cmake -G "Unix Makefiles" -DCMAKE_C_COMPILER=<path to NDK compiler> -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib <path to source>
     make

Windows (Visual Studio Project)
-------------------------------

    cmake -G "Visual Studio 14 2015" <path to source>
    cmake --build .

Windows (clang/C2) (Visual Studio Project)
------------------------------------------

    cmake -G "Visual Studio 14 2015" -T "LLVM-vs2014" <path to source>
    cmake --build .

Xcode (project)
---------------

    cmake -G "Xcode" <path to source>

Running Unit Tests
------------------

    cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib -DENABLE_TESTING=YES <path to source>
    make
    make test


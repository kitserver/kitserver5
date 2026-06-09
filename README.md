# Introduction

Kitserver 5 is a companion program for Pro Evolution Soccer 5, Winning Eleven 9, and Winning Eleven 9 LE.

# How to build

The Makefile (in the src folder) is intended to be used by Microsoft NMAKE
tool, which comes with Visual Studio 2017+ Community Edition.

All dependencies are included, so you do not need to install any other
software to build this program

Execute the setenv.cmd so that it points to correct location
of your Visual Studio. You can also use setenv_legacy.cmd
for older releases of Visual Studio

Two types of build are supported:

RELEASE:

    nmake

BUILD SPECIFIC DLL:
    nmake kload.dll mydll.dll

DEBUG:

    nmake debug=1

To delete all object files in preparation for clean re-build:

    nmake clean-all

Currently, the difference between "debug" and "release" builds is in the
way the debug configuration parameter is treated. The "release" build
ignores debug values > 1, while "debug" build provides more detailed
logging for debug > 1. This behaviour, however, may change in the future.

(NOTE: You can also build the EXE and DLL files separately, using
appropriate targets, specified in the Makefile.)

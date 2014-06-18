#!/bin/sh

# Run a program with the new libraries, not the installed ones.

export LD_LIBRARY_PATH
LD_LIBRARY_PATH=`pwd`/lib

"$@"

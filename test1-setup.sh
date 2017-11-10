#!/bin/sh
# PURPOSE: Copy .phpt files as .php
TESTDIR=tests
rm -rf testt && mkdir testt
for F in $(echo $TESTDIR/*)
do
    if [ "${F##*.}" = "phpt" ]; then
        # sym link with a .php file extension instead of .phpt
        N=$(basename $F .phpt).php
        ln -s ../$F testt/$N
    elif [ -d $F ]; then
        # sym link the unchanged filename
        N=$(basename $F)
        ln -s ../$F testt/$N
    fi
done

#!/bin/sh
# PURPOSE: Load all .php scripts sequentially
BASEDIR=$(dirname "$0")
TESTDIR="$BASEDIR"/testt
PORT=9999
cd "$TESTDIR"
for F in *.php
do
    #echo "wget -O L.php_cli_test http://localhost:$PORT/$F" >> files.txt
    wget -O L.php_cli_test http://localhost:$PORT/$F
done

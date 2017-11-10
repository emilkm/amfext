#!/bin/sh
# PURPOSE: Start the CLI webserver and serve the temp tests created by test1-setup.sh
TESTDIR=testt
PHPCLI=../../sapi/cli/php
PHPOPT="-d max_execution_time=600 -d log_errors=Off"
PORT=9999
VALGRIND=valgrind
VALGRINDOPTS="--suppressions=php.supp --leak-check=full --show-reachable=yes --track-origins=yes"
USE_ZEND_ALLOC=0 $VALGRIND $VALGRINDOPTS $PHPCLI $PHPOPT -S localhost:$PORT -t $TESTDIR

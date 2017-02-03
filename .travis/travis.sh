#!/bin/bash

# PHP Settings
PHP_BASE="php-$PHP_VERSION"
PHP_PACKAGE="php-$PHP_VERSION.tar.gz"
PHP_URL="http://us1.php.net/get/$PHP_PACKAGE/from/this/mirror"

# Move out of project 
cd ../

# Get and extract PHP
wget $PHP_URL -O $PHP_PACKAGE
tar -xf $PHP_PACKAGE 
cd $PHP_BASE 

# Clone into ext directory
mv ../amfext ext/amf

# Build PHP
./buildconf --force
./configure --enable-debug --disable-all --enable-libxml --enable-simplexml --enable-dom --with-amf
make

# Run tests
export REPORT_EXIT_STATUS=1
export TEST_PHP_EXECUTABLE=sapi/cli/php
export NO_INTERACTION=1
sapi/cli/php run-tests.php ext/amf  


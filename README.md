amfext
======

AMFEXT is a PHP extension that provides encoding and decoding support for AMF0 and AMF3

# PHP7

`master` branch is now PHP7

* The tests provide reasonable coverage. All encoding/decoding tests from https://github.com/emilkm/efxphp have been used here.

[![Build Status](https://travis-ci.org/emilkm/amfext.svg?branch=master)](https://travis-ci.org/emilkm/amfext)

# PHP5

For PHP5 see the `php56` branch.

* Compiles and works on Windows and Linux, but has some memory leaks outstanding.

# DONE

* Works well with https://github.com/emilkm/efxphp
* Extension or not, encoded output and decoded input are the same, and are OK by Adobe/Apache BlazeDS.
* Removed support for _recordset_.
* Removed userland access to StringBuilder.
* Removed transliteration support. Can look at adding it back if necessary.
* Fixed various issues from the original version.
* Added support for Date, ByteArray, XML, XML Document, and Vector types.
* Changed the object serialization to support traits and traits references.
* Setup Travis CI.

# TODO

* Remove dependency on userland callbacks.
* Change StringBuilder to internal functionality, or object.
* Look at changing encoding so it supports the full packet.
* Write docs.
* ...




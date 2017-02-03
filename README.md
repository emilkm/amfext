amfext
=====

AMFEXT is an AMF PHP extension with encoding and decoding support for AMF0 and AMF3

## PHP7

`master` branch is now PHP7

* All tests pass on Windows and Linux.
* The tests provide reasonable coverage. All encoding/decoding tests from https://github.com/emilkm/efxphp have been used here.

# PHP5

For PHP5 see the `php56` branch.

* Compiles and runs on Windows and Linux, but has some memory leaks outstanding.

# DONE

* Works well with https://github.com/emilkm/efxphp
* Extension or not, encoded output and decoded input are the same, and are OK by Adobe/Apache BlazeDS.
* Removed support for _recordset_.
* Removed userland access to StringBuilder.
* Removed transliteration support. Can look at adding it back if necessary.
* Fixed various issues from the original version.
* Added support for Date, XML, Vector types.
* Changed the object serialization to support traits and traits references.

# TODO

* Setup Travis CI
* Remove dependency on userland callbacks.
* Change StringBuilder to internal functionality, or object.
* Look at changing encoding so it supports the full packet.
* Write docs.




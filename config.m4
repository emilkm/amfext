PHP_ARG_ENABLE(amf, wether to enable AMF Serialization support,
[  --enable-amf          Enable AMF Serialization support])

if test "$PHP_AMF" != "no"; then
  PHP_NEW_EXTENSION(amf, amf.c, $ext_shared)
  PHP_SUBST(AMF_SHARED_LIBADD)
  AC_DEFINE([HAVE_AMF], 1, [Wether you have AMF])
fi
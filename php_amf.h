#ifndef PHP_AMF_H
#define PHP_AMF_H 1

#define PHP_AMF_VERSION "0.1"
#define PHP_AMF_WORLD_EXTNAME "amf"

PHP_FUNCTION(amf_encode);
PHP_FUNCTION(amf_decode);

extern zend_module_entry amf_module_entry;
#define phpext_amf_ptr &amf_module_entry

#endif

#ifndef PHP_AMF_H
#define PHP_AMF_H 1

#define PHP_AMF_VERSION "0.1"
#define PHP_AMF_WORLD_EXTNAME "amf"

PHP_FUNCTION(amf_encode);
PHP_FUNCTION(amf_decode);

extern zend_module_entry amf_module_entry;
#define phpext_amf_ptr &amf_module_entry

ZEND_BEGIN_MODULE_GLOBALS(amf)
HashTable   *user_classes;
intptr_t     hash_mask_handle;
intptr_t     hash_mask_handlers;
int          hash_mask_init;
ZEND_END_MODULE_GLOBALS(amf)

ZEND_EXTERN_MODULE_GLOBALS(amf)
#define AMF_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(amf, v)

#endif

/**
 * PHP extension for Action Message Format (AMF) encoding and decoding with support for AMF0 and AMF3
 * 
 * amfext (http://emilmalinov.com/amfext)
 *
 * @copyright Copyright (c) 2015 Emil Malinov
 * @license   http://www.apache.org/licenses/LICENSE-2.0 Apache License 2.0
 * @link      http://github.com/emilkm/amfext
 * @package   amfext
 * 
 * @author    Emanuele Ruffaldi emanuele.ruffaldi@gmail.com - majority of the work
 * @author    Emil Malinov - bug fixes, enhancements, PHP version compatibility maintenance, unit tests                                 
 */

#ifndef PHP_AMF_H
#define PHP_AMF_H 1

#define PHP_AMF_VERSION "0.9.6-e"
#define PHP_AMF_WORLD_EXTNAME "amf"

PHP_FUNCTION(amf_encode);
PHP_FUNCTION(amf_decode);
PHP_FUNCTION(amf_sb_join_test);
PHP_FUNCTION(amf_sb_new);
PHP_FUNCTION(amf_sb_append);
PHP_FUNCTION(amf_sb_append_move);
PHP_FUNCTION(amf_sb_length);
PHP_FUNCTION(amf_sb_as_string);
PHP_FUNCTION(amf_sb_write);
PHP_FUNCTION(amf_sb_echo);
PHP_FUNCTION(amf_sb_memusage);

extern zend_module_entry amf_module_entry;
#define phpext_amf_ptr &amf_module_entry

#endif

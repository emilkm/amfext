/**
 * PHP7 extension for Action Message Format (AMF) encoding and decoding with support for AMF0 and AMF3
 * 
 * amfext (http://emilmalinov.com/amfext)
 *
 * @copyright Copyright (c) 2015 Emil Malinov
 * @license   http://www.apache.org/licenses/LICENSE-2.0 Apache License 2.0
 * @link      http://github.com/emilkm/amfext
 * @package   amfext
 * 
 * @author    Emanuele Ruffaldi emanuele.ruffaldi@gmail.com - original version for PHP 5.2
 * @author    Emil Malinov - PHP 7.X, PHP 5.X maintenance, unit tests, enhancements, bug fixes                                
 */

#ifndef PHP_AMF_H
#define PHP_AMF_H 1

#define PHP_AMF_VERSION "0.1"
#define PHP_AMF_WORLD_EXTNAME "amf"

PHP_FUNCTION(amf_encode);
PHP_FUNCTION(amf_decode);

extern zend_module_entry amf_module_entry;
#define phpext_amf_ptr &amf_module_entry

#endif

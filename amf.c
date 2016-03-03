/**
 * amf encoding and decoding of AMF and AMF3 data
 *
 * @license http://opensource.org/licenses/php.php PHP License Version 3
 * @copyright (c) 2006-2007 Emanuele Ruffaldi emanuele.ruffaldi@gmail.com
 * @author Emanuele Ruffaldi emanuele.ruffaldi@gmail.com
 *
 *
 * Naming of Functions:
 *
 * amf_write_       performs low level writing into the buffer
 * amf0_write_      writes some C value in AMF0
 * amf3_write_      writes some C value in AMF3
 * amf0_serialize_  writes a C value in AMF0 with the correct AMF type byte
 * amf3_serialize_  writes a C value in AMF0 with the correct AMF type byte
 */
#undef _DEBUG
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/php_string.h"
#include "ext/standard/php_var.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/basic_functions.h"
#include "ext/standard/php_incomplete_class.h"
#include "php_amf.h"
#include "php_memory_streams.h"
#include "ext/standard/info.h"
#include "stdlib.h"
#include "win32/php_stdint.h"

/*  module Declarations {{{1*/

static zend_function_entry amf_functions[] = 
{
    PHP_FE(amf_encode, NULL)
    PHP_FE(amf_decode, NULL)
    PHP_FE(amf_sb_join_test, NULL)
    PHP_FE(amf_sb_new, NULL)
    PHP_FE(amf_sb_append, NULL)
    PHP_FE(amf_sb_append_move, NULL)
    PHP_FE(amf_sb_length, NULL)
    PHP_FE(amf_sb_as_string, NULL)
    PHP_FE(amf_sb_write, NULL)
    PHP_FE(amf_sb_memusage, NULL)
    PHP_FALIAS(amf_sb_flat, amf_sb_as_string, NULL)
    PHP_FALIAS(amf_sb_echo, amf_sb_write, NULL)
    {NULL, NULL, NULL}
};

static PHP_MINFO_FUNCTION(amf) 
{
    php_info_print_table_start();
    php_info_print_table_header(2, "AMF Native Support", "enabled");
    php_info_print_table_row(2, "Compiled Version", PHP_AMF_VERSION);
    php_info_print_table_end();

/*  DISPLAY_INI_ENTRIES(); */
}

/*  resource StringBuilder */
#define PHP_AMF_STRING_BUILDER_RES_NAME "String Builder"
static void php_amf_sb_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC);
int amf_serialize_output_resource_reg;

PHP_MINIT_FUNCTION(amf) 
{
    amf_serialize_output_resource_reg = zend_register_list_destructors_ex(php_amf_sb_dtor, NULL, PHP_AMF_STRING_BUILDER_RES_NAME, module_number);
    return SUCCESS;
}

zend_module_entry amf_module_entry = 
{
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_AMF_WORLD_EXTNAME,
    amf_functions,
    PHP_MINIT(amf),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(amf),
#if ZEND_MODULE_API_NO >= 20010901
    PHP_AMF_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_AMF
ZEND_GET_MODULE(amf)
#endif

/* AMF enumeration {{{1*/

/** AMF0 types */
enum AMF0Codes { AMF0_NUMBER, AMF0_BOOLEAN, AMF0_STRING, AMF0_OBJECT, AMF0_MOVIECLIP, AMF0_NULL, AMF0_UNDEFINED, AMF0_REFERENCE, AMF0_MIXEDARRAY, AMF0_ENDOBJECT, AMF0_ARRAY, AMF0_DATE, AMF0_LONGSTRING, AMF0_UNSUPPORTED, AMF0_RECORDSET, AMF0_XML, AMF0_TYPEDOBJECT, AMF0_AMF3 };

/** AMF3 types */
enum AMF3Codes { AMF3_UNDEFINED, AMF3_NULL, AMF3_FALSE, AMF3_TRUE, AMF3_INTEGER, AMF3_NUMBER, AMF3_STRING, AMF3_XMLDOCUMENT, AMF3_DATE, AMF3_ARRAY, AMF3_OBJECT, AMF3_XML, AMF3_BYTEARRAY, AMF3_VECTOR_INT, AMF3_VECTOR_UINT, AMF3_VECTOR_DOUBLE, AMF3_VECTOR_OBJECT };

/** return values for callbacks */
enum AMFCallbackResult { AMFC_RAW, AMFC_XML, AMFC_OBJECT, AMFC_TYPEDOBJECT, AMFC_ANY, AMFC_ARRAY, AMFC_NONE, AMFC_BYTEARRAY, AMFC_EXTERNAL, AMFC_DATE, AMFC_XMLDOCUMENT, AMFC_VECTOR_OBJECT };

/** flags passed to amf_encode and amf_decode */
enum AMFFlags { AMF_AMF3 = 1, AMF_BIGENDIAN = 2, AMF_OBJECT_AS_ASSOC = 4, AMF_POST_DECODE = 8, AMF_AS_STRING_BUILDER = 16, AMF_TRANSLATE_CHARSET = 32, AMF_TRANSLATE_CHARSET_FAST = 32|64, AMF3_NSND_ARRAY_AS_OBJECT = 128 };

/** events invoked by the callback */
enum AMFEvent { AMFE_MAP = 1, AMFE_POST_OBJECT, AMFE_POST_XML, AMFE_MAP_EXTERNALIZABLE, AMFE_POST_BYTEARRAY, AMFE_TRANSLATE_CHARSET, AMFE_POST_DATE, AMFE_POST_XMLDOCUMENT, AMFE_VECTOR_INT, AMFE_VECTOR_UINT, AMFE_VECTOR_DOUBLE, AMFE_VECTOR_OBJECT };

/** flags for the recordset _amf_recordset_ */
enum AMFRecordSet { AMFR_NONE = 0, AMFR_ARRAY = 1, AMFR_ARRAY_COLLECTION = 2 };

/** flags for AMF3_OBJECT */
enum AMF3ObjectDecl { AMF_INLINE_ENTITY = 1, AMF_INLINE_CLASS = 2, AMF_CLASS_EXTERNAL = 4, AMF_CLASS_DYNAMIC = 8, AMF_CLASS_MEMBERCOUNT_SHIFT = 4, AMF_CLASS_SHIFT = 2 };

/**
 * flags for emitting strings that could possibly be translated
 * Typically use AMF_STRING_AS_TEXT. When you have bytearray or XML data, no transformation should be
 * made, so use AMF_STRING_AS_BYTE. In some cases internal ASCII strings are sent so just use
 * AMF_STRING_AS_SAFE_TEXT that is equivalent to AMF_STRING_AS_BYTE.
 */
enum AMFStringData { AMF_STRING_AS_TEXT = 0, AMF_STRING_AS_BYTE = 1, AMF_STRING_AS_SAFE_TEXT = 1 };

enum AMFStringTranslate { AMF_TO_UTF8, AMF_FROM_UTF8 };

/* Memory Management {{{1*/

/** deallocates a zval during deserialization */
static void amf_zval_dtor(void *p) 
{
    zval **zval_ptr = (zval **) p;
    zval_ptr_dtor(zval_ptr);
}

/** context of serialization */
typedef struct {
    HashTable objects0;     // stack of objects, no reference
    HashTable objects;      // stack of objects for AMF3, no reference
    HashTable strings;      // stack of strings for AMF3: string key => index
    HashTable traits;       // stack of traits for AMF3, allocated
    HashTable objtypes0;    // stack of object types for AMF0, related to objects0
    HashTable objtypes;     // stack of object types for AMF0, related to objects
    zval **callbackTarget;
    zval *callbackFx;
    zval *zEmpty_string;
    int flags;
    int nextObject0Index;
    int nextObjectIndex;
    int nextStringIndex;
    int nextTraitIndex;
} amf_serialize_data_t, amf_deserialize_data_t;


/**
* The result of the encoder is a string that grows depending on the input. When using memory streams or
* smart_str the allocation of memory is not efficient because these methods allow the generic access of the string.
* Instead in our case a StringBuilder approach is better suited. We have implemented such StringBuilder approach
* in which the resulting string is made of string parts. Each string part has a default length of AMFPARTSIZE
* that eventually can be bigger when long strings are appended. At the end of the processing such sequence of parts
* is joined into the resulting strings.
*
* Note: the AMFTSRMLS_CC and AMFTSRMLS_DC macros are required for supporting the stream method. In the StringBuilder
* method such macros are empty
*
*
* Optimized version: the StringBuilder is made of a sequence of references to string zval and blocks of raw data. In this
* way big strings from PHP are just referenced and copied at the end of the encoding. The memory management is modified
* by allocating a big block of memory in which raw and zval parts are placed. This behaviour is obtained by using a two
* state mechanism
*
* Structure
* |shortlength(2bytes)|rawdata|
* |0(2)|zval|
* |-1|
*/
#define amf_USE_STRING_BUILDER
/* #define amf_DISABLE_OUTPUT */

/*
 * This flag controls the use of zval in String Builders:
 * Always: #define amf_ZVAL_STRING_BUILDER 1 ||
 * Never: #define amf_ZVAL_STRING_BUILDER 0 &&
 * Only if with size: #define amf_ZVAL_STRING_BUILDER
 *
 * The code is:
 * if (amf_ZVAL_STRING_BUILDER len > amf_ZVAL_STRING_BUILDER_THRESHOLD)
 */
#define amf_ZVAL_STRING_BUILDER
#define amf_ZVAL_STRING_BUILDER_THRESHOLD 128
/*
#define amf_NO_ZVAL_STRING_BUILDER
#define amf_GUARD_ALLOCATION
*/

#ifdef amf_USE_STRING_BUILDER

typedef struct {
    int size;           // bit 0 = zval, rest is length. Length of 0 is terminaton
    union {
#ifndef amf_NO_ZVAL_STRING_BUILDER
        zval *zv;       // zvalue of the string
#endif
        char data[1];
    };
} amf_string_chunk;

/** this structure is placed at the beginning of the data block */
typedef struct amf_string_part_t {
    struct amf_string_part_t *next;  // pointer to the next
    amf_string_chunk data[1];        // dummy beginning of the data
} amf_string_part;

typedef struct {
    char *data;                 // pointer to the data of the current block
    int length;                 // total length of the string
    int default_size;
    int left_in_part;           // items left in part
    amf_string_chunk *last_chunk;
    amf_string_part  *last;     // last and current part. The next points to the beginning. Simple list
    int chunks;
    int parts;                  // number of parts, useful for debugging
    int total_allocated;        // total memory allocated
} amf_serialize_output_t;
typedef amf_serialize_output_t *amf_serialize_output;


#define AMFTSRMLS_CC
#define AMFTSRMLS_DC
#define AMFPARMAXSIZE 32768*4
#define AMFPARTSIZE 64

#define amf_PARTFLAG_ALLOCATED 1
#define amf_PARTFLAG_ZVAL 2

#ifdef amf_GUARD_ALLOCATION
static void *guard_emalloc(int k) 
{
    void *r = emalloc(k + 10); 
    memset(r, 0x7E, k); 
    memset((char *) r + k, 0x7F, 10); 
    return r;
}

static void guard_memcpy(char *cp, const char *src, int k) 
{
    while (k-- != 0) {
        if (*cp != 0x7E) {
            printf("guard!!!\n");
            break;
        }
        *cp++ = *src++;
    }
}
#else
#define guard_emalloc(k) emalloc(k)
#define guard_memcpy(cp, src, k) memcpy(cp, src, k)
#endif

static inline void amf_write_zstring(amf_serialize_output buf, zval *zstr AMFTSRMLS_DC);
static inline void amf_write_string(amf_serialize_output buf, const char *cp, int length AMFTSRMLS_DC);

/** allocate a block containing the part header and the data */
static amf_string_part *amf_serialize_output_part_ctor(int size) 
{
    amf_string_part *r = (amf_string_part *) guard_emalloc(size + sizeof(amf_string_part) + sizeof(amf_string_chunk) - sizeof(char));
    r->next = r;
    r->data->size = 0;
    return r;
}

/* closes the current chunk and move the pointer to the next chunk */
static void amf_serialize_output_close_chunk(amf_serialize_output buf) 
{
    /* close the last chunk if not a zchun */
    if (buf->last_chunk->size == 0) {
        buf->last_chunk->size = (buf->data-&buf->last_chunk->data[0]) << 1;
        if (buf->last_chunk->size == 0) {
            return;
        }
        /* get another chunk at the end */
        buf->last_chunk = (amf_string_chunk *) buf->data;
        buf->left_in_part -= sizeof(amf_string_chunk);
        buf->chunks++;
    } else {
        buf->last_chunk++;
    }
}

static void amf_serialize_output_close_part(amf_serialize_output buf) 
{
    amf_serialize_output_close_chunk(buf);
    buf->last_chunk->size = 0;
}

/** allocates a new StringBuilder with a default buffer */
static void amf_serialize_output_ctor(amf_serialize_output buf) 
{
    buf->length = 0;
    buf->default_size = AMFPARTSIZE;
    buf->last = amf_serialize_output_part_ctor(buf->default_size);
    buf->last_chunk = &buf->last->data[0];
    buf->last_chunk->size = 0;
    buf->data = &buf->last_chunk->data[0];
    buf->left_in_part = AMFPARTSIZE;
    buf->total_allocated = AMFPARTSIZE + sizeof(amf_string_part) + sizeof(amf_string_chunk) - sizeof(char);
    buf->parts = 1;
    buf->chunks = 0;
}

/**
 * appends a block of size specified to the StringBuilder. If the current part is a zpart then take some memory
 * from that. The size is not mandatory!
 */
static void amf_serialize_output_part_append(amf_serialize_output buf, int size) 
{
    amf_string_part *last = buf->last;
    amf_string_part *head = last->next;
    amf_string_part *cur;

    amf_serialize_output_close_part(buf);

    if (size == 0) {
        if (buf->default_size < AMFPARMAXSIZE) {
            buf->default_size *= 2;
        }
        size = buf->default_size;
    } 
    else if (size > AMFPARMAXSIZE) {
        size = AMFPARMAXSIZE;
    }

    cur = amf_serialize_output_part_ctor(size);
    buf->parts++;               // number of parts
    buf->total_allocated += size + sizeof(amf_string_part) + sizeof(amf_string_chunk) - sizeof(char);

    last->next = cur;           // last points to the new last
    cur->next = head;           // new last points to the head
    buf->last = cur;            // update new last

    buf->last_chunk = &buf->last->data[0];
    buf->last_chunk->size = 0;
    buf->data = &buf->last_chunk->data[0];
    buf->left_in_part = size;   // update the data space
}


/** builds a single string from a sequence of strings and places it into a zval */
static void amf_serialize_output_write(amf_serialize_output buf, php_stream *stream TSRMLS_DC)
{
    amf_string_part *cur, *head;
    if (buf->length == 0) {
        return;
    }
    head = cur = buf->last->next;
    amf_serialize_output_close_part(buf);

    /* printf("flattening length:%d parts:%d chunks:%d memory:%d\n", buf->length, buf->parts, buf->chunks, buf->total_allocated) */
    do {
        amf_string_chunk *chunk = (amf_string_chunk *) cur->data;
        while(chunk->size != 0) {
#ifndef amf_NO_ZVAL_STRING_BUILDER
            if ((chunk->size & 1) != 0) 
            {
                if (stream == NULL) {
                    zend_write(Z_STRVAL_P(chunk->zv), Z_STRLEN_P(chunk->zv));
                } else {
                    php_stream_write(stream, Z_STRVAL_P(chunk->zv), Z_STRLEN_P(chunk->zv));
                }
                chunk++;
            } 
            else
#endif
            {
                int len = chunk->size >> 1;
                if (stream == NULL) {
                    zend_write(chunk->data, len);
                } else {
                    php_stream_write(stream, chunk->data, len);
                }
                chunk = (amf_string_chunk *) (((char *) chunk->data) + len);
            }
        }
        cur = cur->next;
    } while (cur != head);
}

/** appends a sb from another and eventually clean up */
static void amf_serialize_output_append_sb(amf_serialize_output buf, amf_serialize_output inbuf, int copy) {
    amf_string_part *cur, *head, *last;
    if (inbuf->length == 0) {
        return;
    }
    last = inbuf->last;
    head = cur = last->next;

    if (copy == 1) {
        amf_serialize_output_close_part(inbuf);
        do {
            amf_string_chunk *chunk = (amf_string_chunk *) cur->data;
            while (chunk->size != 0) {
    #ifndef amf_NO_ZVAL_STRING_BUILDER
                if ((chunk->size & 1) != 0) 
                {
                    amf_write_zstring(buf, chunk->zv);
                    chunk++;
                }
                else
    #endif
                {
                    int len = chunk->size >> 1;
                    amf_write_string(buf, chunk->data,len);
                    chunk = (amf_string_chunk *) (((char *) chunk->data) + len);
                }
            }
            cur = cur->next;
        } while (cur != head);
    } else {
        /* TODO: possibly memory waste in last chunk */
        amf_string_part *dhead, *dlast;

        amf_serialize_output_close_part(buf);
        dlast = buf->last;
        dhead = dlast->next;
        buf->length += inbuf->length;
        buf->chunks += inbuf->chunks;
        buf->parts += inbuf->parts;
        buf->total_allocated += buf->total_allocated;
        buf->data = inbuf->data;
        dlast->next = head;      // after the last of dst, there is head of sr
        last->next = dhead;      // after the last of src, there is head of ds
        buf->last = last;
        buf->last_chunk = inbuf->last_chunk;
        buf->left_in_part = inbuf->left_in_part;

        amf_serialize_output_ctor(inbuf); // cleanup
    }
}

/** builds a single string from a sequence of strings and places it into a zval */
static void amf_serialize_output_get(amf_serialize_output buf, zval *result)
{
    amf_string_part *cur, *head;
    char *cp, *bcp;
    if (buf->length == 0) {
        ZVAL_EMPTY_STRING(result);
        return;
    }
    cp = bcp = (char *) guard_emalloc(buf->length);
    head = cur = buf->last->next;

    amf_serialize_output_close_part(buf);

    /* printf("flattening length:%d parts:%d chunks:%d memory:%d\n", buf->length, buf->parts, buf->chunks, buf->total_allocated); */
    do {
        amf_string_chunk *chunk = (amf_string_chunk *) cur->data;
        while (chunk->size != 0) {
#ifndef amf_NO_ZVAL_STRING_BUILDER
            if ((chunk->size & 1) != 0) 
            {
                int len = Z_STRLEN_P(chunk->zv);
                guard_memcpy(cp, Z_STRVAL_P(chunk->zv), len);
                cp += len;
                chunk++;
            }
            else
#endif
            {
                int len = chunk->size >> 1;
                guard_memcpy(cp, chunk->data, len);
                cp += len;
                chunk = (amf_string_chunk *) (((char *) chunk->data) + len);
            }
        }
        cur = cur->next;
    } while (cur != head);
    ZVAL_STRINGL(result, bcp, buf->length, 1);
    efree(bcp);
}

/**  destructor of the buffer */
static void amf_serialize_output_dtor(amf_serialize_output_t *buf) {
    amf_string_part *head, *cur;
    if (buf->last == NULL) {
        return;
    }
    cur = head = buf->last->next;
    do {
        amf_string_part *dt = cur;
        cur = cur->next;
        efree(dt);
    } while (cur != head);

    buf->length = 0;
    buf->last = NULL;
}

#else
typedef php_stream amf_serialize_output_t;
typedef amf_serialize_output_t * amf_serialize_output;
#define AMFTSRMLS_CC TSRMLS_CC
#define AMFTSRMLS_DC TSRMLS_DC
#endif

#define amf_SERIALIZE_CTOR(x, cb) amf_sederialize_ctor(&x, 1, cb TSRMLS_CC);
#define amf_DESERIALIZE_CTOR(x, cb) amf_sederialize_ctor(&x, 0, cb TSRMLS_CC);

#define amf_SERIALIZE_DTOR(x) amf_sederialize_dtor(&x);
#define amf_DESERIALIZE_DTOR(x) amf_sederialize_dtor(&x);    


/*  Common {{{1*/

/** initializes a zval to a HashTable of zval with a possible number of items */
static int amf_array_init(zval *arg, int count TSRMLS_DC) 
{
    ALLOC_HASHTABLE(arg->value.ht);
    zend_hash_init(arg->value.ht, count, NULL, ZVAL_PTR_DTOR, 0);
    arg->type = IS_ARRAY;
    return SUCCESS;
}

/** receives a pointer to the data and to the callback */
static void amf_sederialize_ctor(amf_serialize_data_t *x, int is_serialize, zval **cb TSRMLS_DC) 
{
    int error = 1;
    x->callbackTarget = NULL;
    x->callbackFx = NULL;
    MAKE_STD_ZVAL(x->zEmpty_string);
    ZVAL_EMPTY_STRING(x->zEmpty_string);
    if (cb != NULL) {
        if (Z_TYPE_PP(cb) == IS_ARRAY) {
            zval **tmp1, **tmp2;
            HashTable *ht = HASH_OF(*cb);
            int n = zend_hash_num_elements(ht);
            if (n == 2) {
                if (zend_hash_index_find(ht, 0, (void **) &tmp1) == SUCCESS && Z_TYPE_PP(tmp1) == IS_OBJECT) {
                    if (zend_hash_index_find(ht, 1, (void **) &tmp2) == SUCCESS && Z_TYPE_PP(tmp2) == IS_STRING) {
                        x->callbackTarget = tmp1;
                        x->callbackFx = *tmp2;
                        error = 0;
                    }
                }
            }
        } else if (Z_TYPE_PP(cb) == IS_STRING) {
            x->callbackFx = *cb;
            error = 0;
        }
        if (error == 1) {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf callback requires a string or an array (targetobject, methodname)");
        }
    }
    zend_hash_init(&(x->objects0), 10, NULL, NULL, 0);
    zend_hash_init(&(x->objects), 10, NULL, NULL, 0);
    zend_hash_init(&(x->objtypes0), 10, NULL, NULL, 0);
    zend_hash_init(&(x->objtypes), 10, NULL, NULL, 0);

    /* deserializer stores zval of strings for AMF */
    zend_hash_init(&(x->strings), 10, NULL, is_serialize ? NULL : amf_zval_dtor, 0);
    x->nextObject0Index = 0;
    x->nextObjectIndex = 0;
    x->nextTraitIndex = 0;
    x->nextStringIndex = 0;
    /* deserializer stores a hash for each class, while serializer is a long */
    zend_hash_init(&(x->traits), 10, NULL, is_serialize ? NULL : amf_zval_dtor, 0);
}

static void amf_sederialize_dtor(amf_serialize_data_t *x)
{
    zval_ptr_dtor(&(x->zEmpty_string));
    zend_hash_destroy(&(x->objects0));
    zend_hash_destroy(&(x->objects));
    zend_hash_destroy(&(x->strings));
    zend_hash_destroy(&(x->traits));
    zend_hash_destroy(&(x->objtypes0));
    zend_hash_destroy(&(x->objtypes));
}

/** returns the i-th element from the array */
static inline int amf_get_index_long(HashTable *ht, int i, int def) {
    zval **var;
    if (zend_hash_index_find(ht, i, (void **) &var) == SUCCESS) {
        if (Z_TYPE_PP(var) == IS_LONG) {
            return Z_LVAL_PP(var);
        } else {
            return def;
        }
    } else {
        return def;
    }
}

/** returns the i-th element from the array as long and returns default */
static inline int amf_get_assoc_long(HashTable *ht, const char *field, int def) {
    zval **var;
    if (zend_hash_find(ht, (char *) field, strlen(field) + 1, (void **) &var) == SUCCESS) {
        if (Z_TYPE_PP(var) == IS_LONG) {
            return Z_LVAL_PP(var);
        } else if (Z_TYPE_PP(var) == IS_DOUBLE) {
            return (int)Z_DVAL_PP(var);
        } else if (Z_TYPE_PP(var) == IS_BOOL) {
            return Z_BVAL_PP(var);
        } else {
            return def;
        }
    } else {
        return def;
    }
}




/**
 * places an object in the cache by using a string representation of its address
 * it is not using the direct pointer because the key is not guaranteed to be
 * sized as the pointer
 * \param old is the pointer to the output code
 * \param nextIndex is a pointer to a variable containing the nextIndex used by objects
 * \param action if bit 0 is set do not lookup. If bit 1 is set do not add
 * \return FAILURE if existent
 * code taken from serializer
 */
static inline int amf_cache_zval(HashTable *var_hash, HashTable *var, ulong *old, int *nextIndex, int action)
{
    if (sizeof(ulong) >= sizeof(int *)) {
        ulong *old_idx = NULL;
        ulong idx = (ulong)var;

        if ((action & 1) == 0) {
            if (zend_hash_index_find(var_hash, idx, (void **) &old_idx) == SUCCESS) {
                *old = *old_idx;
                return FAILURE;
            }
        }

        if ((action & 2) == 0) {
            /* +1 because otherwise hash will think we are trying to store NULL pointer */
            if (nextIndex == NULL) {
                *old = zend_hash_num_elements(var_hash);
            } else {
                *old = *nextIndex;
                *nextIndex = *nextIndex + 1;  // equal to the number of elements
            }
            zend_hash_quick_add(var_hash, NULL, 0, idx, old, sizeof(*old), NULL);
        }
    } else {
        char id[32], *p;
        register int len;

        /* relies on "(long)" being a perfect hash function for data pointers */
        p = smart_str_print_long(id + sizeof(id) - 1, (long) var);
        len = id + sizeof(id) - 1 - p;

        if ((action & 1) == 0) {
            if (zend_hash_find(var_hash, p, len, (void **) &old) == SUCCESS) {
                return FAILURE;
            }
        }

        if ((action & 2) == 0) {
            /* +1 because otherwise hash will think we are trying to store NULL pointer */
            if (nextIndex == 0) {
                *old = zend_hash_num_elements(var_hash);
            } else {
                *old = *nextIndex;
                *nextIndex = *nextIndex + 1;  // equal to the number of elements
            }
            zend_hash_add(var_hash, p, len, old, sizeof(*old), NULL);
        }
    }
    return SUCCESS;
}

static int amf_cache_zval_typed(amf_serialize_data_t *var_hash, zval *val, ulong *old, int version, int action, int amfc_type TSRMLS_DC)
{
    HashTable *cache = version == 0 ? &(var_hash->objects0) : &(var_hash->objects);
    HashTable *obj;
    switch (Z_TYPE_P(val)) {
        case IS_OBJECT:
            obj = Z_OBJPROP_P(val); 
            break;
        case IS_ARRAY:
            obj = HASH_OF(val); 
            break;
        case IS_RESOURCE:
            obj = (HashTable *) Z_LVAL_P(val); 
            break;
        default: 
            return SUCCESS;
    }

    if (amf_cache_zval(cache, obj, old, version == 0 ? &(var_hash->nextObject0Index) : &(var_hash->nextObjectIndex), action) == SUCCESS) {
        if ((action & 2) == 0) {
            if (version == 0) {

            } else {
                zval *zv;
                MAKE_STD_ZVAL(zv);
                ZVAL_LONG(zv, amfc_type);
                zend_hash_index_update(&(var_hash->objtypes), *old, &zv, sizeof(zv), NULL);
            }
        }
        return SUCCESS;
    } else {
        return FAILURE;
    }
}

/* Encode {{{1*/

static void amf0_serialize_var(amf_serialize_output buf, zval **struc, amf_serialize_data_t *var_hash TSRMLS_DC);
static void amf3_serialize_var(amf_serialize_output buf, zval **struc, amf_serialize_data_t *var_hash TSRMLS_DC);
static void amf0_serialize_array(amf_serialize_output buf, HashTable *myht, amf_serialize_data_t *var_hash TSRMLS_DC);
static void amf3_serialize_array(amf_serialize_output buf, HashTable *myht, amf_serialize_data_t *var_hash TSRMLS_DC);
static void amf3_serialize_vector(amf_serialize_output buf, HashTable *myht, amf_serialize_data_t *var_hash TSRMLS_DC);
static int amf3_write_string_zval(amf_serialize_output buf, zval *string_zval, enum AMFStringData raw, amf_serialize_data_t *var_hash TSRMLS_DC);
static int amf3_write_string(amf_serialize_output buf, const char *cp, int n, enum AMFStringData raw, amf_serialize_data_t *var_hash TSRMLS_DC);
static void amf3_write_uint29(amf_serialize_output buf, int num AMFTSRMLS_DC);

#ifdef amf_USE_STRING_BUILDER
/** Writes a single byte into the output buffer */
static inline void amf_write_byte(amf_serialize_output buf, int n)
{
#ifndef amf_DISABLE_OUTPUT
    if (buf->left_in_part <= 0) {
        amf_serialize_output_part_append(buf, 0);
    }
    *buf->data++ = n;
    buf->left_in_part--;
    buf->length++;
#endif
}
#else
/**  Writes a single byte into the output buffer */
static inline void _AMF_write_byte(amf_serialize_output buf, int n TSRMLS_DC)
{
    char c = (char)n;
    php_stream_write(buf, &c, 1);
}
/**  Writes a single byte into the output buffer */
#define amf_write_byte(buf, n) _AMF_write_byte((buf), (n) TSRMLS_CC)
#endif

static inline void amf_write_string(amf_serialize_output buf, const char *cp, int length AMFTSRMLS_DC)
{
#ifndef amf_DISABLE_OUTPUT
#ifdef amf_USE_STRING_BUILDER
    while (length > 0) {
        int left;
        if (buf->left_in_part <= 0) {
            amf_serialize_output_part_append(buf, length > AMFPARTSIZE ? length: 0);
        }
        left = buf->left_in_part;
        if (left > length) {
            left = length;
        }
        /* printf("append raw %d of %d in buffer of %d\n", left, length, buf->last->length) */
        guard_memcpy(buf->data, cp, left);
        cp += left;
        buf->data += left;
        buf->left_in_part -= left;
        buf->length += left;
        length -= left;
    }
#else
    php_stream_write(buf, cp, length);
#endif
#endif
}

/** writes a string from a zval. Provides additional optimization */
static inline void amf_write_zstring(amf_serialize_output buf, zval *zstr AMFTSRMLS_DC)
{
#ifndef amf_DISABLE_OUTPUT
    const int len = Z_STRLEN_P(zstr);
    if (len == 0) {
        return;
    }
#ifdef amf_USE_STRING_BUILDER
#ifndef amf_NO_ZVAL_STRING_BUILDER
    else if (amf_ZVAL_STRING_BUILDER len > amf_ZVAL_STRING_BUILDER_THRESHOLD) {
        if (buf->left_in_part < sizeof(amf_string_chunk)) {
            amf_serialize_output_part_append(buf, 0 AMFTSRMLS_DC);
        }

        amf_serialize_output_close_chunk(buf);

        buf->last_chunk->size = 1;  // zval chunk
        buf->last_chunk->zv = zstr;
        Z_ADDREF_P(zstr);
        buf->chunks++;
        buf->left_in_part -= sizeof(amf_string_chunk);

        /* prepare for a raw chunk */
        buf->last_chunk++;
        buf->last_chunk->size = 0;
        buf->data = buf->last_chunk->data;
        buf->length += len;
    }
#endif
#endif
    else {
        amf_write_string(buf, Z_STRVAL_P(zstr), len AMFTSRMLS_CC);
    }
#endif
}


static int my_hash_zval_identical_function(const zval **z1, const zval **z2)
{
	zval result;
	TSRMLS_FETCH();

	/* is_identical_function() returns 1 in case of identity and 0 in case
	 * of a difference;
	 * whereas this comparison function is expected to return 0 on identity,
	 * and non zero otherwise.
	 */
	if (is_identical_function(&result, (zval *) *z1, (zval *) *z2 TSRMLS_CC) == FAILURE) {
		return 1;
	}
	return !Z_LVAL(result);
}


/** writes a double number in AMF format. It is stored as Big Endian */
static void amf_write_double(amf_serialize_output buf, double num, amf_serialize_data_t *var_hash AMFTSRMLS_DC)
{
    union aligned {
        double dval;
        char cval[8];
    } d;
    const char *number = d.cval;
    d.dval = num;

    /* AMF number: b(0) double(8 bytes big endian */
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        char numberr[8] = { number[7], number[6], number[5], number[4], number[3], number[2], number[1], number[0] };
        amf_write_string(buf, numberr, 8 AMFTSRMLS_CC);
    } else {
        amf_write_string(buf, number, 8 AMFTSRMLS_CC);
    }
}

/** writes a int number in AMF format. It is stored as Big Endian */
static void amf3_write_vector_int(amf_serialize_output buf, int num, amf_serialize_data_t *var_hash AMFTSRMLS_DC)
{
    union aligned {
        int ival;
        char cval[4];
    } i;
    const char *number = i.cval;
    i.ival = num;

    /* AMF number: b(0) int(4 bytes big endian */
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        char numberr[8] = { number[3], number[2], number[1], number[0] };
        amf_write_string(buf, numberr, 4 AMFTSRMLS_CC);
    } else {
        amf_write_string(buf, number, 4 AMFTSRMLS_CC);
    }
}

/** writes a uint number in AMF format. It is stored as Big Endian */
static void amf3_write_vector_uint(amf_serialize_output buf, uint num, amf_serialize_data_t *var_hash AMFTSRMLS_DC)
{
    union aligned {
        uint Ival;
        char cval[4];
    } i;
    const char *number = i.cval;
    i.Ival = num;

    /* AMF number: b(0) int(4 bytes big endian */
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        char numberr[8] = { number[3], number[2], number[1], number[0] };
        amf_write_string(buf, numberr, 4 AMFTSRMLS_CC);
    } else {
        amf_write_string(buf, number, 4 AMFTSRMLS_CC);
    }
}

/** writes a double number in AMF format. It is stored as Big Endian */
static void amf3_write_vector_double(amf_serialize_output buf, double num, amf_serialize_data_t *var_hash AMFTSRMLS_DC)
{
    amf_write_double(buf, num, var_hash);
}

/** writes an integer in AMF0 format. It is formatted in Big Endian 4 byte */
static void amf0_write_int(amf_serialize_output buf, int n AMFTSRMLS_DC)
{
    char tmp[4] = { (n >> 24) & 0xFF, (n >> 16) & 0xFF, (n >> 8) & 0xFF, (n & 0xFF) };
    amf_write_string(buf, tmp, 4 AMFTSRMLS_CC);
}

/** writes a short in AMF0 format. It is formatted in Big Endian 2 byte */
static void amf0_write_short(amf_serialize_output buf, int n AMFTSRMLS_DC)
{
    amf_write_byte(buf, ((n >> 8) & 0xFF));
    amf_write_byte(buf, (n & 0xFF));
}

/** writes the end of object terminator of AMF0 */
static void amf0_write_objectend(amf_serialize_output buf AMFTSRMLS_DC)
{
    static char objectEnd[] = {0,0,9};
    amf_write_string(buf, objectEnd, 3 AMFTSRMLS_CC);
}

static zval *amf_translate_charset_zstring(zval *inz, enum AMFStringTranslate direction, amf_serialize_data_t *var_hash TSRMLS_DC);
static zval *amf_translate_charset_string(const unsigned char *cp, int length, enum AMFStringTranslate direction, amf_serialize_data_t *var_hash TSRMLS_DC);

/** serializes a zval as zstring in AMF0 using AMF0_STRING or AMF0_LONGSTRING */
static void amf0_serialize_zstring(amf_serialize_output buf, zval *zv, enum AMFStringData raw, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    int length;
    if (raw == AMF_STRING_AS_TEXT && (var_hash->flags & AMF_TRANSLATE_CHARSET) != 0) {
        zval *zzv;
        if ((zzv = amf_translate_charset_zstring(zv, AMF_TO_UTF8, var_hash TSRMLS_CC)) != 0) {
            zv = zzv;
        }
    }

    /* AMF string: b(2) w(length) text(utf) if length < 65536 */
    /* AMF string: b(12) dw(length) text(utf) */
    length = Z_STRLEN_P(zv);
    if (length < 65536) {
        amf_write_byte(buf, AMF0_STRING);
        amf0_write_short(buf, length AMFTSRMLS_CC);
        if (length == 0) {
            return;
        }
    } else {
        amf_write_byte(buf, AMF0_LONGSTRING);
        amf0_write_int(buf, length AMFTSRMLS_CC);
    }
    amf_write_zstring(buf, zv AMFTSRMLS_CC);
}

static void amf0_serialize_emptystring(amf_serialize_output buf AMFTSRMLS_DC)
{
    amf_write_byte(buf, AMF0_STRING);
    amf0_write_short(buf, 0 AMFTSRMLS_CC);
}

/** serializes a string variable */
static void amf0_serialize_string(amf_serialize_output buf, const char *cp, enum AMFStringData raw, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    int length = strlen(cp);

    if (length > 0 && raw == AMF_STRING_AS_TEXT && (var_hash->flags & AMF_TRANSLATE_CHARSET) != 0) {
        zval *zzv = 0;
        if ((zzv = amf_translate_charset_string((unsigned char *) cp, length, AMF_TO_UTF8, var_hash TSRMLS_CC)) != 0) {
            amf0_serialize_zstring(buf, zzv, AMF_STRING_AS_BYTE, var_hash TSRMLS_CC);
            return;
        }
    }

    length = strlen(cp);
    if (length < 65536) {
        amf_write_byte(buf, AMF0_STRING);
        amf0_write_short(buf, length AMFTSRMLS_CC);
    } else {
        amf_write_byte(buf, AMF0_LONGSTRING);
        amf0_write_int(buf, length AMFTSRMLS_CC);
    }
    amf_write_string(buf, cp, length AMFTSRMLS_CC);
}

/** sends a short string to AMF */
static void amf0_write_string(amf_serialize_output buf, const char *cp, enum AMFStringData raw, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    int length = strlen(cp);
    if (length > 0 && raw == AMF_STRING_AS_TEXT && (var_hash->flags & AMF_TRANSLATE_CHARSET) != 0) {
        zval *zzv = 0;
        if ((zzv = amf_translate_charset_string((unsigned char *) cp, length, AMF_TO_UTF8, var_hash TSRMLS_CC)) != 0) {
            int length = Z_STRLEN_P(zzv);
            if (length >= 65536) {
                length = 65536 - 2;
            }
            amf0_write_short(buf, length AMFTSRMLS_CC);
            amf_write_zstring(buf, zzv AMFTSRMLS_CC);
            return;
        }
    }

    length = strlen(cp);
    amf0_write_short(buf, length AMFTSRMLS_CC);
    amf_write_string(buf, cp, length AMFTSRMLS_CC);
}

/**  serializes an empty AMF3 string */
static inline void amf3_write_emptystring(amf_serialize_output buf AMFTSRMLS_DC)
{
    amf_write_byte(buf, 1);
}

/** writes the AMF3_OBJECT followed by the class information */
static inline void amf3_write_head(amf_serialize_output buf, zval **amfcType, int head AMFTSRMLS_DC)
{
    int amfct;

    if (Z_TYPE_PP(amfcType) == IS_LONG) {
        amfct = Z_LVAL_PP(amfcType);
        switch (amfct) {
            case AMFC_DATE:
                amf_write_byte(buf, AMF3_DATE);
                break;
            case AMFC_BYTEARRAY:
                amf_write_byte(buf, AMF3_BYTEARRAY);
                break;
            case AMFC_XML:
                amf_write_byte(buf, AMF3_XML);
                break;
            case AMFC_XMLDOCUMENT:
                amf_write_byte(buf, AMF3_XMLDOCUMENT);
                break;
            case AMFC_VECTOR_OBJECT:
                amf_write_byte(buf, AMF3_VECTOR_OBJECT);
                break;
            default:
                amf_write_byte(buf, AMF3_OBJECT);
        }
    } else {
        amf_write_byte(buf, AMF3_OBJECT);
    }
    amf3_write_uint29(buf, head AMFTSRMLS_CC);
}

/** writes the AMF3_OBJECT followed by the class information */
static inline void amf3_write_objecthead(amf_serialize_output buf, int head AMFTSRMLS_DC)
{
    amf_write_byte(buf, AMF3_OBJECT);
    amf3_write_uint29(buf, head AMFTSRMLS_CC);
}

/** serializes a Hash Table as AMF3 plain object */
static void amf3_serialize_object_anonymous(amf_serialize_output buf, HashTable *myht, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    char *key;
    zval **data;
    ulong keyIndex;
    uint key_len;
    HashPosition pos;
    int memberCount = 0;

    {
        ulong var_no = var_hash->nextTraitIndex++;
        const int isDynamic = AMF_CLASS_DYNAMIC;
        const int isExternalizable = 0;  // AMF_CLASS_EXTERNALIZABLE

        zend_hash_add(&(var_hash->traits), "", 0, &var_no, sizeof(var_no), NULL);
        amf3_write_objecthead(buf, memberCount << AMF_CLASS_MEMBERCOUNT_SHIFT | isExternalizable | isDynamic | AMF_INLINE_CLASS | AMF_INLINE_ENTITY  AMFTSRMLS_CC);
        amf3_write_string(buf, "", 0, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
    }

    /* iterate over all the keys */
    zend_hash_internal_pointer_reset_ex(myht, &pos);
    for (;; zend_hash_move_forward_ex(myht, &pos)) {
        int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
        if (keyType == HASH_KEY_NON_EXISTANT) {
            break;
        }

        /* is it possible */
        if (keyType == HASH_KEY_IS_LONG) {
            char txt[20];
            sprintf(txt, "%d", keyIndex);
            amf3_write_string(buf, txt, strlen(txt), AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
        } else if (keyType == HASH_KEY_IS_STRING) {
            /* Don't write protected properties or explicit type */
            if (key[0] == 0 || key == "_explicitType") {
                continue;
            }
            amf3_write_string(buf, key, key_len - 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
        }

        /* we should still add element even if it's not OK, since we already wrote the length of the array before */
        if (zend_hash_get_current_data_ex(myht, (void **) &data, &pos) != SUCCESS || !data) {
            amf_write_byte(buf, AMF3_UNDEFINED);
        } else {
            amf3_serialize_var(buf, data, var_hash TSRMLS_CC);
        }
    }
    amf3_write_emptystring(buf AMFTSRMLS_CC);
}

/** serializes a Hash Table as AMF3 plain object */
static void amf3_serialize_object_typed(amf_serialize_output buf, HashTable *myht, const char *className, int classNameLen, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    char *key;
    zval **data, *ti, *properties; 
    ulong keyIndex;
    uint key_len;
    HashPosition pos;
    ulong ref, reference = 0; 
    HashTable *htCached = NULL, *htCachedProps = NULL, *htProperties = NULL;
    int isDynamic = 0; // AMF_CLASS_DYNAMIC
    int isExternalizable = 0;  // AMF_CLASS_EXTERNALIZABLE
    int propCount = 0;
    int cleanup = 1;


    /* ti (traits info) is an associative array containing the reference, and an array of property names */
    propCount = zend_hash_num_elements(myht);
    MAKE_STD_ZVAL(properties);
    amf_array_init(properties, propCount + 1 TSRMLS_CC);
    MAKE_STD_ZVAL(ti);
    amf_array_init(ti, 2 TSRMLS_CC);

    add_assoc_zval(ti, "properties", properties);
    htProperties = HASH_OF(properties);
    
    zend_hash_internal_pointer_reset_ex(myht, &pos);
    for (;; zend_hash_move_forward_ex(myht, &pos)) {
        int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
        if (keyType == HASH_KEY_NON_EXISTANT) {
            break;
        }

        /* Don't write protected properties or explicit type */
        if (keyType != HASH_KEY_IS_STRING || (keyType == HASH_KEY_IS_STRING && (key[0] == 0 || key == "_explicitType"))) {
            continue;
        }

        add_assoc_null(properties, key);
    }

    if (zend_hash_find(&(var_hash->traits), (char *) className, classNameLen, (void **) &data) == SUCCESS) {
        htCached = HASH_OF(*data);
        if (zend_hash_find(htCached, "properties", sizeof("properties"), (void **) &data) == SUCCESS) {
            htCachedProps = HASH_OF(*data);
        } else {

        }

        if (zend_hash_compare(htCachedProps, htProperties, (compare_func_t) my_hash_zval_identical_function, 1 TSRMLS_CC) == 0) {
            if (zend_hash_find(htCached, "reference", sizeof("reference"), (void **) &data) == SUCCESS) {
                reference = Z_LVAL_P(*data);
            } else {

            }

            ref = reference << AMF_CLASS_SHIFT | AMF_INLINE_ENTITY;
            amf3_write_objecthead(buf, ref AMFTSRMLS_CC);
        } else {
            isDynamic = AMF_CLASS_DYNAMIC;
            ref = 3 | isExternalizable | isDynamic | (0 << 4);
            amf3_write_objecthead(buf, ref AMFTSRMLS_CC);
            amf3_write_string(buf, className, classNameLen, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
        }
    } else {
        cleanup = 0;
        add_assoc_long(ti, "reference", var_hash->nextTraitIndex++);
        zend_hash_add(&(var_hash->traits), (char *) className, classNameLen, &ti, sizeof(ti), NULL);
        //Z_ADDREF_P(ti);
        
        ref = 3 | isExternalizable | isDynamic | (propCount << 4);
        amf3_write_objecthead(buf, ref  AMFTSRMLS_CC);
        amf3_write_string(buf, className, classNameLen, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);

        /* iterate over all the properties */
        zend_hash_internal_pointer_reset_ex(htProperties, &pos);
        for (;; zend_hash_move_forward_ex(htProperties, &pos)) {
            int keyType = zend_hash_get_current_key_ex(htProperties, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
            if (keyType == HASH_KEY_NON_EXISTANT) {
                break;
            }

            /* Skip the numeric index */
            if (keyType != HASH_KEY_IS_STRING) {
                continue;
            }
            amf3_write_string(buf, key, key_len - 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
        }
    }

    if (isDynamic == 0) {
        zend_hash_internal_pointer_reset_ex(myht, &pos);
        for (;; zend_hash_move_forward_ex(myht, &pos)) {
            int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
            if (keyType == HASH_KEY_NON_EXISTANT) {
                break;
            }

            if (keyType == HASH_KEY_IS_STRING) {
                /* Don't write protected properties or explicit type */
                if (key[0] == 0 || key == "_explicitType") {
                    continue;
                }
            
                /* we should still add element even if it's not OK, since we already wrote the length of the array before */
                if (zend_hash_get_current_data_ex(myht, (void **) &data, &pos) != SUCCESS || !data) {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                } else {
                    amf3_serialize_var(buf, data, var_hash TSRMLS_CC);
                }
            }
        }
    } else {
        zend_hash_internal_pointer_reset_ex(myht, &pos);
        for (;; zend_hash_move_forward_ex(myht, &pos)) {
            int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
            if (keyType == HASH_KEY_NON_EXISTANT) {
                break;
            }

            if (keyType == HASH_KEY_IS_STRING) {
                /* Don't write protected properties or explicit type */
                if (key[0] == 0 || key == "_explicitType") {
                    continue;
                }
                amf3_write_string(buf, key, key_len - 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);

                /* we should still add element even if it's not OK, since we already wrote the length of the array before */
                if (zend_hash_get_current_data_ex(myht, (void **) &data, &pos) != SUCCESS || !data) {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                } else {
                    amf3_serialize_var(buf, data, var_hash TSRMLS_CC);
                }
            }
        }
        amf3_write_emptystring(buf AMFTSRMLS_CC);
    }

    if (cleanup == 1) {
        
        //amf_zval_dtor(properties);
    }
}

static int amf_perform_serialize_callback_event(int ievent, zval *arg0, zval **zResultValue, int shared, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    if (var_hash->callbackFx != NULL) {
        int r; // result from function
        zval *zEmpty1 = NULL, *zievent;
        zval *zResultValuePtr;
        zval *arg0orig = arg0;
        MAKE_STD_ZVAL(zievent);
        ZVAL_LONG(zievent, ievent);

        if (arg0 == NULL) {
            MAKE_STD_ZVAL(zEmpty1);
            ZVAL_NULL(zEmpty1);
        }

        {
            zval **params[2] = { arg0 == NULL ? &zEmpty1 : &arg0, &zievent };
            if ((r = call_user_function_ex(CG(function_table), var_hash->callbackTarget, var_hash->callbackFx, &zResultValuePtr, 2, params, 0, NULL TSRMLS_CC)) == SUCCESS) {
                /* if the result is different from the original value we cannot rely on that zval* if it is not empty */
                if (arg0 != arg0orig) {
                    zval_add_ref(&arg0orig);
                }

                if (zResultValuePtr != *zResultValue && zResultValuePtr != NULL) {
                    if (*zResultValue == NULL) {
                        MAKE_STD_ZVAL(*zResultValue);
                    } else if (shared != 0) { // cannot replace the zval
                        zval_ptr_dtor(zResultValue);
                        MAKE_STD_ZVAL(*zResultValue);
                    }
                    COPY_PZVAL_TO_ZVAL(**zResultValue, zResultValuePtr);
                }
            }
        }
        zval_ptr_dtor(&zievent);
        if (zEmpty1 != NULL) {
            zval_ptr_dtor(&zEmpty1);
        }
        return r;
    } else {
        return FAILURE;
    }
}

/**
 * invokes the encoding callback
 * \param event is the event = AMFE_MAP
 * \param struc is the value
 * \param className is the resulting class name of the class of the object
 * \return
 */
static int amf_perform_serialize_callback(zval **struc, const char **className, int *classNameLen, zval ***resultValue, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    int resultType = AMFC_TYPEDOBJECT;

    if (var_hash->callbackFx != NULL) {
        zval *zievent;
        zval **params[] = { struc, &zievent };
        zval *rresultValue = NULL;
        MAKE_STD_ZVAL(zievent);
        ZVAL_LONG(zievent, AMFE_MAP);

        if (call_user_function_ex(CG(function_table), var_hash->callbackTarget, var_hash->callbackFx, &rresultValue, 2, params, 0, NULL TSRMLS_CC) == SUCCESS) {
            if (rresultValue != NULL && Z_TYPE_PP(&rresultValue) == IS_ARRAY) {
                zval **tmp;
                HashTable *ht = HASH_OF(rresultValue);
                if (zend_hash_index_find(ht, 0, (void **) &tmp) == SUCCESS) {
                    *resultValue = tmp;
                    if (zend_hash_index_find(ht, 1, (void **) &tmp) == SUCCESS) {
                        convert_to_long_ex(tmp);
                        resultType = Z_LVAL_PP(tmp);
                        if (zend_hash_index_find(ht, 2, (void **) &tmp) == SUCCESS && Z_TYPE_PP(tmp) == IS_STRING) {
                            *className = Z_STRVAL_PP(tmp);
                            *classNameLen = Z_STRLEN_PP(tmp);
                        }
                    }
                }
                /* php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf custom %p => %p %d %s",struc, resultValue, resultType, *className) */
            }
        }
        zval_ptr_dtor(&zievent);
    }
    return resultType;
}

/* AMF3 object */
static void amf3_serialize_object(amf_serialize_output buf, zval **struc, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    const char *className = Z_TYPE_PP(struc) == IS_RESOURCE ? "" : Z_OBJCE_PP(struc)->name;
    int classNameLen = strlen(className);
    ulong objectIndex;
    int explicitTypeIsSet = 0;
    zval **explicitType;
    zval **amfcType;

    /* if the object is already in cache then just go for it */
    if (amf_cache_zval_typed(var_hash, *struc, &objectIndex, 1, 2, -1 TSRMLS_CC) == FAILURE) {
        if (zend_hash_index_find(&(var_hash->objtypes), objectIndex, (void **) &amfcType) == SUCCESS) {
            amf3_write_head(buf, amfcType, (objectIndex << 1) AMFTSRMLS_CC);
        } else {
            amf3_write_objecthead(buf, (objectIndex << 1) AMFTSRMLS_CC);
        }
        return;
    }

    if (Z_TYPE_PP(struc) == IS_OBJECT) {
        HashTable *obj = Z_OBJPROP_PP(struc);
        explicitTypeIsSet = zend_hash_exists(obj, "_explicitType", sizeof("_explicitType"));
        if (explicitTypeIsSet == 1) {
            zend_hash_find(obj, "_explicitType", sizeof("_explicitType"), (void **) &explicitType);
        }
    }

    if ((strcmp(className, "stdClass") == 0 && explicitTypeIsSet == 0)
        || (explicitTypeIsSet == 1 && Z_TYPE_PP(explicitType) == IS_STRING && Z_STRVAL_PP(explicitType) == "")
    ) {
        amf_cache_zval(&(var_hash->objects), Z_OBJPROP_PP(struc), &objectIndex, &(var_hash->nextObjectIndex), 0);
        amf3_serialize_object_anonymous(buf, Z_OBJPROP_PP(struc), var_hash TSRMLS_CC);
    } else {
        int resultType = AMFC_TYPEDOBJECT;
        int resultValueLength = 0;
        zval **resultValue = struc;
        int deallocResult = Z_REFCOUNT_PP(struc);

        resultType = amf_perform_serialize_callback(struc, &className, &classNameLen, &resultValue, var_hash TSRMLS_CC);

        if (Z_TYPE_PP(resultValue) == IS_RESOURCE) {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. Resources should be transformed to something");
            amf_write_byte(buf, AMF3_UNDEFINED);
            return;
        }

        switch (resultType) {
            case AMFC_RAW:
                if (Z_TYPE_PP(resultValue) == IS_STRING) {
                    amf_write_zstring(buf, *resultValue AMFTSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_RAW requires a string");
                }
                break;
            case AMFC_OBJECT:
                if (amf_cache_zval_typed(var_hash, *resultValue, &objectIndex, 1, 0, AMFC_OBJECT TSRMLS_CC) == FAILURE) {
                    amf3_write_objecthead(buf, objectIndex << 1 AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_OBJECT) {
                    amf3_serialize_object_anonymous(buf, Z_OBJPROP_PP(resultValue), var_hash TSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_OBJECT requires an object or an array");
                }
                break;
            case AMFC_TYPEDOBJECT:
                if (amf_cache_zval_typed(var_hash, *resultValue, &objectIndex, 1, 0, AMFC_TYPEDOBJECT TSRMLS_CC) == FAILURE) {
                    amf3_write_objecthead(buf, objectIndex << 1 AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_OBJECT) {
                    amf3_serialize_object_typed(buf, Z_OBJPROP_PP(resultValue), className, classNameLen, var_hash TSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_TYPEDOBJECT requires an object or an array");
                }
                break;
            case AMFC_ARRAY:
                if (amf_cache_zval_typed(var_hash, *resultValue, &objectIndex, 1, 0, AMFC_ARRAY TSRMLS_CC) == FAILURE) {
                    amf3_write_objecthead(buf, objectIndex << 1 AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_ARRAY) {
                    amf3_serialize_array(buf, HASH_OF(*resultValue), var_hash TSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_OBJECT) {
                    amf3_serialize_array(buf, Z_OBJPROP_PP(resultValue), var_hash TSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_ARRAY requires an object or an array");
                }
                break;
            case AMFC_DATE:
                if (amf_cache_zval_typed(var_hash, *struc, &objectIndex, 1, 0, AMFC_DATE TSRMLS_CC) == FAILURE) {
                    amf_write_byte(buf, AMF3_DATE);
                    amf3_write_uint29(buf, objectIndex << 1 AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_DOUBLE) {
                    amf_write_byte(buf, AMF3_DATE);
                    amf3_write_uint29(buf, 1 AMFTSRMLS_CC);
                    amf_write_double(buf, Z_DVAL_PP(resultValue), var_hash AMFTSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_DATE requires a double");
                }
                break;
            case AMFC_XML:
                if (amf_cache_zval_typed(var_hash, *struc, &objectIndex, 1, 0, AMFC_XML TSRMLS_CC) == FAILURE) {
                    amf_write_byte(buf, AMF3_XML);
                    amf3_write_uint29(buf, objectIndex << 1 AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_STRING) {
                    amf_write_byte(buf, AMF3_XML);
                    amf3_write_string_zval(buf, *resultValue, AMF_STRING_AS_BYTE, var_hash TSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_XML requires a string");
                }
                break;
            case AMFC_XMLDOCUMENT:
                if (amf_cache_zval_typed(var_hash, *struc, &objectIndex, 1, 0, AMFC_XMLDOCUMENT TSRMLS_CC) == FAILURE) {
                    amf_write_byte(buf, AMF3_XMLDOCUMENT);
                    amf3_write_uint29(buf, objectIndex << 1 AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_STRING) {
                    amf_write_byte(buf, AMF3_XMLDOCUMENT);
                    amf3_write_string_zval(buf, *resultValue, AMF_STRING_AS_BYTE, var_hash TSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_XML requires a string");
                }
                break;
            case AMFC_BYTEARRAY:
                if (amf_cache_zval_typed(var_hash, *struc, &objectIndex, 1, 0, AMFC_BYTEARRAY TSRMLS_CC) == FAILURE) {
                    amf_write_byte(buf, AMF3_BYTEARRAY);
                    amf3_write_uint29(buf, objectIndex << 1 AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_STRING) {
                    amf_write_byte(buf, AMF3_BYTEARRAY);
                    amf3_write_string_zval(buf, *resultValue, AMF_STRING_AS_BYTE, var_hash TSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_BYTEARRAY requires a string");
                }
                break;
            case AMFC_VECTOR_OBJECT:
                if (amf_cache_zval_typed(var_hash, *resultValue, &objectIndex, 1, 0, AMFC_VECTOR_OBJECT TSRMLS_CC) == FAILURE) {
                    amf_write_byte(buf, AMF3_VECTOR_OBJECT);
                    amf3_write_uint29(buf, objectIndex << 1 AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_ARRAY) {
                    amf3_serialize_vector(buf, HASH_OF(*resultValue), var_hash TSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_OBJECT) {
                    amf3_serialize_vector(buf, Z_OBJPROP_PP(resultValue), var_hash TSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_ARRAY requires an object or an array");
                }
                break;
            case AMFC_ANY:
                amf3_serialize_var(buf, resultValue, var_hash TSRMLS_CC);
                break;
            case AMFC_NONE:
                amf_write_byte(buf, AMF3_UNDEFINED);
                break;
            default:
                amf_write_byte(buf, AMF3_UNDEFINED);
                php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. unknown type %d", resultType);
                break;
        }
        if (*resultValue != *struc) {
            zval_ptr_dtor(resultValue);
        }
    }
}

/*
 serializes an object
 objectdata:
   utfname data
   w(0) b(9) = endof

 objectdata:
   utfname data
   w(0) b(9)
*/
static void amf0_serialize_objectdata(amf_serialize_output buf, HashTable *myht, int isArray, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    char *key;
    uint key_len;
    zval **data;
    int keyIndex;
    HashPosition pos;

    zend_hash_internal_pointer_reset_ex(myht, &pos);
    for (;; zend_hash_move_forward_ex(myht, &pos)) {
        int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
        if (keyType == HASH_KEY_NON_EXISTANT) {
            break;
        }

        if (keyType == HASH_KEY_IS_LONG) {
            char txt[20];
            int length;
            sprintf(txt, "%d", keyIndex);
            length = strlen(txt);
            amf0_write_short(buf, length AMFTSRMLS_CC);
            amf_write_string(buf, txt, length AMFTSRMLS_CC);
        } else {
            /* skip private member */
            if (isArray == 0 && key[0] == 0 && key != "_explicitType") {
                continue;
            }
            amf0_write_short(buf, key_len - 1 AMFTSRMLS_CC);
            amf_write_string(buf, key, key_len - 1 AMFTSRMLS_CC);
        }

        /* we should still add element even if it's not OK, since we already wrote the length of the array before */
        if (zend_hash_get_current_data_ex(myht, (void **) &data, &pos) != SUCCESS || !data) {
            amf_write_byte(buf, AMF0_UNDEFINED);
        } else {
            amf0_serialize_var(buf, data, var_hash TSRMLS_CC);
        }
    }
    amf0_write_objectend(buf AMFTSRMLS_CC);
}

/*
 serializes an object
 objectdata:
   utfname data
   w(0) b(9) = endof

 objectdata:
   utfname data
   w(0) b(9)
*/
static void amf0_serialize_object(amf_serialize_output buf, zval **struc, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    const char *className = Z_TYPE_PP(struc) == IS_RESOURCE ? "" : Z_OBJCE_PP(struc)->name;
    int classNameLen = strlen(className);
    ulong objectIndex;
    int explicitTypeIsSet = 0;
    zval **explicitType;

     /* if the object is already in cache then just go for it */
    if (amf_cache_zval_typed(var_hash, *struc, &objectIndex, 1, 2, -1 TSRMLS_CC) == FAILURE) {
        amf_write_byte(buf, AMF0_REFERENCE);
        amf0_write_short(buf, objectIndex AMFTSRMLS_CC);
        return;
    }

    if (Z_TYPE_PP(struc) == IS_OBJECT) {
        HashTable *obj = Z_OBJPROP_PP(struc);
        explicitTypeIsSet = zend_hash_exists(obj, "_explicitType", sizeof("_explicitType"));
        if (explicitTypeIsSet == 1) {
            zend_hash_find(obj, "_explicitType", sizeof("_explicitType"), (void **) &explicitType);
        }
    }

    if ((strcmp(className, "stdClass") == 0 && explicitTypeIsSet == 0)
        || (explicitTypeIsSet == 1 && Z_TYPE_PP(explicitType) == IS_STRING && Z_STRVAL_PP(explicitType) == "")
    ) {
        amf_write_byte(buf, AMF0_OBJECT);
        amf0_serialize_objectdata(buf, Z_OBJPROP_PP(struc), 0, var_hash TSRMLS_CC);
    } else {
        int resultType = AMFC_TYPEDOBJECT;
        int resultValueLength = 0;
        zval **resultValue = struc;

        resultType = amf_perform_serialize_callback(struc, &className, &classNameLen, &resultValue, var_hash TSRMLS_CC);

        if (Z_TYPE_PP(resultValue) == IS_RESOURCE) {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. Resources should be transformed to something");
            amf_write_byte(buf, AMF0_UNDEFINED);
            return;
        }

        switch (resultType) {
            case AMFC_RAW:
                /* it's a string purely sent */
                amf_write_zstring(buf, *resultValue AMFTSRMLS_CC);
                break;
            case AMFC_XML:
            case AMFC_XMLDOCUMENT:
                resultValueLength = Z_STRLEN_PP(resultValue);
                amf_write_byte(buf, AMF0_XML);
                amf0_write_int(buf, resultValueLength AMFTSRMLS_CC);
                amf_write_zstring(buf, *resultValue AMFTSRMLS_CC);
                break;
            case AMFC_OBJECT:
                if (Z_TYPE_PP(resultValue) == IS_OBJECT) {
                    if (amf_cache_zval_typed(var_hash, *resultValue, &objectIndex, 0, 0, AMFC_OBJECT TSRMLS_CC) == FAILURE) {
                        amf_write_byte(buf, AMF0_REFERENCE);
                        amf0_write_short(buf, objectIndex AMFTSRMLS_CC);
                    } else {
                        amf_write_byte(buf, AMF0_OBJECT);
                        amf0_serialize_objectdata(buf, Z_OBJPROP_PP(resultValue), 0, var_hash TSRMLS_CC);
                    }
                } else {
                    amf_write_byte(buf, AMF0_NULL);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_OBJECT requires an object");
                }
                break;
            case AMFC_ARRAY:
                if (amf_cache_zval_typed(var_hash, *resultValue, &objectIndex, 0, 0, AMFC_ARRAY TSRMLS_CC) == FAILURE) {
                    amf_write_byte(buf, AMF0_REFERENCE);
                    amf0_write_short(buf, objectIndex AMFTSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_ARRAY) {
                    amf0_serialize_array(buf, HASH_OF(*resultValue), var_hash TSRMLS_CC);
                } else if (Z_TYPE_PP(resultValue) == IS_OBJECT) {
                    amf0_serialize_array(buf, Z_OBJPROP_PP(resultValue), var_hash TSRMLS_CC);
                } else {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_ARRAY requires an object or an array");
                }
                break;
            case AMFC_TYPEDOBJECT:
                if (amf_cache_zval_typed(var_hash, *resultValue, &objectIndex, 0, 0, AMFC_TYPEDOBJECT TSRMLS_CC) == FAILURE) {
                    amf_write_byte(buf, AMF0_REFERENCE);
                    amf0_write_short(buf, objectIndex AMFTSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF0_TYPEDOBJECT);
                    if (Z_TYPE_PP(resultValue) == IS_OBJECT) {
                        amf0_write_string(buf, className, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                        amf0_serialize_objectdata(buf, Z_OBJPROP_PP(resultValue), 0, var_hash TSRMLS_CC);
                    } else {
                        amf0_write_string(buf, className, AMF_STRING_AS_TEXT, var_hash  TSRMLS_CC);
                        amf0_serialize_objectdata(buf, HASH_OF(*resultValue), 0, var_hash TSRMLS_CC);
                    }
                }
                break;
            case AMFC_ANY:
                amf0_serialize_var(buf, resultValue, var_hash TSRMLS_CC);
                break;
            case AMFC_EXTERNAL:
                amf3_write_objecthead(buf, AMF_INLINE_CLASS | AMF_INLINE_ENTITY AMFTSRMLS_CC);
                amf3_write_string(buf, className, classNameLen, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                amf3_serialize_var(buf, resultValue, var_hash TSRMLS_CC);
                break;
            case AMFC_NONE:
                amf_write_byte(buf, AMF0_UNDEFINED);
                break;
            case AMFC_BYTEARRAY:
                if (Z_TYPE_PP(resultValue) == IS_STRING) {
                    amf0_serialize_zstring(buf, *resultValue, AMF_STRING_AS_BYTE, var_hash TSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF0_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_BYTEARRAY requires a string");
                }
                break;
            case AMFC_DATE:
                if (Z_TYPE_PP(resultValue) == IS_DOUBLE) {
                    amf_write_byte(buf, AMF0_DATE);
                    amf_write_double(buf, Z_DVAL_PP(resultValue), var_hash AMFTSRMLS_CC);
                    amf0_write_short(buf, 0 AMFTSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF0_UNDEFINED);
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. AMFC_DATE requires a double");
                }
                break;
            default:
                amf_write_byte(buf, AMF0_UNDEFINED);
                php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf encoding callback. unknown type %d", resultType);
                break;
        }
        if (*resultValue != *struc) {
            zval_ptr_dtor(resultValue);
        }
    }
}

/** writes an integer in AMF3 format as a variable bytes */
static void amf3_write_uint29(amf_serialize_output buf, int value AMFTSRMLS_DC)
{
    value &= 0x1fffffff;
    if (value < 0x80) {
        amf_write_byte(buf,value);
    } else if (value < 0x4000) {
        amf_write_byte(buf, value >> 7 & 0x7f | 0x80);
        amf_write_byte(buf, value & 0x7f);
    } else if (value < 0x200000) {
        amf_write_byte(buf, value >> 14 & 0x7f | 0x80);
        amf_write_byte(buf, value >> 7 & 0x7f | 0x80);
        amf_write_byte(buf, value & 0x7f);
    } else {
        char tmp[4] = { value >> 22 & 0x7f | 0x80, value >> 15 & 0x7f | 0x80, value >> 8 & 0x7f | 0x80, value & 0xff };
        amf_write_string(buf, tmp, 4 AMFTSRMLS_CC);
    }
}

/** serializes a string */
static int amf3_write_string(amf_serialize_output buf, const char *string_ptr, int string_length, enum AMFStringData raw, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    if (string_length == 0) {
        amf_write_byte(buf, 1);  // inline and empty
        return -1;
    } else {
        ulong *val;
        if (zend_hash_find(&(var_hash->strings), (char *) string_ptr, string_length, (void **) &val) == SUCCESS) {
            amf3_write_uint29(buf, (*val - 1) << 1 AMFTSRMLS_CC);
            return *val - 1;
        } else {
            ulong index = ++var_hash->nextStringIndex;
            zend_hash_add(&(var_hash->strings), (char *) string_ptr, string_length, &index, sizeof(index), NULL);
            amf3_write_uint29(buf, ((string_length << 1) | AMF_INLINE_ENTITY) AMFTSRMLS_CC);

            if (raw == AMF_STRING_AS_TEXT && (var_hash->flags & AMF_TRANSLATE_CHARSET) != 0) {
                zval *zv = 0;
                if ((zv = amf_translate_charset_string((unsigned char *) string_ptr, string_length, AMF_TO_UTF8, var_hash TSRMLS_CC)) != 0) {
                    amf_write_zstring(buf, zv AMFTSRMLS_CC);
                    return index - 1;
                }
            }
            amf_write_string(buf, string_ptr, string_length AMFTSRMLS_CC);
            return index - 1;
        }
    }
}

/** writes a string from ZVAL in AMF3 format. Useful for memory reference optimization */
static int amf3_write_string_zval(amf_serialize_output buf, zval *string_zval, enum AMFStringData raw, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    int string_length = Z_STRLEN_P(string_zval);
    char *string_ptr = Z_STRVAL_P(string_zval);
    if (string_length == 0) {
        amf_write_byte(buf, 1);  // inline and empty
        return -1;
    } else {
        ulong *val;

        if (zend_hash_find(&(var_hash->strings), (char *) string_ptr, string_length, (void **) &val) == SUCCESS) {
            amf3_write_uint29(buf, (*val - 1) << 1 AMFTSRMLS_CC);
            return *val - 1;
        } else {
            ulong index = ++var_hash->nextStringIndex;
            zend_hash_add(&(var_hash->strings), (char *) string_ptr, string_length, &index, sizeof(index), NULL);
            amf3_write_uint29(buf, string_length << 1 | AMF_INLINE_ENTITY AMFTSRMLS_CC);
            if (raw == AMF_STRING_AS_TEXT && (var_hash->flags & AMF_TRANSLATE_CHARSET) != 0) {
                zval *zv = 0;
                if ((zv = amf_translate_charset_zstring(string_zval, AMF_TO_UTF8, var_hash TSRMLS_CC)) != 0) {
                    amf_write_zstring(buf, zv AMFTSRMLS_CC);
                    return index - 1;
                }
            }
            amf_write_zstring(buf, string_zval AMFTSRMLS_CC);
            return index - 1;
        }
    }
}

static void amf3_serialize_array(amf_serialize_output buf, HashTable *myht, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    if (myht && zend_hash_num_elements(myht) != 0) {
        char *key;
        zval **data;
        int keyIndex;
        uint key_len;
        HashPosition pos;
        int rt;

        /* Special Handling for arrays with __amf_recordset__ */
        if ((rt = amf_get_assoc_long(myht, "__amf_recordset__", 0)) != AMFR_NONE) {
            zval **columns, **rows;
            HashTable *htRows;
            HashTable *htColumns;
            int nColumns, nRows, iRow;
            int iClassDef;

            if (zend_hash_find(myht, "rows", sizeof("rows"), (void **) &rows) == SUCCESS
                && zend_hash_find(myht, "columns", sizeof("columns"), (void **) &columns) == SUCCESS
                && Z_TYPE_PP(rows) == IS_ARRAY
                && Z_TYPE_PP(columns) == IS_ARRAY
            ) {
                htRows = HASH_OF(*rows);
                htColumns = HASH_OF(*columns);
                nColumns = zend_hash_num_elements(htColumns);
                nRows = zend_hash_num_elements(htRows);

                if (rt == AMFR_ARRAY_COLLECTION) {
                    const char *ac = "flex.messaging.io.ArrayCollection";
                    amf3_write_objecthead(buf, AMF_INLINE_ENTITY | AMF_INLINE_CLASS | AMF_CLASS_EXTERNAL AMFTSRMLS_CC);
                    amf3_write_string(buf, ac, strlen(ac), AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                    var_hash->nextTraitIndex++;
                    var_hash->nextObjectIndex++;
                }

                amf_write_byte(buf, AMF3_ARRAY);
                amf3_write_uint29(buf, (nRows << 1) | AMF_INLINE_ENTITY AMFTSRMLS_CC);
                amf3_write_emptystring(buf AMFTSRMLS_CC);

                /* increment the object count in the cache */
                iRow = 0;
                zend_hash_internal_pointer_reset_ex(htRows, &pos);
                for (;; zend_hash_move_forward_ex(htRows, &pos)) {
                    int nColumnSizeOfRow;
                    int iColumn;
                    HashTable *htRow;
                    zval **zRow;
                    HashPosition posRow;

                    int keyType = zend_hash_get_current_key_ex(htRows, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                    if (keyType != HASH_KEY_IS_LONG) {
                        break;
                    }
                    if (zend_hash_get_current_data_ex(htRows, (void **) &zRow, &pos) != SUCCESS || !zRow || Z_TYPE_PP(zRow) != IS_ARRAY) {
                        amf_write_byte(buf, AMF3_UNDEFINED);
                        continue;
                    }
                    htRow = HASH_OF(*zRow);
                    nColumnSizeOfRow = zend_hash_num_elements(htRow);
                    if (nColumnSizeOfRow > nColumns) { // long row
                        nColumnSizeOfRow = nColumns;
                    }

                    if (iRow == 0) {
                        amf3_write_objecthead(buf, nColumns << AMF_CLASS_MEMBERCOUNT_SHIFT | AMF_INLINE_CLASS | AMF_INLINE_ENTITY AMFTSRMLS_CC);
                        amf3_write_emptystring(buf AMFTSRMLS_CC);  // empty class name
                        iClassDef = var_hash->nextTraitIndex++;

                        for (iColumn = 0; iColumn < nColumns; iColumn++) {
                            zval **columnName;
                            zend_hash_index_find(htColumns, iColumn, (void **) &columnName);
                            if (Z_TYPE_PP(columnName) != IS_STRING) {
                                char key[255];
                                sprintf(key, "unk%d", iColumn);
                                amf3_write_string(buf, key, strlen(key), AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                            } else {
                                amf3_write_string_zval(buf, *columnName, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                            }
                        }
                    } else {
                        amf3_write_objecthead(buf, iClassDef << AMF_CLASS_SHIFT | AMF_INLINE_ENTITY AMFTSRMLS_CC);
                    }
                    var_hash->nextObjectIndex++;

                    zend_hash_internal_pointer_reset_ex(htRow, &posRow);
                    for (iColumn = 0; iColumn < nColumnSizeOfRow; zend_hash_move_forward_ex(htRow, &posRow)) {
                        zval **zValue;
                        int keyType = zend_hash_get_current_key_ex(htRow, &key, &key_len, (ulong *) &keyIndex, 0, &posRow);
                        if (keyType != HASH_KEY_IS_LONG) {
                            break;
                        }
                        if (zend_hash_get_current_data_ex(htRow, (void **)  &zValue, &posRow) != SUCCESS) {
                            zValue = NULL;
                        }
                        if (zValue == NULL) {
                            amf_write_byte(buf, AMF3_UNDEFINED);
                        } else {
                            amf3_serialize_var(buf, zValue, var_hash TSRMLS_CC);
                        }
                        iColumn++;
                    }

                     /* short row */
                    for (; iColumn < nColumns; iColumn++) {
                        amf_write_byte(buf, AMF3_UNDEFINED);
                    }
                    iRow++;
                }
                return;
            } else {
                php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot understand special recordset array: should have __AMF_recordset__, rows and columns keys");
            }
        }

        {
            int max_index = -1;
            ulong dense_count = 0, assoc_count = 0;

            zend_hash_internal_pointer_reset_ex(myht, &pos);
            for (;; zend_hash_move_forward_ex(myht, &pos)) {
                int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                if (keyType == HASH_KEY_NON_EXISTANT) {
                    break;
                }
                switch (keyType) {
                    case HASH_KEY_IS_LONG:
                        if (keyIndex > max_index) {
                            max_index = keyIndex;
                        }
                        if ((keyIndex > 0 && max_index != dense_count) || keyIndex < 0) {
                            assoc_count++;
                        } else {
                            dense_count++;
                        }
                        break;
                    case HASH_KEY_IS_STRING:
                        assoc_count++;
                        break;
                }
            }

            if ((var_hash->flags & AMF3_NSND_ARRAY_AS_OBJECT) != 0 && (assoc_count > 0 || max_index != (int) dense_count - 1)) {
                amf3_write_objecthead(buf, AMF_INLINE_ENTITY | AMF_INLINE_CLASS | AMF_CLASS_DYNAMIC AMFTSRMLS_CC);
                amf3_write_emptystring(buf AMFTSRMLS_CC);
                var_hash->nextTraitIndex++; //http://pecl.php.net/bugs/bug.php?id=12668&edit=1
                zend_hash_internal_pointer_reset_ex(myht, &pos);
                for (;; zend_hash_move_forward_ex(myht, &pos)) {
                    int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                    if (keyType == HASH_KEY_NON_EXISTANT) {
                        break;
                    }
                    switch (keyType) {
                        case HASH_KEY_IS_LONG:
                            {
                                char txt[20];
                                sprintf(txt, "%d", keyIndex);
                                amf3_write_string(buf, txt, strlen(txt), AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                            }
                            break;
                        case HASH_KEY_IS_STRING:
                            amf3_write_string(buf, key, key_len - 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                            break;
                    }
                    if (zend_hash_get_current_data_ex(myht, (void **) &data, &pos) != SUCCESS || !data) {
                        amf_write_byte(buf, AMF3_UNDEFINED);
                    } else {
                        amf3_serialize_var(buf, data, var_hash TSRMLS_CC);
                    }
                }
                amf3_write_emptystring(buf AMFTSRMLS_CC);
            } else {
                amf_write_byte(buf, AMF3_ARRAY);
                amf3_write_uint29(buf, (dense_count << 1) | AMF_INLINE_ENTITY AMFTSRMLS_CC);

                 /* string keys */
                if (assoc_count > 0) {
                    zend_hash_internal_pointer_reset_ex(myht, &pos);
                    for (;; zend_hash_move_forward_ex(myht, &pos)) {
                        int skip = 0;
                        int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                        if (keyType == HASH_KEY_NON_EXISTANT)
                        {
                            break;
                        }
                        switch (keyType) {
                            case HASH_KEY_IS_LONG:
                                if (keyIndex < 0 || keyIndex > (int) dense_count) {
                                    char txt[20];
                                    sprintf(txt, "%d", keyIndex);
                                    amf3_write_string(buf, txt, strlen(txt), AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                                } else {
                                    skip = 1; // numeric keys are written in sequential mode
                                }
                                break;
                            case HASH_KEY_IS_STRING:
                                amf3_write_string(buf, key, key_len - 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                                break;
                        }
                        if (skip) {
                            continue;
                        }

                        if (zend_hash_get_current_data_ex(myht, (void **) &data, &pos) == SUCCESS && data != NULL)  {
                            amf3_serialize_var(buf, data, var_hash TSRMLS_CC);
                        } else {
                            amf_write_byte(buf, AMF3_UNDEFINED);
                        }
                    }
                }

                /* place the empty string to indicate the end of key/value pairs */
                amf3_write_emptystring(buf AMFTSRMLS_CC);

                /* now the linear data, we need to lookup the data because of the sorting */
                if (dense_count > 0) {
                    int iIndex;

                    /* lookup the key if existent (use 0x0 undefined */
                    for (iIndex = 0; iIndex < (int) dense_count; iIndex++) {
                        if (zend_hash_index_find(myht, iIndex, (void **) &data) == FAILURE) {
                            amf_write_byte(buf, AMF3_UNDEFINED);
                        } else {
                            amf3_serialize_var(buf, data, var_hash TSRMLS_CC);
                        }
                    }
                }
            }
        }
    } else {
        /* just an empty array */
        amf_write_byte(buf, AMF3_ARRAY);
        amf3_write_uint29(buf, 0 | 1 AMFTSRMLS_CC);
        amf3_write_emptystring(buf AMFTSRMLS_CC);
    }
}

static void amf3_serialize_vector(amf_serialize_output buf, HashTable *myht, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    if (myht && zend_hash_num_elements(myht) != 0) {
        zval **type, **fixed, **data, **value;
        HashTable *htData;
        HashPosition pos;
        int len, keyIndex, keyType, skip;
        char *key;
        uint key_len;

        if (zend_hash_find(myht, "type", sizeof("type"), (void **) &type) == SUCCESS
            && zend_hash_find(myht, "fixed", sizeof("fixed"), (void **) &fixed) == SUCCESS
            && zend_hash_find(myht, "data", sizeof("data"), (void **) &data) == SUCCESS
            && Z_TYPE_PP(type) == IS_LONG
            && Z_TYPE_PP(fixed) == IS_BOOL
            && Z_TYPE_PP(data) == IS_ARRAY
        ) {
            htData = HASH_OF(*data);
            len = zend_hash_num_elements(htData);

            amf_write_byte(buf, Z_LVAL_PP(type));
            amf3_write_uint29(buf, (len << 1) | AMF_INLINE_ENTITY AMFTSRMLS_CC);
            amf_write_byte(buf, Z_LVAL_PP(fixed) ? 1 : 0);

            if (Z_LVAL_PP(type) == AMF3_VECTOR_OBJECT) {
                zend_hash_internal_pointer_reset_ex(htData, &pos);
                    for (;; zend_hash_move_forward_ex(htData, &pos)) {
                        skip = 0;
                        keyType = zend_hash_get_current_key_ex(htData, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                        if (keyType == HASH_KEY_NON_EXISTANT) {
                            break;
                        }
                        switch (keyType) {
                            case HASH_KEY_IS_LONG:

                                break;
                            case HASH_KEY_IS_STRING:
                                skip = 1;
                                break;
                        }
                        if (zend_hash_get_current_data_ex(htData, (void **) &value, &pos) != SUCCESS || Z_TYPE_PP(value) != IS_OBJECT || skip) {
                            amf_write_byte(buf, AMF3_UNDEFINED);
                        } else {
                            if (keyIndex == 0) {
                                const char *className = "";
                                zend_uint zClassNameLen = 0;
                                int explicitTypeIsSet = 0, classNameLen = 0;
                                zval **explicitType;
                                HashTable *obj = Z_OBJPROP_PP(value);

                                zend_get_object_classname(*value, &className, &zClassNameLen TSRMLS_CC);

                                explicitTypeIsSet = zend_hash_exists(obj, "_explicitType", sizeof("_explicitType"));
                                if (explicitTypeIsSet == 1) {
                                    zend_hash_find(obj, "_explicitType", sizeof("_explicitType"), (void **) &explicitType);
                                }

                                if ((strcmp(className, "stdClass") == 0 && explicitTypeIsSet == 0)
                                    || (explicitTypeIsSet == 1 && Z_TYPE_PP(explicitType) == IS_STRING && Z_STRVAL_PP(explicitType) == "")
                                ) {
                                    className = "Object";
                                    classNameLen = strlen(className);
                                } else if (explicitTypeIsSet == 1) {
                                    className = Z_STRVAL_PP(explicitType);
                                } else {
                                    char *tmp = (char *) className;
                                    className = php_str_to_str(tmp, strlen(tmp), "\\", sizeof("\\") - 1, ".", sizeof(".") - 1, &classNameLen);
                                }
                                amf3_write_string(buf, className, classNameLen, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                            }
                            amf3_serialize_object(buf, value, var_hash TSRMLS_CC);
                        }
                    }
            } else {
                if (Z_LVAL_PP(type) == AMF3_VECTOR_INT) {
                    zend_hash_internal_pointer_reset_ex(htData, &pos);
                    for (;; zend_hash_move_forward_ex(htData, &pos)) {
                        skip = 0;
                        keyType = zend_hash_get_current_key_ex(htData, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                        if (keyType == HASH_KEY_NON_EXISTANT) {
                            break;
                        }
                        switch (keyType) {
                            case HASH_KEY_IS_LONG:

                                break;
                            case HASH_KEY_IS_STRING:
                                skip = 1;
                                break;
                        }
                        if (zend_hash_get_current_data_ex(htData, (void **) &value, &pos) != SUCCESS || Z_TYPE_PP(value) != IS_LONG || skip) {
                            amf_write_byte(buf, AMF3_UNDEFINED);
                        } else {
                            amf3_write_vector_int(buf, Z_LVAL_PP(value), var_hash AMFTSRMLS_CC);
                        }
                    }
                } else if (Z_LVAL_PP(type) == AMF3_VECTOR_UINT) {
                    zend_hash_internal_pointer_reset_ex(htData, &pos);
                    for (;; zend_hash_move_forward_ex(htData, &pos)) {
                        skip = 0;
                        keyType = zend_hash_get_current_key_ex(htData, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                        if (keyType == HASH_KEY_NON_EXISTANT) {
                            break;
                        }
                        switch (keyType) {
                            case HASH_KEY_IS_LONG:

                                break;
                            case HASH_KEY_IS_STRING:
                                skip = 1;
                                break;
                        }
                        if (zend_hash_get_current_data_ex(htData, (void **) &value, &pos) != SUCCESS || (Z_TYPE_PP(value) != IS_LONG && Z_TYPE_PP(value) != IS_DOUBLE) || skip) {
                            amf_write_byte(buf, AMF3_UNDEFINED);
                        } else {
                            if (Z_TYPE_PP(value) == IS_LONG) {
                                amf3_write_vector_uint(buf, Z_LVAL_PP(value), var_hash AMFTSRMLS_CC);
                            } else {
                                amf3_write_vector_uint(buf, Z_DVAL_PP(value), var_hash AMFTSRMLS_CC);
                            }
                        }
                    }
                } else if (Z_LVAL_PP(type) == AMF3_VECTOR_DOUBLE) {
                    zend_hash_internal_pointer_reset_ex(htData, &pos);
                    for (;; zend_hash_move_forward_ex(htData, &pos)) {
                        skip = 0;
                        keyType = zend_hash_get_current_key_ex(htData, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                        if (keyType == HASH_KEY_NON_EXISTANT) {
                            break;
                        }
                        switch (keyType) {
                            case HASH_KEY_IS_LONG:

                                break;
                            case HASH_KEY_IS_STRING:
                                skip = 1;
                                break;
                        }
                        if (zend_hash_get_current_data_ex(htData, (void **) &value, &pos) != SUCCESS || (Z_TYPE_PP(value) != IS_LONG && Z_TYPE_PP(value) != IS_DOUBLE) || skip) {
                            amf_write_byte(buf, AMF3_UNDEFINED);
                        } else {
                            if (Z_TYPE_PP(value) == IS_LONG) {
                                amf3_write_vector_double(buf, (double)Z_LVAL_PP(value), var_hash AMFTSRMLS_CC);
                            } else {
                                amf3_write_vector_double(buf, Z_DVAL_PP(value), var_hash AMFTSRMLS_CC);
                            }
                        }
                    }
                }
            }
        }
    }
}

static void amf3_serialize_var(amf_serialize_output buf, zval **struc, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    ulong objectIndex;

    switch (Z_TYPE_PP(struc)) {
        case IS_BOOL:
            amf_write_byte(buf, Z_LVAL_PP(struc) != 0 ? AMF3_TRUE : AMF3_FALSE);
            return;
        case IS_NULL:
            amf_write_byte(buf, AMF3_NULL);
            return;
        case IS_LONG:
            /* AMF3 integer: b(4) ber encoding(1-4) only if not too big 29 bit */
            {
                long d = Z_LVAL_PP(struc);
                if (d >= -268435456 && d <= 268435455) {
                    amf_write_byte(buf, AMF3_INTEGER);
                    amf3_write_uint29(buf, d AMFTSRMLS_CC);
                } else {
                    amf_write_byte(buf, AMF3_NUMBER);
                    amf_write_double(buf, d, var_hash AMFTSRMLS_CC);
                }
            }
            return;
        case IS_DOUBLE:
            amf_write_byte(buf, AMF3_NUMBER);
            amf_write_double(buf, Z_DVAL_PP(struc), var_hash AMFTSRMLS_CC);
            return;
        case IS_STRING:
            amf_write_byte(buf, AMF3_STRING);
            amf3_write_string_zval(buf, *struc, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
            return;
        case IS_RESOURCE:
        case IS_OBJECT:
            amf3_serialize_object(buf, struc, var_hash TSRMLS_CC);
            return;
        case IS_ARRAY:
            if (amf_cache_zval(&(var_hash->objects), HASH_OF(*struc), &objectIndex, &(var_hash->nextObjectIndex), 0) == FAILURE) {
                amf_write_byte(buf, AMF3_ARRAY);
                amf3_write_uint29(buf, (objectIndex << 1) AMFTSRMLS_CC);
            } else {
                amf3_serialize_array(buf, HASH_OF(*struc), var_hash TSRMLS_CC);
            }
            return;
        default:
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf unknown PHP type %d\n", Z_TYPE_PP(struc));
            amf_write_byte(buf, AMF3_UNDEFINED);
            return;
     }
}

/** serializes an array in AMF0 format It checks form _amf_recordset_ */
static void amf0_serialize_array(amf_serialize_output buf, HashTable *myht, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    int num_elements = zend_hash_num_elements(myht);
    if (num_elements != 0) {
        char *key;
        int keyIndex;
        HashPosition pos;
        uint key_len;
        uint str_count = 0, num_count = 0;
        int has_negative = 0;
        int max_index = -1;

        /* Special Handling for arrays with __amf_recordset__ AMF0: no ArrayCollection */
        if (amf_get_assoc_long(myht, "__amf_recordset__", 0) != AMFR_NONE) {
            zval **columns, **rows, **id;
            HashTable *htRows;
            HashTable *htColumns;
            int nColumns, nRows, iRow;

            if (zend_hash_find(myht, "rows", sizeof("rows"), (void **) &rows) == SUCCESS
                && zend_hash_find(myht, "columns", sizeof("columns"), (void **) &columns) == SUCCESS
                && Z_TYPE_PP(rows) == IS_ARRAY
                && Z_TYPE_PP(columns) == IS_ARRAY
            ) {
                id = NULL;
                zend_hash_find(myht, "id", sizeof("id"), (void **) &id);
                htRows = HASH_OF(*rows);
                htColumns = HASH_OF(*columns);
                nColumns = zend_hash_num_elements(htColumns);
                nRows = zend_hash_num_elements(htRows);

                 /* typedobject class=RecordSet */
                amf_write_byte(buf, AMF0_TYPEDOBJECT);
                amf0_write_string(buf, "RecordSet", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);

                amf0_write_string(buf, "serverInfo", AMF_STRING_AS_SAFE_TEXT, var_hash  TSRMLS_CC);
                amf_write_byte(buf, AMF0_OBJECT);
                var_hash->nextObject0Index++;
                {
                    amf0_write_string(buf, "version", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                    amf_write_byte(buf, AMF0_NUMBER);
                    amf_write_double(buf, 1, var_hash AMFTSRMLS_CC);

                    amf0_write_string(buf, "totalCount", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                    amf_write_byte(buf, AMF0_NUMBER);
                    amf_write_double(buf, nRows, var_hash AMFTSRMLS_CC);

                    amf0_write_string(buf, "cursor", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                    amf_write_byte(buf, AMF0_NUMBER);
                    amf_write_double(buf, 1, var_hash AMFTSRMLS_CC);

                    amf0_write_string(buf, "serviceName", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                    amf0_serialize_string(buf, "PageAbleResult", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);

                    amf0_write_string(buf, "id", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                    if (id != NULL) {
                        amf0_serialize_var(buf, id, var_hash TSRMLS_CC);
                    } else {
                        amf0_serialize_emptystring(buf AMFTSRMLS_CC);
                    }

                    amf0_write_string(buf, "columnNames", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                    amf_write_byte(buf, AMF0_ARRAY);
                    amf0_write_int(buf, nColumns AMFTSRMLS_CC); {
                        int iColumn;
                        for(iColumn = 0; iColumn < nColumns; iColumn++) {
                            zval **columnName;
                            zend_hash_index_find(htColumns, iColumn, (void **) &columnName);
                            if (Z_TYPE_PP(columnName) != IS_STRING) {
                                amf0_serialize_string(buf, "unk", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                            } else {
                                amf0_serialize_zstring(buf, *columnName, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                            }
                        }
                    }

                    amf0_write_string(buf, "initialData", AMF_STRING_AS_SAFE_TEXT, var_hash TSRMLS_CC);
                    amf_write_byte(buf, AMF0_ARRAY);
                    amf0_write_int(buf, nRows AMFTSRMLS_CC); {
                        iRow = 0;
                        zend_hash_internal_pointer_reset_ex(htRows, &pos);
                        for (;; zend_hash_move_forward_ex(htRows, &pos)) {
                            int nColumnSizeOfRow;
                            int iColumn;
                            HashTable *htRow;
                            zval **zRow;
                            HashPosition posRow;

                            int keyType = zend_hash_get_current_key_ex(htRows, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
                            if (keyType != HASH_KEY_IS_LONG) {
                                break;
                            }
                            if (zend_hash_get_current_data_ex(htRows, (void **) &zRow, &pos) != SUCCESS || !zRow || Z_TYPE_PP(zRow) != IS_ARRAY) {
                                amf_write_byte(buf, AMF3_UNDEFINED);
                                continue;
                            }
                            htRow = HASH_OF(*zRow);
                            amf_write_byte(buf, AMF0_ARRAY);
                            amf0_write_int(buf, nColumns AMFTSRMLS_CC);
                            nColumnSizeOfRow = zend_hash_num_elements(htRow);
                            if (nColumnSizeOfRow > nColumns) { // long row
                                nColumnSizeOfRow = nColumns;
                            }
                            zend_hash_internal_pointer_reset_ex(htRow, &posRow);
                            for (iColumn = 0; iColumn < nColumnSizeOfRow; zend_hash_move_forward_ex(htRow, &posRow)) {
                                zval **zValue;
                                int keyType = zend_hash_get_current_key_ex(htRow, &key, &key_len, (ulong *) &keyIndex, 0, &posRow);
                                if (keyType != HASH_KEY_IS_LONG) {
                                    break;
                                }
                                if (zend_hash_get_current_data_ex(htRow, (void **) &zValue, &posRow) != SUCCESS) {
                                    zValue = NULL;
                                }
                                if (zValue == NULL) {
                                    amf_write_byte(buf, AMF0_UNDEFINED);
                                } else {
                                    amf0_serialize_var(buf, zValue, var_hash TSRMLS_CC);
                                }
                                iColumn++;
                            }

                            /* short row */
                            for (; iColumn < nColumns; iColumn++) {
                                amf_write_byte(buf, AMF0_UNDEFINED);
                            }
                            iRow++;
                        }
                    }
                    amf0_write_objectend(buf AMFTSRMLS_CC); // serverInfo
                }
                amf0_write_objectend(buf AMFTSRMLS_CC); // returned RecordSet
                return;
            } else {
                php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot understand special recordset array: should have __AMF_recordset__, rows and columns keys");
            }
        }

         /* first check if it is a mixed (8) or a numeric object */
        zend_hash_internal_pointer_reset_ex(myht, &pos);
        for (;; zend_hash_move_forward_ex(myht, &pos)) {
            int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &keyIndex, 0, &pos);
            if (keyType == HASH_KEY_NON_EXISTANT) {
                break;
            }
            switch (keyType) {
            case HASH_KEY_IS_LONG:
                    if (keyIndex > max_index) {
                        max_index = keyIndex;
                    }
                    if (keyIndex < 0) {
                        has_negative = 1;
                        str_count++;
                    } else {
                        num_count++;
                    }
                    break;
                case HASH_KEY_IS_STRING:
                    str_count++;
                    break;
            }
        }

        /* key with name or negative indices means mixed array */
        if (num_count > 0 && (str_count > 0 || has_negative))
        {
            amf_write_byte(buf, AMF0_MIXEDARRAY);
            amf0_write_int(buf, num_elements AMFTSRMLS_CC);
            amf0_serialize_objectdata(buf, myht, 1, var_hash TSRMLS_CC);
        } else if (num_count > 0) { // numeric keys only
            int iIndex;
            amf_write_byte(buf, AMF0_ARRAY);
            if (max_index == num_count - 1) {
                /* dense array */
                amf0_write_int(buf, num_count AMFTSRMLS_CC);
                /* lookup the key, if non-existent use 0x6 undefined */
                for (iIndex = 0; iIndex < (int) num_count; iIndex++) {
                    zval **zzValue;
                    if (zend_hash_index_find(myht, iIndex, (void **) &zzValue) == FAILURE) {
                        amf_write_byte(buf, AMF0_UNDEFINED);
                    } else {
                        amf0_serialize_var(buf, zzValue, var_hash TSRMLS_CC);
                    }
                }
            } else {
                /* sparse array */
                amf0_write_int(buf, max_index + 1 AMFTSRMLS_CC);
                /* lookup the key, if non-existent use 0x6 undefined */
                for (iIndex = 0; iIndex < (int) max_index + 1; iIndex++) {
                    zval **zzValue;
                    if (zend_hash_index_find(myht, iIndex, (void **) &zzValue) == FAILURE) {
                        amf_write_byte(buf, AMF0_UNDEFINED);
                    } else {
                        amf0_serialize_var(buf, zzValue, var_hash TSRMLS_CC);
                    }
                }
            }
        } else { // string keys only
            amf_write_byte(buf, AMF0_OBJECT);
            amf0_serialize_objectdata(buf, myht, 1, var_hash TSRMLS_CC);
        }
        return;
    } else {
        static char emptyArray[] = {10,0,0,0,0};
        amf_write_string(buf, emptyArray, 5 AMFTSRMLS_CC);
    }
}

static void amf0_serialize_var(amf_serialize_output buf, zval **struc, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    ulong objectIndex;

    switch (Z_TYPE_PP(struc)) {
        case IS_BOOL:
            amf_write_byte(buf, AMF0_BOOLEAN);
            amf_write_byte(buf, Z_LVAL_PP(struc) ? 1 : 0);
            return;
        case IS_NULL:
            amf_write_byte(buf, AMF0_NULL);
            return;
        case IS_LONG:
            amf_write_byte(buf, AMF0_NUMBER);
            amf_write_double(buf, Z_LVAL_PP(struc), var_hash AMFTSRMLS_CC);
            return;
        case IS_DOUBLE:
            if (Z_DVAL_PP(struc) > INT_MAX) {
                convert_to_string(*struc);
                //amf0_serialize_zstring(buf, *struc, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                amf_write_byte(buf, AMF0_STRING);
                amf0_write_short(buf, Z_STRLEN_P(*struc) AMFTSRMLS_CC);  
                amf_write_zstring(buf, *struc AMFTSRMLS_CC);
            } else {
                amf_write_byte(buf, AMF0_NUMBER);
                amf_write_double(buf, Z_DVAL_PP(struc), var_hash AMFTSRMLS_CC);
            }
            return;
        case IS_STRING:
            amf0_serialize_zstring(buf, *struc, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
            return;
        case IS_RESOURCE:
        case IS_OBJECT:
            amf0_serialize_object(buf, struc, var_hash TSRMLS_CC);
            return;
        case IS_ARRAY:
            if (amf_cache_zval(&(var_hash->objects0), HASH_OF(*struc), &objectIndex, &(var_hash->nextObject0Index), 0) == FAILURE)
            {
                amf_write_byte(buf, AMF0_REFERENCE);
                amf0_write_short(buf, objectIndex AMFTSRMLS_CC);
            } else {
                amf0_serialize_array(buf, HASH_OF(*struc), var_hash TSRMLS_CC);
            }
            return;
        default:
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot understand php type %d", Z_TYPE_PP(struc));
            amf_write_byte(buf, AMF0_UNDEFINED);
            return;
    }
}

/** appends something to sb */
static void _amf_sb_append(amf_serialize_output buf, zval *zd, int do_copy TSRMLS_DC)
{
    switch (Z_TYPE_P(zd)) {
        case IS_ARRAY: 
            {
                HashTable *myht = HASH_OF(zd);
                HashPosition pos;

                zend_hash_internal_pointer_reset_ex(myht, &pos);
                for (;; zend_hash_move_forward_ex(myht, &pos)) {
                    zval **zValue = NULL;
                    char *key;
                    uint key_len;
                    int key_index;
                    int keyType = zend_hash_get_current_key_ex(myht, &key, &key_len, (ulong *) &key_index, 0, &pos);

                    if (keyType == HASH_KEY_NON_EXISTANT) {
                        break;
                    }

                    if (zend_hash_get_current_data_ex(myht, (void **) &zValue, &pos) == SUCCESS) {
                        _amf_sb_append(buf, *zValue, do_copy TSRMLS_CC);
                    }
                }
            }
            break;
        case IS_RESOURCE: 
            {
                amf_serialize_output sbc = NULL;
                sbc = (amf_serialize_output) zend_fetch_resource(&zd TSRMLS_CC, -1, PHP_AMF_STRING_BUILDER_RES_NAME, NULL, 1, amf_serialize_output_resource_reg);
                if (sbc != NULL) {
                    amf_serialize_output_append_sb(buf, sbc, 0);
                }
            }
            break;
        default:
            convert_to_string(zd);
            amf_write_zstring(buf, zd AMFTSRMLS_CC);
    }
}


/**
 * function for joining multiple strings using the buffer of this extension
 * it accepts a lot of parameters and if a parameter is an array it traverses it
 */
PHP_FUNCTION(amf_sb_join_test)
{
    int i;
    int argc = ZEND_NUM_ARGS();
    zval **params[10];
#ifdef amf_USE_STRING_BUILDER
    amf_serialize_output_t buf;
    amf_serialize_output pbuf = &buf;
    amf_serialize_output_ctor(&buf);
#else
    amf_serialize_output pbuf = php_stream_memory_create(0);
#endif
    if (argc > sizeof(params) / sizeof(params[0])) {
        argc = sizeof(params) / sizeof(params[0]);
    }

    if (zend_parse_parameters(argc TSRMLS_CC, "Z|Z", &params[0], &params[1], &params[2], &params[3], &params[4],
        &params[5], &params[6], &params[7], &params[8], &params[9]) == FAILURE
    ) {
        return;
    }

    for (i = 0; i < argc; i++) {
        _amf_sb_append(pbuf, *params[i], 1 TSRMLS_CC);
    }

#ifdef amf_USE_STRING_BUILDER
    amf_serialize_output_get(pbuf, return_value);
    amf_serialize_output_dtor(pbuf);
#else
    {
        size_t memsize;
        char *membuf = php_stream_memory_get_buffer(pbuf, &memsize);
        RETURN_STRINGL(membuf, memsize, 1);
        php_stream_close(pbuf);
    }
#endif
}

/**
 * encodes a string into amf format
 * \param value to be ancoded
 * \param flags for encoding AMF_AMF3 AMF_BIGENDIAN
 * \param callback (array or single functionreference)
 */
PHP_FUNCTION(amf_encode)
{
    zval **struc, **strucFlags, **zzCallback = NULL, **zzOutputSB = NULL;
    int flags = 0;
    int asSB = 0;  // 0 = no, 1 = is received, 2 = is create
    amf_serialize_data_t var_hash;
#ifdef amf_USE_STRING_BUILDER
    amf_serialize_output_t buf;
    amf_serialize_output pbuf = &buf;
    amf_serialize_output_ctor(&buf);
#else
    amf_serialize_output pbuf = php_stream_memory_create(0);
#endif

    switch (ZEND_NUM_ARGS()) {
        case 0: 
            WRONG_PARAM_COUNT; 
            return;
        case 1:
            if (zend_parse_parameters(1 TSRMLS_CC, "Z", &struc) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            break;
        default:
            if (zend_parse_parameters((ZEND_NUM_ARGS() > 4 ? 4 : ZEND_NUM_ARGS()) TSRMLS_CC, "ZZZ|Z!", &struc, &strucFlags, &zzCallback, &zzOutputSB) == FAILURE || Z_TYPE_PP(strucFlags) != IS_LONG) {
                WRONG_PARAM_COUNT;
            }
            flags = Z_LVAL_PP(strucFlags);
            break;
    }
#ifdef amf_USE_STRING_BUILDER

    /* if we explicitly pass a SB use it */
    if (zzOutputSB != NULL && Z_TYPE_PP(zzOutputSB) == IS_RESOURCE) {
        amf_serialize_output tpbuf = NULL;
        tpbuf = (amf_serialize_output) zend_fetch_resource(zzOutputSB TSRMLS_CC, -1, PHP_AMF_STRING_BUILDER_RES_NAME, NULL, 1, amf_serialize_output_resource_reg);
        if (tpbuf != NULL) {
            amf_serialize_output_dtor(pbuf);
            pbuf = tpbuf;
            asSB = 1;
            /* Z_ADDREF_P(*zzOutputSB); */
            /* return_value = *zzOutputSB; */
        }
    }

     /* if the user requested a sb and not passed one then enter in SB mode */
    if ((flags & AMF_AS_STRING_BUILDER) != 0 && asSB == 0) {
        pbuf = (amf_serialize_output) emalloc(sizeof(amf_serialize_output_t));
        amf_serialize_output_ctor(pbuf);
        ZEND_REGISTER_RESOURCE(return_value, pbuf, amf_serialize_output_resource_reg);
    }
#endif

    Z_TYPE_P(return_value) = IS_STRING;
    Z_STRVAL_P(return_value) = NULL;
    Z_STRLEN_P(return_value) = 0;
    var_hash.flags = flags;

    amf_SERIALIZE_CTOR(var_hash, zzCallback)
    if ((flags & AMF_AMF3) != 0) {
        amf_write_byte(pbuf, AMF0_AMF3);
        amf3_serialize_var(pbuf, struc, &var_hash TSRMLS_CC);
    } else {
        amf0_serialize_var(pbuf, struc, &var_hash TSRMLS_CC);
    }
#ifdef amf_USE_STRING_BUILDER
    /* flat on regular return as string */
    if (asSB == 0) {
        amf_serialize_output_get(pbuf, return_value);
    }

    /* deallocate if it was wasted */
    if (asSB != 1) {
        amf_serialize_output_dtor(&buf);
    }
#else
    {
        size_t memsize;
        char *membuf = php_stream_memory_get_buffer(pbuf, &memsize);
        RETURN_STRINGL(membuf, memsize, 1);
        php_stream_close(pbuf);
    }
#endif
    amf_SERIALIZE_DTOR(var_hash);
}

/* Decoding {{{1*/

static int amf0_deserialize_var(zval **rval, const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash TSRMLS_DC);
static int amf3_deserialize_var(zval **rval, const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash TSRMLS_DC);



static int amf_perform_deserialize_callback(int ievent, zval *arg0, zval **zResultValue, int shared, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    if (var_hash->callbackFx != NULL) {
        int r;  /* result from function */
        zval *zEmpty1 = NULL, *zievent;
        zval *zResultValuePtr;
        zval *arg0orig = arg0;
        MAKE_STD_ZVAL(zievent);
        ZVAL_LONG(zievent, ievent);

        if (arg0 == NULL) {
            MAKE_STD_ZVAL(zEmpty1);
            ZVAL_NULL(zEmpty1);
        }

        {
            zval **params[2] = { &zievent, arg0 == NULL ? &zEmpty1 : &arg0 };
            if ((r = call_user_function_ex(CG(function_table), var_hash->callbackTarget, var_hash->callbackFx, &zResultValuePtr, 2, params, 0, NULL TSRMLS_CC)) == SUCCESS) {
                /* if the result is different from the original value we cannot rely on that zval* if it is not empty */
                if (arg0 != arg0orig) {
                    zval_add_ref(&arg0orig);
                }

                if (zResultValuePtr != *zResultValue && zResultValuePtr != NULL) {
                    if (*zResultValue == NULL) {
                        MAKE_STD_ZVAL(*zResultValue);
                    } else if (shared != 0) { // cannot replace the zval
                        zval_ptr_dtor(zResultValue);
                        MAKE_STD_ZVAL(*zResultValue);
                    }
                    COPY_PZVAL_TO_ZVAL(**zResultValue, zResultValuePtr);
                }
            }
        }
        zval_ptr_dtor(&zievent);
        if (zEmpty1 != NULL) {
            zval_ptr_dtor(&zEmpty1);
        }
        return r;
    } else {
        return FAILURE;
    }
}

/**
 * obtains an object from the cache
 * \param rval is the pointer to reference
 * \return SUCCESS or FAILURE
 * No reference count is changed
 */
static inline int amf_get_from_cache(HashTable *ht, zval **rval, int index)
{
    zval **px;
    if (zend_hash_index_find(ht, index, (void **) &px) == FAILURE) {
        return FAILURE;
    } else {
        *rval = *px;
        return SUCCESS;
    }
}

/** places an entity in the cache with no change in reference */
static inline int amf_put_in_cache(HashTable *var_hash, zval *var)
{
    zend_hash_next_index_insert(var_hash, &var, sizeof(zval *), NULL);
    return SUCCESS;
}

/** reads an integer in AMF0 format */
static int amf_read_int(const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash)
{
    const unsigned char *cp = *p;
    *p += 4;
    return ((cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | cp[3]);
}

/** reads a short integer in AMF0 format */
static int amf_read_short(const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash)
{
    const unsigned char *cp = *p;
    *p += 2;
    return ((cp[0] << 8) | cp[1]);
}

/** reads a double in AMF0 format, eventually flipping it for bigendian */
static double amf_read_double(const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash)
{
     /* this structure is used to have proper double alignment */
    union aligned {
        double dval;
        char cval[8];
    } d;
    const unsigned char *cp = *p;
    *p += 8;
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        d.cval[0] = cp[7]; d.cval[1] = cp[6]; d.cval[2] = cp[5]; d.cval[3] = cp[4];
        d.cval[4] = cp[3]; d.cval[5] = cp[2]; d.cval[6] = cp[1]; d.cval[7] = cp[0];
    } else {
        memcpy(d.cval, cp, 8);
    }
    return d.dval;
}

/** reads an integer in AMF3 format */
static int amf3_read_uint29(const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash)
{
    const unsigned char *cp = *p;

    int acc = *cp++;
    int mask, r, tmp;
    if (acc < 128) {
        *p = cp;
        return acc;
    } else {
        acc = (acc & 0x7f) << 7;
        tmp = *cp++;
        if (tmp < 128) {
            acc = acc | tmp;
        } else {
            acc = (acc | tmp & 0x7f) << 7;
            tmp = *cp++;
            if (tmp < 128) {
                acc = acc | tmp;
            } else {
                acc = (acc | tmp & 0x7f) << 8;
                tmp = *cp++;
                acc = acc | tmp;
            }
        }
        *p = cp;
    }
    /* 
    To sign extend a value from some number of bits to a greater number of bits,
    just copy the sign bit into all the additional bits in the new format
    convert/sign extend the 29bit two's complement number to 32 bit 
    */
    mask = 1 << 28;  // mask
    r = -(acc & mask) | acc;
    return r;
}

/**
 * reads a string in AMF format, with the specified size
 * \param rrval is modified into string with correct size
 */
static int amf_read_string(zval **rval, const unsigned char **p, const unsigned char *max, int length, enum AMFStringData raw, amf_deserialize_data_t *var_hash TSRMLS_DC)
{
    const unsigned char *src = *p;
    *p += length;
    if (length > 0 && raw == AMF_STRING_AS_TEXT && (var_hash->flags & AMF_TRANSLATE_CHARSET) != 0) {
        zval *rv;
        if ((rv = amf_translate_charset_string(src, length, AMF_FROM_UTF8, var_hash TSRMLS_CC)) != 0) {
            *rval = rv;
            return SUCCESS;
        }
    }
    ZVAL_STRINGL(*rval, (char *) src, length, 1);
    return SUCCESS;
}

/**
 * reads a string in AMF format, with the specified size
 * \param rrval is modified into string with correct size
 */
static int amf0_read_string(zval **rval, const unsigned char **p, const unsigned char *max, int length, enum AMFStringData raw, amf_deserialize_data_t *var_hash TSRMLS_DC)
{
    int slength = length == 2 ? amf_read_short(p, max, var_hash) : amf_read_int(p, max, var_hash);
    const unsigned char *src = *p;
    *p += slength;
    if (slength > 0 && raw == AMF_STRING_AS_TEXT && (var_hash->flags & AMF_TRANSLATE_CHARSET) != 0) {
        zval *rv;
        if ((rv = amf_translate_charset_string(src, slength, AMF_FROM_UTF8, var_hash TSRMLS_CC)) != 0) {
            *rval = rv;
            return SUCCESS;
        }
    }
    ZVAL_STRINGL(*rval, (char *) src, slength, 1);
    return SUCCESS;
}

/**
 * Reads a string in AMF3 format with caching
 * \param storeReference tells to place the string in the cache or not
 * \param rval is the new pointer
 * \return PHP success code
 *
 * Note: the reference count is not changed
*/
static int amf3_read_string(zval **rval, const unsigned char **p, const unsigned char *max, int storeReference, enum AMFStringData raw, amf_deserialize_data_t *var_hash TSRMLS_DC)
{
    int len = amf3_read_uint29(p, max, var_hash);
    if (len == 1) {
        *rval = var_hash->zEmpty_string;
    } else if ((len & AMF_INLINE_ENTITY) != 0) {
        const unsigned char *src = *p;
        zval *newval = NULL;
        len >>= 1;
        *p += len;

        if (!(raw == AMF_STRING_AS_TEXT && (var_hash->flags & AMF_TRANSLATE_CHARSET) != 0 && (newval = amf_translate_charset_string(src, len, AMF_FROM_UTF8, var_hash TSRMLS_CC)) != 0)) {
            MAKE_STD_ZVAL(newval);
            ZVAL_STRINGL(newval, (char *) src, len, 1);
        }

        if (storeReference == 1) {
            zend_hash_index_update(&(var_hash->strings), zend_hash_num_elements(&(var_hash->strings)), (void *) &newval, sizeof(zval*), NULL);
        } else {
            zval_dtor(newval);
        }
        *rval = newval;
    } else {
        return amf_get_from_cache(&(var_hash->strings), rval, (len >> 1));
    }
    return SUCCESS;
}

/** reads an integer in AMF3 vector format */
static int amf3_read_vector_int(const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash)
{
    union aligned {
        int ival;
        char cval[4];
    } i;
    const unsigned char *cp = *p;
    *p += 4;
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        i.cval[0] = cp[3]; i.cval[1] = cp[2]; i.cval[2] = cp[1]; i.cval[3] = cp[0];
    } else {
        memcpy(i.cval, cp, 4);
    }
    return i.ival;
}

/** reads an unsigned integer in AMF3 vector format */
static unsigned int amf3_read_vector_uint(const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash)
{
    union aligned {
        unsigned int Ival;
        char cval[4];
    } i;
    const unsigned char *cp = *p;
    *p += 4;
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        i.cval[0] = cp[3]; i.cval[1] = cp[2]; i.cval[2] = cp[1]; i.cval[3] = cp[0];
    } else {
        memcpy(i.cval, cp, 4);
    }
    return i.Ival;
}

/** reads a double in AMF0 format, eventually flipping it for bigendian */
static double amf3_read_vector_double(const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash)
{
     /* this structure is used to have proper double alignment */
    union aligned {
        double dval;
        char cval[8];
    } d;
    const unsigned char *cp = *p;
    *p += 8;
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        d.cval[0] = cp[7]; d.cval[1] = cp[6]; d.cval[2] = cp[5]; d.cval[3] = cp[4];
        d.cval[4] = cp[3]; d.cval[5] = cp[2]; d.cval[6] = cp[1]; d.cval[7] = cp[0];
    } else {
        memcpy(d.cval, cp, 8);
    }
    return d.dval;
}

/**
 * reads object data with
 * \param className the name of the class
 * \param asArray means to store the result in an associative array (className not meaningful)
 * \param maxIndex is the maximum index of the numerical part of the array, useful for optimization
 *
 * Eventually if flags has AMF_OBJECT_AS_ASSOC then an object is treated as an array
 */
static int amf0_read_objectdata(zval **rval, const unsigned char **p, const unsigned char *max, zval *zClassName, int asArray, int maxIndex, amf_deserialize_data_t *var_hash TSRMLS_DC)
{
     /* Cases */
     /* asArray means that we are building an associative array with up to maxIndex */
     /* flag associative */
     /* classname can be used as well */
    HashTable *htOutput = NULL;
    int callbackDone = 0;

     /* not an array and classname is not empty */
    if (asArray == 0 && zClassName != NULL && Z_STRLEN_P(zClassName) != 0) {
        if (amf_perform_deserialize_callback(AMFE_MAP, zClassName, rval, 0, var_hash TSRMLS_CC) == SUCCESS) {
            if (Z_TYPE_PP(rval) == IS_ARRAY) {
                asArray = 1;
                callbackDone = 1;
                htOutput = HASH_OF(*rval);
            } else if (Z_TYPE_PP(rval) == IS_OBJECT) {
                callbackDone = 1;
                htOutput = Z_OBJPROP_PP(rval);
            }
        }
    }

    if (callbackDone == 0) {
        if (asArray == 1 || (var_hash->flags & AMF_OBJECT_AS_ASSOC) != 0) {
            amf_array_init(*rval, maxIndex TSRMLS_CC);
            asArray = 1;
            htOutput = HASH_OF(*rval);
        } else if (zClassName != NULL)  {
            /* build the corresponding class */
            zend_class_entry **classEntry;

    #if PHP_MAJOR_VERSION >= 5
            if (zend_lookup_class(Z_STRVAL_P(zClassName), Z_STRLEN_P(zClassName), &classEntry TSRMLS_CC) != SUCCESS) {
    #else
            if (zend_hash_find(EG(class_table), Z_STRVAL_P(zClassName), Z_STRLEN_P(zClassName), (void **) &classEntry) != SUCCESS) {
    #endif
                php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot find class %s\n", Z_STRVAL_P(zClassName));
                object_init(*rval);
                /* return FAILURE */
            } else {
                object_init_ex(*rval, *classEntry);
            }
            htOutput = Z_OBJPROP_PP(rval);
        } else {
            object_init(*rval);
            htOutput = Z_OBJPROP_PP(rval);
        }
    }

    /* zval_add_ref(rval) */
    amf_put_in_cache(&(var_hash->objects0), *rval);

    while (1) {
        zval *zName;
        zval *zValue;
        MAKE_STD_ZVAL(zName);
        if (amf0_read_string(&zName, p, max, 2, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC) == FAILURE) {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot read string in array/object");
            return FAILURE;
        }
        if (**p == AMF0_ENDOBJECT) {
            *p = *p + 1;
            zval_ptr_dtor(&zName);
            break;
        }
        MAKE_STD_ZVAL(zValue);
        zValue->type = 0;
        if (amf0_deserialize_var(&zValue, p, max, var_hash TSRMLS_CC) == FAILURE) {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot deserialize key <%s>", Z_STRVAL_P(zName));
            zval_ptr_dtor(&zValue);
            zval_ptr_dtor(&zName);
            return FAILURE;
        }
        if (asArray == 1) {
            /* try to convert the string into a number */
            char *pEndOfString;
            char tmp[32];
            int keyLength = Z_STRLEN_P(zName);
            int iIndex;
            if (keyLength < sizeof(tmp)) {
                 /* TODO: use sscan */
                memcpy(tmp, Z_STRVAL_P(zName), keyLength);
                tmp[keyLength] = 0;
                iIndex = strtoul(tmp, &pEndOfString, 10);
            } else {
                iIndex = 0;
            }

            /* TODO test for key as 0 and key as " */
            if (iIndex != 0 && (pEndOfString == NULL || *pEndOfString == 0)) {
                zend_hash_index_update(htOutput, iIndex, &zValue, sizeof(zval*) , NULL);
            } else {
                add_assoc_zval(*rval, Z_STRVAL_P(zName), zValue);
            }
        } else if (Z_STRLEN_P(zName) > 0) {
            add_property_zval(*rval, Z_STRVAL_P(zName), zValue);
        } else {
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot set empty \"\" property for an object. Use AMF_OBJECT_AS_ASSOC flag");
        }
        zval_ptr_dtor(&zName);
    }

    if (Z_TYPE_PP(rval) == IS_ARRAY) {
        if (zClassName != NULL) {
            Z_ADDREF_P(zClassName);
            add_assoc_zval(*rval, "_explicitType", zClassName);
        }
    } else if ((var_hash->flags & AMF_POST_DECODE) != 0) {
        amf_perform_deserialize_callback(AMFE_POST_OBJECT, *rval, rval, 0, var_hash TSRMLS_CC);
    }
    return SUCCESS;
}

/**
 * generic deserialization in AMF3 format
 * \param rval a zval already allocated
 */
static int amf3_deserialize_var(zval **rval, const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash TSRMLS_DC)
{
    const int type = **p;
    int handle;

    *p = *p + 1;
    switch (type) {
        case AMF3_UNDEFINED:
        case AMF3_NULL:
            ZVAL_NULL(*rval);
            break;
        case AMF3_FALSE:
            ZVAL_BOOL(*rval, 0);
            break;
        case AMF3_TRUE:
            ZVAL_BOOL(*rval, 1);
            break;
        case AMF3_INTEGER:
            ZVAL_LONG(*rval, amf3_read_uint29(p, max, var_hash));
            break;
        case AMF3_NUMBER:
            ZVAL_DOUBLE(*rval, amf_read_double(p, max, var_hash));
            break;
        case AMF3_STRING:
            zval_ptr_dtor(rval);
            if (amf3_read_string(rval, p, max, 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC) == FAILURE) {
                php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup string");
                return FAILURE;
            }
            zval_add_ref(rval);
            break;
        case AMF3_XMLDOCUMENT:
        case AMF3_XML:
        case AMF3_BYTEARRAY:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                int event = type == AMF3_BYTEARRAY ? AMFE_POST_BYTEARRAY : type == AMF3_XML ? AMFE_POST_XML : AMFE_POST_XMLDOCUMENT;
                if (amf_read_string(rval, p, max, handle >> 1, AMF_STRING_AS_BYTE, var_hash TSRMLS_CC) == FAILURE) {
                    const char *name = type == AMF3_BYTEARRAY ? "bytearray" : "xmlstring";
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot read string for %s", name);
                    return FAILURE;
                }
                zval_add_ref(rval);
                amf_perform_deserialize_callback(event, *rval, rval, 1, var_hash TSRMLS_CC);
                amf_put_in_cache(&(var_hash->objects), *rval);
            } else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    const char *objtype = type == AMF3_BYTEARRAY ? "bytearray" : type == AMF3_XML ? "xml" : "xmldocument";
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup %s for %d", objtype, handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_DATE:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                double d = amf_read_double(p, max, var_hash);
                zval *newval = NULL;
                MAKE_STD_ZVAL(newval);
                ZVAL_DOUBLE(newval, d);
                *rval = newval;
                amf_perform_deserialize_callback(AMFE_POST_DATE, *rval, rval, 1, var_hash TSRMLS_CC);
                amf_put_in_cache(&(var_hash->objects), *rval);
            } else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup date %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_ARRAY:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                int iIndex;
                int maxIndex = handle >> 1;
                HashTable *htOutput = NULL;
                amf_array_init(*rval, maxIndex TSRMLS_CC);
                htOutput = HASH_OF(*rval);
                amf_put_in_cache(&(var_hash->objects), *rval);

                while (1) {
                    zval *zKey, *zValue;
                    char *pEndOfString;
                    char tmp[32];
                    int keyLength;
                    int iIndex;
                    if (amf3_read_string(&zKey, p, max, 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC) == FAILURE) {
                        break;
                    }
                    if (Z_STRLEN_P(zKey) == 0) {
                        break;
                    }
                    MAKE_STD_ZVAL(zValue);
                    zValue->type = 0;
                    if (amf3_deserialize_var(&zValue, p, max, var_hash TSRMLS_CC) == FAILURE) {
                        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot deserialize key %s", Z_STRVAL_P(zKey));
                        zval_ptr_dtor(&zValue);
                        break;
                    }
                    keyLength = Z_STRLEN_P(zKey);
                    if (keyLength < sizeof(tmp)) {
                        /* TODO: use sscanf */
                        memcpy(tmp, Z_STRVAL_P(zKey), keyLength);
                        tmp[keyLength] = 0;
                        iIndex = strtoul(tmp, &pEndOfString, 10);
                    } else {
                        iIndex = 0;
                    }

                    /* TODO test for key as 0 and key as " */
                    if (iIndex != 0 && (pEndOfString == NULL || *pEndOfString == 0)) {
                        zend_hash_index_update(htOutput, iIndex, &zValue, sizeof(zval *), NULL);
                    } else {
                        add_assoc_zval(*rval, Z_STRVAL_P(zKey), zValue);
                    }
                }

                for (iIndex = 0; iIndex < maxIndex; iIndex++) {
                    if (**p == AMF3_UNDEFINED) {
                        *p = *p + 1;
                    } else {
                        zval *zValue;
                        MAKE_STD_ZVAL(zValue);
                        zValue->type = 0;
                        if (amf3_deserialize_var(&zValue, p, max, var_hash TSRMLS_CC) == FAILURE) {
                            zval_ptr_dtor(&zValue);
                            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot deserialize array item %d", iIndex);
                            return FAILURE;
                        }
                        add_index_zval(*rval, iIndex, zValue);
                    }
                }
            } else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup array %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_OBJECT:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                int bInlineclassDef;
                int nClassMemberCount = 0;
                int bTypedObject;
                int iDynamicObject;
                int iExternalizable;
                zval *zClassDef, *zClassName = NULL;
                int iMember;
                int bIsArray = 0;
                int iSuccess = FAILURE;

                bInlineclassDef = (handle & AMF_INLINE_CLASS) != 0;

                if (bInlineclassDef == 0) {
                    HashTable *htClassDef;
                    zval **tmp;
                    int iClassDef = (handle >> AMF_CLASS_SHIFT);
                    if (amf_get_from_cache(&(var_hash->traits), &zClassDef, iClassDef) == FAILURE) {
                        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot find class by number %d", iClassDef);
                        return FAILURE;
                    }
                    htClassDef = HASH_OF(zClassDef);

                    /* extract information from classdef packed into the first element */
                    handle = amf_get_index_long(htClassDef, 0, 0);
                    nClassMemberCount = handle >> AMF_CLASS_MEMBERCOUNT_SHIFT;
                    bTypedObject = (handle & 1) != 0;  /* special */
                    iExternalizable = handle & AMF_CLASS_EXTERNAL;
                    iDynamicObject = handle & AMF_CLASS_DYNAMIC;

                    if (zend_hash_index_find(htClassDef, 1, (void **) &tmp) == SUCCESS) {
                        zClassName = *tmp;
                    } else {
                        zClassName = NULL;
                    }
                } else {
                    iExternalizable = handle & AMF_CLASS_EXTERNAL;
                    iDynamicObject = handle & AMF_CLASS_DYNAMIC;
                    nClassMemberCount = handle >> AMF_CLASS_MEMBERCOUNT_SHIFT;

                    amf3_read_string(&zClassName, p, max, 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
                    bTypedObject = Z_STRLEN_P(zClassName) > 0;

                    /* A classdef is an array containing the reference, classname, then list of property names */
                    MAKE_STD_ZVAL(zClassDef);
                    amf_array_init(zClassDef, nClassMemberCount + 2 TSRMLS_CC);
                    add_next_index_long(zClassDef, (bTypedObject ? 1 : 0) | nClassMemberCount << AMF_CLASS_MEMBERCOUNT_SHIFT | iDynamicObject | iExternalizable);
                    Z_ADDREF_P(zClassName);
                    add_next_index_zval(zClassDef, zClassName);

                    /* loop over classMemberCount */
                    for (iMember = 0; iMember < nClassMemberCount; iMember++) {
                        zval *zMemberName;
                        if (amf3_read_string(&zMemberName, p, max, 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC) == FAILURE) {
                            break;
                        }
                        Z_ADDREF_P(zMemberName);
                        add_next_index_zval(zClassDef, zMemberName);
                    }

                    amf_put_in_cache(&(var_hash->traits), zClassDef);
                }

                /* callback for externalizable or classnames not null */
                if (iExternalizable != 0 || (zClassName != NULL && Z_STRLEN_P(zClassName) != 0))
                {
                    if ((iSuccess = amf_perform_deserialize_callback(iExternalizable != 0 ? AMFE_MAP_EXTERNALIZABLE : AMFE_MAP, zClassName, rval, 0, var_hash TSRMLS_CC)) == SUCCESS) {
                        if (Z_TYPE_PP(rval) == IS_ARRAY) {
                            bIsArray = 1;
                        } else if (Z_TYPE_PP(rval) == IS_OBJECT) {
                            bIsArray = 0;
                        } else {
                            /* TODO: error */
                            iSuccess = FAILURE; // nor an object or an array
                        }
                    }
                }

                /* invoke the callback passing: classname, externalizable */
                /* return: treat as any or as object, place in object or in array */
                if (iExternalizable != 0) {
                    if (iSuccess == FAILURE || Z_TYPE_PP(rval) == IS_NULL) {
                        amf_put_in_cache(&(var_hash->objects), NULL);
                        amf3_deserialize_var(rval, p, max, var_hash TSRMLS_CC);
                    } else {
                        amf_put_in_cache(&(var_hash->objects), *rval);
                    }
                } else {
                    /* default behaviour */
                    if (iSuccess == FAILURE || Z_TYPE_PP(rval) == IS_NULL) {
                        if ((var_hash->flags & AMF_OBJECT_AS_ASSOC) != 0) {
                            amf_array_init(*rval, nClassMemberCount TSRMLS_CC);
                            bIsArray = 1;
                        } else {
                            if (bTypedObject != 0) {
                                zend_class_entry **classEntry;

                #if PHP_MAJOR_VERSION >= 5
                                if (zend_lookup_class(Z_STRVAL_P(zClassName), Z_STRLEN_P(zClassName), &classEntry TSRMLS_CC) != SUCCESS) {
                #else
                                if (zend_hash_find(EG(class_table), Z_STRVAL_P(zClassName), Z_STRLEN_P(zClassName), (void **) &classEntry) != SUCCESS) {
                #endif
                                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot find class entry %s", Z_STRVAL_P(zClassName));
                                    object_init(*rval);
                                } else {
                                    object_init_ex(*rval, *classEntry);
                                }
                            } else {
                                object_init(*rval);
                            }
                        }
                    }

                    amf_put_in_cache(&(var_hash->objects), *rval);

                    for (iMember = 0; iMember < nClassMemberCount; iMember++) {
                        zval **pzName, *zValue;
                        if (zend_hash_index_find(HASH_OF(zClassDef), iMember + 2, (void **) &pzName) == FAILURE) {
                            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot find index for class member %d over %d", iMember, nClassMemberCount);
                            return FAILURE;
                        }
                        MAKE_STD_ZVAL(zValue);
                        zValue->type = 0;
                        if (amf3_deserialize_var(&zValue, p, max, var_hash TSRMLS_CC) == FAILURE)  {
                            zval_ptr_dtor(&zValue);
                            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot read value for class member");
                            return FAILURE;
                        }
                        if (bIsArray == 1) {
                            add_assoc_zval(*rval, Z_STRVAL_PP(pzName), zValue);
                        } else {
                            add_property_zval(*rval, Z_STRVAL_PP(pzName), zValue);  /* pass zValue */
                        }
                    }

                    if (iDynamicObject != 0) {
                        while (1) {
                            zval *zKey;
                            zval *zValue;
                            if (amf3_read_string(&zKey, p, max, 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC) == FAILURE) {
                                php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot understand key name %X", "");
                                break;
                            }
                            if (Z_STRLEN_P(zKey) == 0) {
                                break;
                            }
                            MAKE_STD_ZVAL(zValue);
                            zValue->type = 0;
                            if (amf3_deserialize_var(&zValue, p, max, var_hash TSRMLS_CC) == FAILURE) {
                                zval_ptr_dtor(&zValue);
                                php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot deserialize member %s", Z_STRVAL_P(zKey));
                                return FAILURE;
                            }
                            if (bIsArray == 1) {
                                add_assoc_zval(*rval, Z_STRVAL_P(zKey), zValue);
                            } else {
                                add_property_zval(*rval, Z_STRVAL_P(zKey), zValue);
                            }
                        }
                    }

                    if (bIsArray == 1) {
                        if (bTypedObject != 0) {
                            Z_ADDREF_P(zClassName);
                            add_assoc_zval(*rval, "_explicitType", zClassName);
                        }
                    } else if ((var_hash->flags & AMF_POST_DECODE) != 0) {
                        amf_perform_deserialize_callback(AMFE_POST_OBJECT, *rval, rval, 0, var_hash TSRMLS_CC);
                    }
                }
            } else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup object %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_VECTOR_INT:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                const unsigned char *cp = *p;
                zval *zvVectorData, *zvVectorFixed = NULL;
                int iIndex;
                int vectorLen = handle >> 1;
                HashTable *htVector = NULL;

                MAKE_STD_ZVAL(zvVectorFixed);
                ZVAL_BOOL(zvVectorFixed, *cp++);
                *p = cp;

                amf_perform_deserialize_callback(AMFE_VECTOR_INT, *rval, rval, 1, var_hash TSRMLS_CC);
                amf_put_in_cache(&(var_hash->objects), *rval);

                htVector = HASH_OF(*rval);
                MAKE_STD_ZVAL(zvVectorData);
                zvVectorData->type = 0;
                amf_array_init(zvVectorData, vectorLen TSRMLS_CC);

                for (iIndex = 0; iIndex < vectorLen; iIndex++) {
                    int item = amf3_read_vector_int(p, max, var_hash);
                    zval *newval = NULL;
                    MAKE_STD_ZVAL(newval);
                    ZVAL_LONG(newval, item);

                    add_index_zval(zvVectorData, iIndex, newval);
                }
                zend_hash_update(htVector, "data", strlen("data") + 1, &zvVectorData, sizeof(zval *), NULL);
            } else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup vector %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_VECTOR_UINT:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                const unsigned char *cp = *p;
                zval *zvVectorData, *zvVectorFixed = NULL;
                int iIndex;
                int vectorLen = handle >> 1;
                HashTable *htVector = NULL;

                MAKE_STD_ZVAL(zvVectorFixed);
                ZVAL_BOOL(zvVectorFixed, *cp++);
                *p = cp;

                amf_perform_deserialize_callback(AMFE_VECTOR_UINT, *rval, rval, 1, var_hash TSRMLS_CC);
                amf_put_in_cache(&(var_hash->objects), *rval);

                htVector = HASH_OF(*rval);
                MAKE_STD_ZVAL(zvVectorData);
                zvVectorData->type = 0;
                amf_array_init(zvVectorData, vectorLen TSRMLS_CC);

                for (iIndex = 0; iIndex < vectorLen; iIndex++) {
                    unsigned int item = amf3_read_vector_uint(p, max, var_hash);
                    zval *newval = NULL;
                    MAKE_STD_ZVAL(newval);
                    if (item > INT_MAX) {
                        ZVAL_DOUBLE(newval, item);
                    } else {
                        ZVAL_LONG(newval, item);
                    }

                    add_index_zval(zvVectorData, iIndex, newval);
                }
                zend_hash_update(htVector, "data", strlen("data") + 1, &zvVectorData, sizeof(zval *), NULL);
            } else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup vector %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_VECTOR_DOUBLE:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                const unsigned char *cp = *p;
                zval *zvVectorData, *zvVectorFixed = NULL;
                int iIndex;
                int vectorLen = handle >> 1;
                HashTable *htVector = NULL;

                MAKE_STD_ZVAL(zvVectorFixed);
                ZVAL_BOOL(zvVectorFixed, *cp++);
                *p = cp;

                amf_perform_deserialize_callback(AMFE_VECTOR_DOUBLE, *rval, rval, 1, var_hash TSRMLS_CC);
                amf_put_in_cache(&(var_hash->objects), *rval);

                htVector = HASH_OF(*rval);
                MAKE_STD_ZVAL(zvVectorData);
                zvVectorData->type = 0;
                amf_array_init(zvVectorData, vectorLen TSRMLS_CC);

                for (iIndex = 0; iIndex < vectorLen; iIndex++) {
                    double item = amf3_read_vector_double(p, max, var_hash);
                    zval *newval = NULL;
                    MAKE_STD_ZVAL(newval);
                    ZVAL_DOUBLE(newval, item);

                    add_index_zval(zvVectorData, iIndex, newval);
                }
                zend_hash_update(htVector, "data", strlen("data") + 1, &zvVectorData, sizeof(zval *), NULL);
            } else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup vector %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_VECTOR_OBJECT:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                const unsigned char *cp = *p;
                zval *zvVectorData, *zvVectorFixed, *zvVectorClassName = NULL;
                zval ;
                int iIndex;
                int vectorLen = handle >> 1;
                HashTable *htVector = NULL;

                MAKE_STD_ZVAL(zvVectorFixed);
                ZVAL_BOOL(zvVectorFixed, *cp++);
                *p = cp;

                if (amf3_read_string(&zvVectorClassName, p, max, 1, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot understand vector className %X", "");
                    break;
                }

                amf_perform_deserialize_callback(AMFE_VECTOR_OBJECT, *rval, rval, 1, var_hash TSRMLS_CC);
                amf_put_in_cache(&(var_hash->objects), *rval);

                htVector = HASH_OF(*rval);
                MAKE_STD_ZVAL(zvVectorData);
                zvVectorData->type = 0;
                amf_array_init(zvVectorData, vectorLen TSRMLS_CC);

                for (iIndex = 0; iIndex < vectorLen; iIndex++) {
                    zval *newval = NULL;
                    MAKE_STD_ZVAL(newval);
                    newval->type = 0;
                    if (amf3_deserialize_var(&newval, p, max, var_hash TSRMLS_CC) == FAILURE) {
                        zval_ptr_dtor(&newval);
                        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot deserialize array item %d", iIndex);
                        return FAILURE;
                    }
                    add_index_zval(zvVectorData, iIndex, newval);
                }
                zend_hash_update(htVector, "data", strlen("data") + 1, &zvVectorData, sizeof(zval *), NULL);
            } else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf cannot lookup vector %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        default:
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf unknown AMF3 type %d", type);
            return FAILURE;
    }
    return SUCCESS;
}

/** generic deserialization in AMF0 format */
static int amf0_deserialize_var(zval **rval, const unsigned char **p, const unsigned char *max, amf_deserialize_data_t *var_hash TSRMLS_DC)
{
    const unsigned char *cursor = *p;
    int type = *cursor++;
    *p = cursor;

    switch (type) {
        case AMF0_NUMBER:
            ZVAL_DOUBLE(*rval, amf_read_double(p, max, var_hash));
            break;
        case AMF0_ENDOBJECT:
            return FAILURE;
        case AMF0_BOOLEAN:
            ZVAL_BOOL(*rval, *cursor++);
            *p = cursor;
            break;
        case AMF0_DATE: {
                double d = amf_read_double(p, max, var_hash);
                int tz = amf_read_short(p, max, var_hash);
                zval *newval = NULL;
                MAKE_STD_ZVAL(newval);
                ZVAL_DOUBLE(newval, d);
                *rval = newval;
                amf_perform_deserialize_callback(AMFE_POST_DATE, *rval, rval, 0, var_hash TSRMLS_CC);
            }
            break;
        case AMF0_STRING:
            return amf0_read_string(rval, p, max, 2, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
        case AMF0_NULL:
        case AMF0_UNDEFINED:
            ZVAL_NULL(*rval);
            break;
        case AMF0_REFERENCE: {
                int objectIndex = amf_read_short(p, max, var_hash);
                if (amf_get_from_cache(&(var_hash->objects0), rval, objectIndex) == FAILURE) {
                    php_error_docref(NULL TSRMLS_CC, E_NOTICE, "cannot find object reference %d", objectIndex);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
			break;
        case AMF0_OBJECT:
            /* AMF0 read object: key=value up to AMF0_ENDOBJECT that is used for termination */
            return amf0_read_objectdata(rval, p, max, NULL, 0, 0, var_hash TSRMLS_CC);
        case AMF0_MIXEDARRAY: {
                /* AMF0 Mixed: I(maxindex) then name=value up to AMF0_ENDOBJECT */
                int maxIndex = amf_read_int(p, max, var_hash);
                return amf0_read_objectdata(rval, p, max, NULL, 1, maxIndex, var_hash TSRMLS_CC);
            }
            break;
        case AMF0_ARRAY: {
                int iIndex;
                int length = amf_read_int(p, max, var_hash);
                HashTable *ht = NULL;
                amf_array_init(*rval, length TSRMLS_CC);
                ht = HASH_OF(*rval);
                amf_put_in_cache(&(var_hash->objects0), *rval);

                for (iIndex = 0; iIndex < length; iIndex++) {
                    if (**p == AMF0_UNDEFINED)  {
                        *p = *p + 1;
                    } else {
                        zval *zValue;
                        MAKE_STD_ZVAL(zValue);
                        zValue->type = 0;
                        if (amf0_deserialize_var(&zValue, p, max, var_hash TSRMLS_CC) == FAILURE) {
                            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf bad deserialized value for array index %d", iIndex);
                            zval_ptr_dtor(&zValue);
                            return FAILURE;
                        }
                        add_index_zval(*rval, iIndex, zValue);
                    }
                }
            }
            break;
        case AMF0_TYPEDOBJECT: {
                zval *zClassName;
                MAKE_STD_ZVAL(zClassName);
                if (amf0_read_string(&zClassName, p, max, 2, AMF_STRING_AS_TEXT, var_hash  TSRMLS_CC) == FAILURE) {
                    return FAILURE;
                }
                if (amf0_read_objectdata(rval, p, max, zClassName, 0, 0, var_hash TSRMLS_CC) == FAILURE) {
                    return FAILURE;
                }
                zval_ptr_dtor(&zClassName);
            }
            break;
        case AMF0_LONGSTRING:
            return amf0_read_string(rval, p, max, 4, AMF_STRING_AS_TEXT, var_hash TSRMLS_CC);
        case AMF0_XML:
            if (amf0_read_string(rval, p, max, 4, AMF_STRING_AS_BYTE, var_hash TSRMLS_CC) == FAILURE) {
                return FAILURE;
            }
            amf_perform_deserialize_callback(AMFE_POST_XML, *rval, rval, 0, var_hash TSRMLS_CC);
            break;
        case AMF0_AMF3:
            var_hash->flags |= AMF_AMF3;
            return amf3_deserialize_var(rval, p, max, var_hash TSRMLS_CC);
        case AMF0_MOVIECLIP:
        case AMF0_UNSUPPORTED:
        case AMF0_RECORDSET:
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf unsupported AMF type %d", type);
            return FAILURE;
        default:
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf unknown AMF type %d", type);
            return FAILURE;
    }
    return SUCCESS;
}

/**
 * PHP function that decodes a string
 * \param string to be decoded
 * \param flags as bitmask of AMF_BIGEND, AMF_OBJECT_AS_ASSOC. It is optional
 * \param beginning index. It is modified to the resulting offset
 * \param context. The context for multiple encodings
 */
PHP_FUNCTION(amf_decode)
{
    zval **zzInput = NULL, **zzFlags = NULL, **zzOffset = NULL, **zzCallback = NULL;
    int offset = 0;
    int flags = 0;
    amf_deserialize_data_t var_hash;

    switch (ZEND_NUM_ARGS()) {
        case 0:
            WRONG_PARAM_COUNT;
            return;
        case 1:
            if (zend_parse_parameters(1 TSRMLS_CC, "Z", &zzInput) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            break;
        case 2:
            if (zend_parse_parameters(2 TSRMLS_CC, "ZZ", &zzInput, &zzFlags) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            convert_to_long_ex(zzFlags);
            flags = Z_LVAL_PP(zzFlags);
            break;
        default:
            if (zend_parse_parameters((ZEND_NUM_ARGS() > 3 ? 4 : 3) TSRMLS_CC, "ZZZ|Z", &zzInput, &zzFlags, &zzOffset, &zzCallback) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            convert_to_long_ex(zzFlags);
            convert_to_long_ex(zzOffset);
            flags = Z_LVAL_PP(zzFlags);
            offset = Z_LVAL_PP(zzOffset);
            break;
    }
    var_hash.flags = flags;

    if (Z_TYPE_PP(zzInput) == IS_STRING) {
        const unsigned char *p = (unsigned char *) Z_STRVAL_PP(zzInput) + offset;
        const unsigned char *p0 = p;
        zval *tmp = return_value;

        if (Z_STRLEN_PP(zzInput) == 0)  {
            RETURN_FALSE;
        }
        amf_DESERIALIZE_CTOR(var_hash, zzCallback);
        if (amf0_deserialize_var(&tmp, &p, p + Z_STRLEN_PP(zzInput) - offset, &var_hash TSRMLS_CC) == FAILURE) {
            amf_DESERIALIZE_DTOR(var_hash);
            php_error_docref(NULL TSRMLS_CC, E_NOTICE, "Error at offset %ld of %d bytes", (long)((char *) p - Z_STRVAL_PP(zzInput)), Z_STRLEN_PP(zzInput));
            RETURN_FALSE;
        }
        if (zzFlags != NULL) {
            ZVAL_LONG(*zzFlags, var_hash.flags);
        }
        if (zzOffset != NULL) {
            ZVAL_LONG(*zzOffset, offset + p - p0);
        }
        amf_DESERIALIZE_DTOR(var_hash);

        *return_value = *tmp;
    } else {
        php_error_docref(NULL TSRMLS_CC, E_NOTICE, "amf_decode requires a string argument");
        RETURN_FALSE;
    }
}

/* StringBuilder Resource {{{1*/


PHP_FUNCTION(amf_sb_new)
{
#ifdef amf_USE_STRING_BUILDER
    amf_serialize_output buf = (amf_serialize_output)emalloc(sizeof(amf_serialize_output_t));
    amf_serialize_output_ctor(buf);
    ZEND_REGISTER_RESOURCE(return_value, buf, amf_serialize_output_resource_reg);
#else
    RETURN_FALSE;
#endif
}

/* TODO */
PHP_FUNCTION(amf_sb_append_move)
{
#ifdef amf_USE_STRING_BUILDER
    int i;
    int argc = ZEND_NUM_ARGS();
    zval **params[10];
    amf_serialize_output sb = NULL;

    if (argc > sizeof(params) / sizeof(params[0])) {
        argc = sizeof(params) / sizeof(params[0]);
    }

    if (zend_parse_parameters(argc TSRMLS_CC, "Z|Z", &params[0], &params[1], &params[2], &params[3], &params[4],
        &params[5], &params[6], &params[7], &params[8], &params[9]) == FAILURE
    ) {
        return;
    }
    if (Z_TYPE_PP(params[0]) != IS_RESOURCE) {
        return;
    }

    ZEND_FETCH_RESOURCE(sb, amf_serialize_output, params[0], -1, PHP_AMF_STRING_BUILDER_RES_NAME, amf_serialize_output_resource_reg);

    for (i = 1; i < argc; i++) {
        _amf_sb_append(sb, *params[i], 0 TSRMLS_CC);
    }
#endif
}

/** equivalent to join_test */
PHP_FUNCTION(amf_sb_append)
{
#ifdef amf_USE_STRING_BUILDER
    int i;
    int argc = ZEND_NUM_ARGS();
    zval **params[10];
    amf_serialize_output sb = NULL;

    if (argc > sizeof(params) / sizeof(params[0])) {
        argc = sizeof(params) / sizeof(params[0]);
    }

    if (zend_parse_parameters(argc TSRMLS_CC, "Z|Z", &params[0], &params[1], &params[2], &params[3], &params[4],
        &params[5], &params[6], &params[7], &params[8], &params[9]) == FAILURE
    ) {
        return;
    }
    if (Z_TYPE_PP(params[0]) != IS_RESOURCE) {
        return;
    }

    ZEND_FETCH_RESOURCE(sb, amf_serialize_output, params[0], -1, PHP_AMF_STRING_BUILDER_RES_NAME, amf_serialize_output_resource_reg);

    for (i = 1; i < argc; i++) {
        _amf_sb_append(sb, *params[i], 1 TSRMLS_CC);
    }
#endif
}

PHP_FUNCTION(amf_sb_length)
{
#ifdef amf_USE_STRING_BUILDER
    zval *zsb;
    amf_serialize_output sb = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zsb) == FAILURE) {
        RETURN_FALSE;
    }
    ZEND_FETCH_RESOURCE(sb, amf_serialize_output, &zsb, -1, PHP_AMF_STRING_BUILDER_RES_NAME, amf_serialize_output_resource_reg);
    RETURN_LONG(sb->length)
#endif
}

PHP_FUNCTION(amf_sb_memusage)
{
#ifdef amf_USE_STRING_BUILDER
    zval *zsb;
    amf_serialize_output sb = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zsb) == FAILURE) {
        RETURN_LONG(0);
    }
    ZEND_FETCH_RESOURCE(sb, amf_serialize_output, &zsb, -1, PHP_AMF_STRING_BUILDER_RES_NAME, amf_serialize_output_resource_reg);
    RETURN_LONG(sb->total_allocated);
#endif
}

PHP_FUNCTION(amf_sb_write)
{
#ifdef amf_USE_STRING_BUILDER
    php_stream *stream = NULL;
    zval **params[2] = { NULL, NULL };
    amf_serialize_output sb = NULL;
    if (zend_parse_parameters((ZEND_NUM_ARGS() > 1 ? 2 : 1) TSRMLS_CC, "Z|Z!", &params[0], &params[1]) == FAILURE) {
        return;
    }
    ZEND_FETCH_RESOURCE(sb, amf_serialize_output, params[0], -1, PHP_AMF_STRING_BUILDER_RES_NAME, amf_serialize_output_resource_reg);
    if (params[1] == NULL)  {
        zval r;
        zval *r2 = &r;

        /* PHP4 allows for stream = NULL and uses zend_write */
        if (zend_get_constant("STDOUT", sizeof("STDOUT"), &r TSRMLS_CC)) {
            if (Z_TYPE_P(r2) == IS_RESOURCE) {
                php_stream_from_zval(stream, &r2);
            } else {
                RETURN_FALSE;
            }
        }
    } else {
        if (Z_TYPE_PP(params[1]) == IS_RESOURCE) {
            php_stream_from_zval(stream, params[1]);
        } else {
            RETURN_FALSE;
        }
    }
    amf_serialize_output_write(sb, stream TSRMLS_CC);
    RETURN_TRUE;
#endif
}

PHP_FUNCTION(amf_sb_as_string)
{
#ifdef amf_USE_STRING_BUILDER
    zval *zsb;
    amf_serialize_output sb = NULL;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zsb) == FAILURE) {
        RETURN_FALSE;
    }
    ZEND_FETCH_RESOURCE(sb, amf_serialize_output, &zsb, -1, PHP_AMF_STRING_BUILDER_RES_NAME, amf_serialize_output_resource_reg);
    amf_serialize_output_get(sb, return_value);
#endif
}

static void php_amf_sb_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC)
{
#ifdef amf_USE_STRING_BUILDER
   amf_serialize_output sb = (amf_serialize_output)rsrc->ptr;
   if (sb) {
       amf_serialize_output_dtor(sb);
       efree(sb);
   }
#endif
}

/* Charset {{{1*/

static int amf_string_is_ascii(const unsigned char *cp, int length)
{
    while (length-- > 0) {
        if (*cp++ >= 0x7F) { // isascii
            return 0;
        }
    }
    return 1;
}

/** invoke the callback as (AMFE_TRANSLATE_CHARSET, inputstring) => resulting string */
zval *amf_translate_charset_zstring(zval *inz, enum AMFStringTranslate direction, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    zval *r = 0;
    int rr;

    /* maybe direction == AMF_FROM_UTF8 */
    if ((var_hash->flags & AMF_TRANSLATE_CHARSET_FAST) == AMF_TRANSLATE_CHARSET_FAST && amf_string_is_ascii((unsigned char *) Z_STRVAL_P(inz), Z_STRLEN_P(inz)) == 1) {
        return NULL;
    }
    rr = direction == AMF_TO_UTF8
        ? amf_perform_serialize_callback_event(AMFE_TRANSLATE_CHARSET, inz, &r, 0, var_hash TSRMLS_CC)
        : amf_perform_deserialize_callback(AMFE_TRANSLATE_CHARSET, inz, &r, 0, var_hash TSRMLS_CC);

    if (rr == SUCCESS && r != 0) {
        if (Z_TYPE_P(r) == IS_STRING) {
            return r;
        } else {
            zval_ptr_dtor(&r);
            return NULL;
        }
    } else {
        return NULL;
    }
}

/**
 * The translation is performed from a source C string. In this case we create a ZSTRING and try to translate
 * it. In case of failure we return the generated ZSTRNG
 * If we add direct charset handling we can perform the operation directly here without allocating the ZSTRING
 */
zval *amf_translate_charset_string(const unsigned char *cp, int length, enum AMFStringTranslate direction, amf_serialize_data_t *var_hash TSRMLS_DC)
{
    zval *tmp, *r = NULL;
    int rr;

    /* maybe direction == AMF_FROM_UTF8 */
    if ((var_hash->flags & AMF_TRANSLATE_CHARSET_FAST) == AMF_TRANSLATE_CHARSET_FAST && amf_string_is_ascii(cp, length) == 1) {
        return NULL;
    }

    MAKE_STD_ZVAL(tmp);
    ZVAL_STRINGL(tmp, (char *) cp, length, 1);
    rr = direction == AMF_TO_UTF8
        ? amf_perform_serialize_callback_event(AMFE_TRANSLATE_CHARSET, tmp, &r, 0, var_hash TSRMLS_CC)
        : amf_perform_deserialize_callback(AMFE_TRANSLATE_CHARSET, tmp, &r, 0, var_hash TSRMLS_CC);

    if (rr == SUCCESS && r != 0) {
        if (Z_TYPE_P(r) == IS_STRING) {
            zval_ptr_dtor(&tmp);
            return r;
        } else {
            zval_ptr_dtor(&r);
        }
    }
    return tmp;
}

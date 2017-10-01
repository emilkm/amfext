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

#undef _DEBUG
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "ext/pcre/php_pcre.h"
#include "ext/date/php_date.h"
#include "zend_smart_str.h"
#include "php_amf.h"

ZEND_BEGIN_ARG_INFO_EX(arginfo_amf_decode, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(1, decode_flags)
    ZEND_ARG_INFO(1, position)
    ZEND_ARG_INFO(0, decode_callback)
ZEND_END_ARG_INFO()

static zend_function_entry amf_functions[] = 
{
    PHP_FE(amf_encode, NULL)
    PHP_FE(amf_decode, arginfo_amf_decode)
    {NULL, NULL, NULL}
};

static PHP_MINFO_FUNCTION(amf) 
{
    php_info_print_table_start();
    php_info_print_table_header(2, "AMF Native Support", "enabled");
    php_info_print_table_row(2, "Compiled Version", PHP_AMF_VERSION);
    php_info_print_table_end();
}

/**  resource StringBuilder */
#define PHP_AMF_STRING_BUILDER_RES_NAME "String Builder"
static void php_amf_sb_dtor(zend_resource *rsrc);
int amf_serialize_output_resource_reg;

PHP_MINIT_FUNCTION(amf) 
{
    amf_serialize_output_resource_reg = zend_register_list_destructors_ex(php_amf_sb_dtor, NULL, PHP_AMF_STRING_BUILDER_RES_NAME, module_number);
    return SUCCESS;
}

zend_module_entry amf_module_entry = 
{
    STANDARD_MODULE_HEADER,
    PHP_AMF_WORLD_EXTNAME,
    amf_functions,
    PHP_MINIT(amf),
    NULL,
    NULL,
    NULL,
    PHP_MINFO(amf),
    PHP_AMF_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_AMF
ZEND_GET_MODULE(amf)
#endif

/* AMF enumeration {{{ */

/** AMF0 types */
enum AMF0Codes { AMF0_NUMBER, AMF0_BOOLEAN, AMF0_STRING, AMF0_OBJECT, AMF0_MOVIECLIP, AMF0_NULL, AMF0_UNDEFINED, AMF0_REFERENCE, AMF0_MIXEDARRAY, AMF0_ENDOBJECT, AMF0_ARRAY, AMF0_DATE, AMF0_LONGSTRING, AMF0_UNSUPPORTED, AMF0_RECORDSET, AMF0_XML, AMF0_TYPEDOBJECT, AMF0_AMF3 };

/** AMF3 types */
enum AMF3Codes { AMF3_UNDEFINED, AMF3_NULL, AMF3_FALSE, AMF3_TRUE, AMF3_INTEGER, AMF3_NUMBER, AMF3_STRING, AMF3_XMLDOCUMENT, AMF3_DATE, AMF3_ARRAY, AMF3_OBJECT, AMF3_XML, AMF3_BYTEARRAY, AMF3_VECTOR_INT, AMF3_VECTOR_UINT, AMF3_VECTOR_DOUBLE, AMF3_VECTOR_OBJECT };

/** return values for callbacks */
enum AMFCallbackResult { AMFC_RAW, AMFC_XML, AMFC_OBJECT, AMFC_TYPEDOBJECT, AMFC_ANY, AMFC_ARRAY, AMFC_NONE, AMFC_BYTEARRAY, AMFC_EXTERNAL, AMFC_DATE, AMFC_XMLDOCUMENT, AMFC_VECTOR_OBJECT };

/** flags passed to amf_encode and amf_decode */
enum AMFFlags { AMF_AMF3 = 1, AMF_BIGENDIAN = 2, AMF_OBJECT_AS_ASSOC = 4, AMF_POST_DECODE = 8, AMF_AS_STRING_BUILDER = 16, AMF_TRANSLATE_CHARSET = 32, AMF_TRANSLATE_CHARSET_FAST = 32 | 64, AMF3_NSND_ARRAY_AS_OBJECT = 128 };

/** events invoked by the callback */
enum AMFEvent { AMFE_MAP = 1, AMFE_POST_OBJECT, AMFE_POST_XML, AMFE_MAP_EXTERNALIZABLE, AMFE_POST_BYTEARRAY, AMFE_TRANSLATE_CHARSET, AMFE_POST_DATE, AMFE_POST_XMLDOCUMENT, AMFE_VECTOR_INT, AMFE_VECTOR_UINT, AMFE_VECTOR_DOUBLE, AMFE_VECTOR_OBJECT };

/** flags for the recordset _amf_recordset_ */
enum AMFRecordSet { AMFR_NONE = 0, AMFR_ARRAY = 1, AMFR_ARRAY_COLLECTION = 2 };

/** flags for AMF3_OBJECT */
enum AMF3ObjectDecl { AMF_INLINE_ENTITY = 1, AMF_INLINE_CLASS = 2, AMF_CLASS_EXTERNAL = 4, AMF_CLASS_DYNAMIC = 8, AMF_CLASS_MEMBERCOUNT_SHIFT = 4, AMF_CLASS_SHIFT = 2 };

/** object cache actions **/
enum ObjectCacheAction { OCA_LOOKUP_AND_ADD = 0, OCA_ADD_ONLY = 1, OCA_LOOKUP_ONLY = 2 };

/* }}} */

/* String Builder {{{ ********************************************************/
/*****************************************************************************/
/*****************************************************************************/

/**
 * The result of the encoder is a string that grows depending on the input. When using memory streams or
 * smart_str the allocation of memory is not efficient because these methods allow the generic access of the string.
 * Instead in our case a StringBuilder approach is better suited. We have implemented such StringBuilder approach
 * in which the resulting string is made of string parts. Each string part has a default length of AMFPARTSIZE
 * that eventually can be bigger when long strings are appended. At the end of the processing such sequence of parts
 * is joined into the resulting strings.
 *
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
    size_t size;           // bit 0 = zval, rest is length. Length of 0 is terminaton
    union {
#ifndef amf_NO_ZVAL_STRING_BUILDER
        zval *zv;            // string zval
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
    char *data;                      // pointer to the data of the current block
    size_t length;                   // total length of the string
    size_t default_size;
    size_t left_in_part;             // items left in part
    amf_string_chunk *last_chunk;
    amf_string_part *last;           // last and current part. The next points to the beginning. Simple list
    int chunks;
    int parts;                       // number of parts, useful for debugging
    size_t total_allocated;          // total memory allocated
} amf_serialize_output_t;
typedef amf_serialize_output_t *amf_serialize_output;

#define AMFPARMAXSIZE 32768*4
#define AMFPARTSIZE 64

#define amf_PARTFLAG_ALLOCATED 1
#define amf_PARTFLAG_ZVAL 2

#ifdef amf_GUARD_ALLOCATION
static void *guard_emalloc(int k)
{
    void *r = emalloc(k + 10);
    memset(r, 0x7E, k);
    memset((char *)r + k, 0x7F, 10);
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

static char *backslash_to_dot(char *src, char *dst, size_t len)
{
    unsigned char *s = NULL, *e = NULL;

    if (!src) {
        return (NULL);
    }

    guard_memcpy(dst, src, len);
    s = (unsigned char *)dst;
    e = (unsigned char *)dst + len;

    while (s < e) {
        if (*s == '\\') {
            *s = '.';
        }
        s++;
    }

    return (dst);
}

static inline void amf_write_string(amf_serialize_output buf, const char *cp, size_t len);
static inline void amf_write_string_zval(amf_serialize_output buf, zval *val);

/** allocate a block containing the part header and the data */
static amf_string_part *amf_serialize_output_part_ctor(size_t size)
{
    amf_string_part *r = (amf_string_part *)guard_emalloc(size + sizeof(amf_string_part) + sizeof(amf_string_chunk) - sizeof(char));
    r->next = r;
    r->data->size = 0;
    return r;
}

/** closes the current chunk and move the pointer to the next chunk */
static void amf_serialize_output_close_chunk(amf_serialize_output buf)
{
    /* close the last chunk if not a zchun */
    if (buf->last_chunk->size == 0) {
        buf->last_chunk->size = (buf->data - &buf->last_chunk->data[0]) << 1;
        if (buf->last_chunk->size == 0) {
            return;
        }
        /* get another chunk at the end */
        buf->last_chunk = (amf_string_chunk *)buf->data;
        buf->left_in_part -= sizeof(amf_string_chunk);
        buf->chunks++;
    }
    else {
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
 * Appends a block of size specified to the StringBuilder. If the current part is a zpart
 * then take some memory from that. The size is not mandatory!
 */
static void amf_serialize_output_part_append(amf_serialize_output buf, size_t size)
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
static void amf_serialize_output_write(amf_serialize_output buf, php_stream *stream)
{
    amf_string_part *cur, *head;
    if (buf->length == 0) {
        return;
    }
    head = cur = buf->last->next;
    amf_serialize_output_close_part(buf);

    /* printf("flattening length:%d parts:%d chunks:%d memory:%d\n", buf->length, buf->parts, buf->chunks, buf->total_allocated) */
    do {
        amf_string_chunk *chunk = (amf_string_chunk *)cur->data;
        while (chunk->size != 0) {
#ifndef amf_NO_ZVAL_STRING_BUILDER
            if ((chunk->size & 1) != 0)
            {
                if (stream == NULL) {
                    zend_write(Z_STRVAL_P(chunk->zv), Z_STRLEN_P(chunk->zv));
                }
                else {
                    php_stream_write(stream, Z_STRVAL_P(chunk->zv), Z_STRLEN_P(chunk->zv));
                }
                chunk++;
            }
            else
#endif
            {
                size_t len = chunk->size >> 1;
                if (stream == NULL) {
                    zend_write(chunk->data, len);
                }
                else {
                    php_stream_write(stream, chunk->data, len);
                }
                chunk = (amf_string_chunk *)(((char *)chunk->data) + len);
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
            amf_string_chunk *chunk = (amf_string_chunk *)cur->data;
            while (chunk->size != 0) {
#ifndef amf_NO_ZVAL_STRING_BUILDER
                if ((chunk->size & 1) != 0)
                {
                    amf_write_string_zval(buf, chunk->zv);
                    chunk++;
                }
                else
#endif
                {
                    size_t len = chunk->size >> 1;
                    amf_write_string(buf, chunk->data, len);
                    chunk = (amf_string_chunk *)(((char *)chunk->data) + len);
                }
            }
            cur = cur->next;
        } while (cur != head);
    }
    else {
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
        dlast->next = head;      /* after the last of dst, there is head of sr */
        last->next = dhead;      /* after the last of src, there is head of ds */
        buf->last = last;
        buf->last_chunk = inbuf->last_chunk;
        buf->left_in_part = inbuf->left_in_part;

        amf_serialize_output_ctor(inbuf); /* cleanup */
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
    cp = bcp = (char *)guard_emalloc(buf->length);
    head = cur = buf->last->next;

    amf_serialize_output_close_part(buf);

    /* printf("flattening length:%d parts:%d chunks:%d memory:%d\n", buf->length, buf->parts, buf->chunks, buf->total_allocated); */
    do {
        amf_string_chunk *chunk = (amf_string_chunk *)cur->data;
        while (chunk->size != 0) {
#ifndef amf_NO_ZVAL_STRING_BUILDER
            if ((chunk->size & 1) != 0)
            {
                size_t len = Z_STRLEN_P(chunk->zv);
                guard_memcpy(cp, Z_STRVAL_P(chunk->zv), len);
                cp += len;
                chunk++;
            }
            else
#endif
            {
                size_t len = chunk->size >> 1;
                guard_memcpy(cp, chunk->data, len);
                cp += len;
                chunk = (amf_string_chunk *)(((char *)chunk->data) + len);
            }
        }
        cur = cur->next;
    } while (cur != head);
    ZVAL_STRINGL(result, bcp, buf->length);
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
#define
#define
#endif


static void php_amf_sb_dtor(zend_resource *rsrc)
{
#ifdef amf_USE_STRING_BUILDER
    amf_serialize_output sb = (amf_serialize_output)rsrc->ptr;
    if (sb) {
        amf_serialize_output_dtor(sb);
        efree(sb);
    }
#endif
}


#ifdef amf_USE_STRING_BUILDER
/** Writes a single byte into the output buffer */
static inline void amf_write_byte(amf_serialize_output buf, int val)
{
#ifndef amf_DISABLE_OUTPUT
    if (buf->left_in_part <= 0) {
        amf_serialize_output_part_append(buf, 0);
    }
    *buf->data++ = val;
    buf->left_in_part--;
    buf->length++;
#endif
}
#else
/**  Writes a single byte into the output buffer */
static inline void _amf_write_byte(amf_serialize_output buf, int val)
{
    char c = (char)val;
    php_stream_write(buf, &c, val);
}
/**  Writes a single byte into the output buffer */
#define amf_write_byte(buf, val) _amf_write_byte((buf), (val))
#endif

static inline void amf_write_string(amf_serialize_output buf, const char *cp, size_t len)
{
#ifndef amf_DISABLE_OUTPUT
#ifdef amf_USE_STRING_BUILDER
    while (len > 0) {
        zend_long left;
        if (buf->left_in_part <= 0) {
            amf_serialize_output_part_append(buf, len > AMFPARTSIZE ? len : 0);
        }
        left = buf->left_in_part;
        if (left > len) {
            left = len;
        }
        /* printf("append raw %d of %d in buffer of %d\n", left, length, buf->last->length) */
        guard_memcpy(buf->data, cp, left);
        cp += left;
        buf->data += left;
        buf->left_in_part -= left;
        buf->length += left;
        len -= left;
    }
#else
    php_stream_write(buf, cp, length);
#endif
#endif
}


/** writes a string from a zval. Provides additional optimization */
static inline void amf_write_string_zval(amf_serialize_output buf, zval *val)
{
#ifndef amf_DISABLE_OUTPUT
    size_t len = Z_STRLEN_P(val);
    if (len == 0) {
        return;
    }
#ifdef amf_USE_STRING_BUILDER
#ifndef amf_NO_ZVAL_STRING_BUILDER
    else if (amf_ZVAL_STRING_BUILDER len > amf_ZVAL_STRING_BUILDER_THRESHOLD) {
        if (buf->left_in_part < sizeof(amf_string_chunk)) {
            amf_serialize_output_part_append(buf, 0);
        }

        amf_serialize_output_close_chunk(buf);

        buf->last_chunk->size = 1;  /* zval chunk */
        buf->last_chunk->zv = val;
        Z_ADDREF_P(val);
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
        amf_write_string(buf, Z_STRVAL_P(val), len);
    }
#endif
}

/* }}} */

/* AMF {{{ *******************************************************************/
/*****************************************************************************/
/*****************************************************************************/

#define AMF_U8_MAX 255
#define AMF_U16_MAX 65535
#define AMF_U32_MAX 4294967295
#define AMF3_INT28_MAX 268435455
#define AMF3_INT28_MIN -268435456
#define AMF3_UINT29_MAX 536870911

/** context of (de)serialization */
typedef struct {
    zend_fcall_info fci;
    zend_fcall_info_cache fci_cache;
    HashTable objects0;              /* stack of objects, no reference */
    HashTable objects;               /* stack of objects for AMF3, no reference */
    HashTable strings;               /* stack of strings for AMF3: string key => index */
    HashTable traits;                /* stack of traits for AMF3, allocated */
    HashTable objtypes0;             /* stack of object types for AMF0, related to objects0 */
    HashTable objtypes;              /* stack of object types for AMF0, related to objects */
    int flags;
    uint32_t next_object0_index;
    uint32_t next_object_index;
    uint32_t next_string_index;
    uint32_t next_trait_index;
} amf_context_data_t;


#define amf_SERIALIZE_CTOR(x) amf_context_ctor(&x, 1);
#define amf_DESERIALIZE_CTOR(x) amf_context_ctor(&x, 0);

#define amf_SERIALIZE_DTOR(x) amf_context_dtor(&x);
#define amf_DESERIALIZE_DTOR(x) amf_context_dtor(&x);  

/** receives a pointer to the data and to the callback */
static void amf_context_ctor(amf_context_data_t *var_hash, int is_serialize)
{
    zend_hash_init(&(var_hash->objects0), 10, NULL, NULL, 0);
    zend_hash_init(&(var_hash->objects), 10, NULL, NULL, 0);
    zend_hash_init(&(var_hash->objtypes0), 10, NULL, NULL, 0);
    zend_hash_init(&(var_hash->objtypes), 10, NULL, NULL, 0);

    /* deserializer stores zval of strings for AMF */
    zend_hash_init(&(var_hash->strings), 10, NULL, is_serialize ? NULL : ZVAL_PTR_DTOR, 0);
    var_hash->next_object0_index = 0;
    var_hash->next_object_index = 0;
    var_hash->next_trait_index = 0;
    var_hash->next_string_index = 0;
    zend_hash_init(&(var_hash->traits), 10, NULL, ZVAL_PTR_DTOR, 0);
}

static void amf_context_dtor(amf_context_data_t *var_hash)
{
    zend_hash_destroy(&(var_hash->objects0));
    zend_hash_destroy(&(var_hash->objects));
    zend_hash_destroy(&(var_hash->strings));
    zend_hash_destroy(&(var_hash->traits));
    zend_hash_destroy(&(var_hash->objtypes0));
    zend_hash_destroy(&(var_hash->objtypes));
}

/** return the i-th element from the array */
static inline zend_long amf_get_index_long(HashTable *ht, zend_ulong idx, int def) 
{
    zval *var;
    if ((var = zend_hash_index_find(ht, idx)) != NULL && Z_TYPE_P(var) == IS_LONG) {
        return (int)Z_LVAL_P(var);
    }
    else {
        return def;
    }
}

static inline int amf_get_from_cache(HashTable *ht, zval *rval, zend_ulong idx)
{
    zval *var;
    if ((var = zend_hash_index_find(ht, idx)) == NULL) {
        return FAILURE;
    }
    else {
        *rval = *var;
        return SUCCESS;
    }
}

static inline int amf_put_in_cache(HashTable *ht, zval *var)
{
    zend_hash_next_index_insert(ht, var);
    return SUCCESS;
}

/**
 * places an object in the cache by using a string representation of its address
 * it is not using the direct pointer because the key is not guaranteed to be
 * sized as the pointer
 * \param object_index_ptr is the pointer to the output code
 * \param next_index_ptr is a pointer to a variable containing the nextIndex used by objects
 * \param action if bit 0 is set do not lookup. If bit 1 is set do not add
 * \return FAILURE if exists
 */
static inline int amf_cache_object(HashTable *ht_cache, HashTable *ht_val, uint32_t *object_index_ptr, uint32_t *next_index_ptr, int action)
{
    zend_ulong ht_idx = (zend_ulong)(zend_uintptr_t)ht_val;

    /* if add only bit not set, then lookup */
    if ((action & OCA_ADD_ONLY) == 0) {
        zval *idx;
        if ((idx = zend_hash_index_find(ht_cache, ht_idx)) != NULL) {
            *object_index_ptr = (uint32_t)Z_LVAL_P(idx);

            return FAILURE;
        }
    }
    
    /* if lookup only bit not set, then add */
    if ((action & OCA_LOOKUP_ONLY) == 0) { 
        /* +1 because otherwise hash will think we are trying to store NULL pointer */
        if (next_index_ptr == NULL) {
            *object_index_ptr = zend_hash_num_elements(ht_cache);
        }
        else {
            *object_index_ptr = *next_index_ptr;
            *next_index_ptr = *next_index_ptr + 1;  /* equal to the number of elements */
        }
        zval zidx;
        ZVAL_LONG(&zidx, *object_index_ptr);
        zend_hash_index_add(ht_cache, ht_idx, &zidx);
    }

    return SUCCESS;
}

static int amf_cache_object_typed(amf_context_data_t *var_hash, zval *val, uint32_t *object_index_ptr, int is_amf3, int action, int amfc_type)
{
    HashTable *ht_cache = is_amf3 == 0 ? &(var_hash->objects0) : &(var_hash->objects);
    HashTable *ht_val;
    switch (Z_TYPE_P(val)) {
        case IS_OBJECT:
            ht_val = Z_OBJPROP_P(val);
            break;
        case IS_ARRAY:
            ht_val = HASH_OF(val);
            break;
        case IS_RESOURCE:
            ht_val = (HashTable *)Z_LVAL_P(val);
            break;
        default:
            return SUCCESS;
    }

    if (amf_cache_object(ht_cache, ht_val, object_index_ptr, is_amf3 == 0 ? &(var_hash->next_object0_index) : &(var_hash->next_object_index), action) == SUCCESS) {
        /* if lookup only bit not set, then add */
        if ((action & OCA_LOOKUP_ONLY) == 0) {
            if (is_amf3 == 0) {
                /* AMF0 does not cache type info */
            }
            else {
                zval ztype;
                ZVAL_LONG(&ztype, amfc_type);
                zend_hash_index_update(&(var_hash->objtypes), *object_index_ptr, &ztype);
            }
        }
        return SUCCESS;
    }
    else {
        return FAILURE;
    }
}

/* }}} */

/* Serialize {{{ *************************************************************/
/*****************************************************************************/
/*****************************************************************************/


/** writes a short in AMF0 format. It is formatted in Big Endian 2 byte */
static void amf0_write_short(amf_serialize_output buf, int val)
{
    amf_write_byte(buf, ((val >> 8) & 0xFF));
    amf_write_byte(buf, (val & 0xFF));
}

/** writes an integer in AMF0 format. It is formatted in Big Endian 4 byte */
static void amf0_write_int(amf_serialize_output buf, int val)
{
    char tmp[4] = { (val >> 24) & 0xFF, (val >> 16) & 0xFF, (val >> 8) & 0xFF, (val & 0xFF) };
    amf_write_string(buf, tmp, 4);
}

/** writes a double number in AMF format. It is stored as Big Endian */
static void amf_write_double(amf_serialize_output buf, double val, amf_context_data_t *var_hash)
{
    union aligned {
        double dval;
        char cval[8];
    } d;
    const char *number = d.cval;
    d.dval = val;

    /* AMF number: b(0) double(8 bytes big endian) */
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        char numberr[8] = { number[7], number[6], number[5], number[4], number[3], number[2], number[1], number[0] };
        amf_write_string(buf, numberr, 8);
    }
    else {
        amf_write_string(buf, number, 8);
    }
}

/** writes the end of object terminator of AMF0 */
static void amf0_write_objectend(amf_serialize_output buf)
{
    static char objectEnd[] = { 0,0,9 };
    amf_write_string(buf, objectEnd, 3);
}

/** writes an integer in AMF3 format as a variable bytes */
static void amf3_write_uint29(amf_serialize_output buf, int val)
{
    val &= 0x1fffffff;
    if (val < 0x80) {
        amf_write_byte(buf, val);
    }
    else if (val < 0x4000) {
        amf_write_byte(buf, (val >> 7 & 0x7f) | 0x80);
        amf_write_byte(buf, val & 0x7f);
    }
    else if (val < 0x200000) {
        amf_write_byte(buf, (val >> 14 & 0x7f) | 0x80);
        amf_write_byte(buf, (val >> 7 & 0x7f) | 0x80);
        amf_write_byte(buf, val & 0x7f);
    }
    else {
        char tmp[4] = { (val >> 22 & 0x7f) | 0x80, (val >> 15 & 0x7f) | 0x80, (val >> 8 & 0x7f) | 0x80, val & 0xff };
        amf_write_string(buf, tmp, 4);
    }
}

/** writes a int number in AMF format. It is stored as Big Endian */
static void amf3_write_vector_int(amf_serialize_output buf, int val, amf_context_data_t *var_hash)
{
    union aligned {
        int ival;
        char cval[4];
    } i;
    const char *number = i.cval;
    i.ival = val;

    /* AMF number: b(0) int(4 bytes big endian) */
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        char numberr[8] = { number[3], number[2], number[1], number[0] };
        amf_write_string(buf, numberr, 4);
    }
    else {
        amf_write_string(buf, number, 4);
    }
}

/** writes a uint number in AMF format. It is stored as Big Endian */
static void amf3_write_vector_uint(amf_serialize_output buf, uint val, amf_context_data_t *var_hash)
{
    union aligned {
        uint Ival;
        char cval[4];
    } i;
    const char *number = i.cval;
    i.Ival = val;

    /* AMF number: b(0) int(4 bytes big endian) */
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        char numberr[8] = { number[3], number[2], number[1], number[0] };
        amf_write_string(buf, numberr, 4);
    }
    else {
        amf_write_string(buf, number, 4);
    }
}

/** writes a double number in AMF format. It is stored as Big Endian */
static void amf3_write_vector_double(amf_serialize_output buf, double val, amf_context_data_t *var_hash)
{
    amf_write_double(buf, val, var_hash);
}

/** writes an empty string to AMF */
static void amf0_write_emptystring(amf_serialize_output buf)
{
    amf_write_byte(buf, AMF0_STRING);
    amf0_write_short(buf, 0);
}

/** writes a short string to AMF */
static void amf0_write_shortstring(amf_serialize_output buf, const char *cp, amf_context_data_t *var_hash)
{
    size_t len = strlen(cp);

    if (len > AMF_U16_MAX) {
        php_error_docref(NULL, E_NOTICE, "amf0 cannot write short strings longer than %d", AMF_U16_MAX);
        return;
    }

    amf0_write_short(buf, (int)len);
    amf_write_string(buf, cp, len);
}


/** serializes a zval as zstring in AMF0 using AMF0_STRING or AMF0_LONGSTRING */
static void amf0_write_string_zval(amf_serialize_output buf, zval *zv, amf_context_data_t *var_hash)
{
    size_t len = Z_STRLEN_P(zv);
    
    if (len > AMF_U32_MAX) {
        php_error_docref(NULL, E_NOTICE, "amf0 cannot write strings longer than %d", AMF_U32_MAX);
        return;
    }

    if (len == 0) {
        amf_write_byte(buf, AMF0_STRING);
        amf0_write_short(buf, 0);
    }
    else if (len <= AMF_U16_MAX) {
        amf_write_byte(buf, AMF0_STRING);
        amf0_write_short(buf, (int)len);
    }
    else {
        amf_write_byte(buf, AMF0_LONGSTRING);
        amf0_write_int(buf, (int)len);
    }
    amf_write_string_zval(buf, zv);
}

/** writes an empty AMF3 string */
static inline void amf3_write_emptystring(amf_serialize_output buf)
{
    amf_write_byte(buf, 1);
}

/** writes a string from ZVAL in AMF3 format */
static int amf3_write_string(amf_serialize_output buf, const char *cp, size_t len, amf_context_data_t *var_hash)
{
    if (len == 0) {
        amf_write_byte(buf, 1);  /* inline and empty */
        return -1;
    }
    else if (len > AMF3_UINT29_MAX) {
        php_error_docref(NULL, E_NOTICE, "amf3 cannot write strings longer than %d", AMF3_UINT29_MAX);
        return -1;
    }
    else {
        zval *val;
        if ((val = zend_hash_str_find(&(var_hash->strings), (char *)cp, len)) != NULL) {
            
            amf3_write_uint29(buf, ((int)Z_LVAL_P(val) - 1) << 1);
            return (int)Z_LVAL_P(val) - 1;
        }
        else {
            uint32_t index = ++var_hash->next_string_index;
            zval val;
            ZVAL_LONG(&val, index);
            zend_hash_str_add(&(var_hash->strings), (char *)cp, len, &val);
            amf3_write_uint29(buf, (((int)len << 1) | AMF_INLINE_ENTITY));

            amf_write_string(buf, cp, len);
            return index - 1;
        }
    }
}

/** writes a string from ZVAL in AMF3 format. Useful for memory reference optimization */
static int amf3_write_string_zval(amf_serialize_output buf, zval *zv, amf_context_data_t *var_hash)
{
    char *cp = Z_STRVAL_P(zv);
    size_t len = Z_STRLEN_P(zv);

    if (len == 0) {
        amf_write_byte(buf, 1);  /* inline and empty */
        return -1;
    }
    else if (len > AMF3_UINT29_MAX) {
        php_error_docref(NULL, E_NOTICE, "amf3 cannot write strings longer than %d", AMF3_UINT29_MAX);
        return -1;
    }
    else {
        zval *val;
        if ((val = zend_hash_str_find(&(var_hash->strings), (char *)cp, len)) != NULL) {
            amf3_write_uint29(buf, ((int)Z_LVAL_P(val) - 1) << 1);
            return (int)Z_LVAL_P(val) - 1;
        }
        else {
            uint32_t index = ++var_hash->next_string_index;
            zval val;
            ZVAL_LONG(&val, index);
            zend_hash_str_add(&(var_hash->strings), (char *)cp, len, &val);
            amf3_write_uint29(buf, (((int)len << 1) | AMF_INLINE_ENTITY));

            amf_write_string_zval(buf, zv);
            return index - 1;
        }
    }
}

static inline void amf3_write_head(amf_serialize_output buf, zval *amfc_type, int head)
{
    zend_long amfct;

    if (Z_TYPE_P(amfc_type) == IS_LONG) {
        amfct = Z_LVAL_P(amfc_type);
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
    }
    else {
        amf_write_byte(buf, AMF3_OBJECT);
    }
    amf3_write_uint29(buf, head);
}

/** writes the AMF3_OBJECT followed by the class information */
static inline void amf3_write_objecthead(amf_serialize_output buf, int head)
{
    amf_write_byte(buf, AMF3_OBJECT);
    amf3_write_uint29(buf, head);
}


static int amf_invoke_serialize_callback(zval *rval, zval *arg, amf_context_data_t *var_hash)
{
    int result, rtype = AMFC_TYPEDOBJECT;
    zval retval, params[1];

    ZVAL_COPY_VALUE(&params[0], arg);

    var_hash->fci.params = params;
    var_hash->fci.param_count = 1;
    var_hash->fci.no_separation = 0;
    var_hash->fci.retval = &retval;

    if ((result = zend_call_function(&(var_hash->fci), &(var_hash->fci_cache))) == FAILURE) {
        php_error_docref(NULL, E_NOTICE, "amf serialize callback problem");
        rtype = AMFC_NONE;
    }
    else if (EG(exception)) {
        php_error_docref(NULL, E_NOTICE, "amf serialize callback exception");
        rtype = AMFC_NONE;
    }
    else if (Z_TYPE(retval) == IS_ARRAY) {
        zval *tmp;
        HashTable *ht = Z_ARRVAL(retval);

        if ((tmp = zend_hash_index_find(ht, 0)) != NULL) {
            ZVAL_COPY(rval, tmp);
        }

        if ((tmp = zend_hash_index_find(ht, 1)) != NULL) {
            convert_to_long_ex(tmp);
            rtype = (int)Z_LVAL_P(tmp);
        }
    }

    zval_ptr_dtor(&retval);

    return rtype;
}

static int amf_get_explicit_type(zval *val, zval *explicit_type)
{
    if (Z_TYPE_P(val) != IS_OBJECT) {
        return 0;
    }

    HashTable *obj = Z_OBJPROP_P(val);
    int explicit_type_is_set = zend_hash_str_exists(obj, "_explicitType", sizeof("_explicitType") - 1);
    zval *tmp;

    if (explicit_type_is_set == 1) {
        if ((tmp = zend_hash_str_find(obj, "_explicitType", sizeof("_explicitType") - 1)) == NULL) {
            ZVAL_EMPTY_STRING(explicit_type);
        }
        else if (Z_TYPE_P(tmp) == IS_INDIRECT) {
            tmp = Z_INDIRECT_P(tmp);
            ZVAL_DEREF(tmp);
            if (Z_TYPE_P(tmp) == IS_STRING) {
                ZVAL_COPY_VALUE(explicit_type, tmp);
            }
            else {
                ZVAL_EMPTY_STRING(explicit_type);
            }
        }
        else if (Z_TYPE_P(tmp) == IS_STRING) {
            ZVAL_COPY_VALUE(explicit_type, tmp);
        }
        else {
            ZVAL_EMPTY_STRING(explicit_type);
        }
    }

    return explicit_type_is_set;
}

static void amf3_serialize_array(amf_serialize_output buf, HashTable *ht, amf_context_data_t *var_hash);
static void amf3_serialize_object(amf_serialize_output buf, zval *val, amf_context_data_t *var_hash);
static void amf3_serialize_object_anonymous(amf_serialize_output buf, HashTable *ht, amf_context_data_t *var_hash);
static void amf3_serialize_object_typed(amf_serialize_output buf, HashTable *ht, const char *class_name, amf_context_data_t *var_hash);
static void amf3_serialize_vector(amf_serialize_output buf, HashTable *ht, amf_context_data_t *var_hash);

static void amf3_serialize_var(amf_serialize_output buf, zval *val, amf_context_data_t *var_hash)
{
    switch (Z_TYPE_P(val)) {
        case IS_TRUE:
            amf_write_byte(buf, AMF3_TRUE);
            return;
        case IS_FALSE:
            amf_write_byte(buf, AMF3_FALSE);
            return;
        case IS_NULL:
            amf_write_byte(buf, AMF3_NULL);
            return;
        case IS_LONG: {
            zend_long d = Z_LVAL_P(val);
            if (d >= AMF3_INT28_MIN && d <= AMF3_INT28_MAX) {
                amf_write_byte(buf, AMF3_INTEGER);
                amf3_write_uint29(buf, (int)d);
            }
            else {
                amf_write_byte(buf, AMF3_NUMBER);
                amf_write_double(buf, (double)d, var_hash);
            }
        }   return;
        case IS_DOUBLE:
            amf_write_byte(buf, AMF3_NUMBER);
            amf_write_double(buf, Z_DVAL_P(val), var_hash);
            return;
        case IS_STRING:
            amf_write_byte(buf, AMF3_STRING);
            amf3_write_string_zval(buf, val, var_hash);
            return;
        case IS_RESOURCE:
        case IS_OBJECT:
            amf3_serialize_object(buf, val, var_hash);
            return;
        case IS_ARRAY: {
            uint32_t object_index;
            if (amf_cache_object(&(var_hash->objects), Z_ARRVAL_P(val), &object_index, &(var_hash->next_object_index), OCA_LOOKUP_AND_ADD) == FAILURE) {
                amf_write_byte(buf, AMF3_ARRAY);
                amf3_write_uint29(buf, (int)(object_index << 1));
            }
            else {
                amf3_serialize_array(buf, Z_ARRVAL_P(val), var_hash);
            }
        }   return;
        default:
            php_error_docref(NULL, E_NOTICE, "amf unknown PHP type %d\n", Z_TYPE_P(val));
            amf_write_byte(buf, AMF3_UNDEFINED);
            return;
    }
}

/** serializes an array in AMF3 format */
static void amf3_serialize_array(amf_serialize_output buf, HashTable *ht, amf_context_data_t *var_hash)
{
    int num_elements = zend_hash_num_elements(ht);
    if (num_elements == 0) {
        amf_write_byte(buf, AMF3_ARRAY);
        amf3_write_uint29(buf, 0 | 1);
        amf3_write_emptystring(buf);
        return;
    }
    else {
        /* TODO: optimize writing of packed arrays, look at HT_IS_PACKED and HT_IS_WITHOUT_HOLES for detection */

        zend_ulong hdx;
        zend_string *key;
        zval *val;

        zend_long dense_count = 0, assoc_count = 0, max = -1;

        ZEND_HASH_FOREACH_KEY(ht, hdx, key) {
            if (key == NULL && hdx < ZEND_LONG_MAX) {
                max = hdx;
            }
            if (key != NULL || hdx > ZEND_LONG_MAX || ((zend_long)hdx > 0 && max != dense_count)) {
                assoc_count++;
            }
            else {
                dense_count++;
            }
        } ZEND_HASH_FOREACH_END();

        if (dense_count > AMF3_UINT29_MAX) {
            php_error_docref(NULL, E_NOTICE, "amf serialize array. Too many array items.");
            dense_count = AMF3_UINT29_MAX;
        }

        /* Encode string, mixed, sparse, or negative index arrays as anonymous object, associative array otherwise. */
        if ((var_hash->flags & AMF3_NSND_ARRAY_AS_OBJECT) != 0 && (assoc_count > 0 || max != dense_count - 1)) {
            amf3_write_objecthead(buf, AMF_INLINE_ENTITY | AMF_INLINE_CLASS | AMF_CLASS_DYNAMIC);
            amf3_write_emptystring(buf);
            var_hash->next_trait_index++;

            ZEND_HASH_FOREACH_KEY_VAL(ht, hdx, key, val) {
                if (key == NULL) {
                    char str[32];
                    sprintf(str, "%d", hdx);
                    size_t len = strlen(str);

                    amf3_write_string(buf, str, len, var_hash);
                }
                else {
                    amf3_write_string(buf, ZSTR_VAL(key), ZSTR_LEN(key), var_hash);
                }
                amf3_serialize_var(buf, val, var_hash);
            } ZEND_HASH_FOREACH_END();

            amf3_write_emptystring(buf);
        }
        else {
            amf_write_byte(buf, AMF3_ARRAY);
            amf3_write_uint29(buf, ((int)dense_count << 1) | AMF_INLINE_ENTITY);

            /* string keys */
            if (assoc_count > 0) {
                ZEND_HASH_FOREACH_KEY_VAL(ht, hdx, key, val) {
                    int skip = 0;
                    if (key == NULL) {
                        if (hdx > ZEND_LONG_MAX || (zend_long)hdx > dense_count) {
                            char str[32];
                            sprintf(str, "%d", hdx);
                            size_t len = strlen(str);

                            amf3_write_string(buf, str, len, var_hash);
                        } 
                        else {
                            skip = 1;
                        }
                    }
                    else {
                        amf3_write_string(buf, ZSTR_VAL(key), ZSTR_LEN(key), var_hash);
                    }
                    if (skip) {
                        continue;
                    }
                    amf3_serialize_var(buf, val, var_hash);
                } ZEND_HASH_FOREACH_END();
            }

            /* place the empty string to indicate the end of key/value pairs */
            amf3_write_emptystring(buf);

            /* now the linear data, we need to lookup the data because of the sorting */
            if (dense_count > 0) {
                int i;
                for (i = 0; i < dense_count; i++) {
                    if ((val = zend_hash_index_find(ht, i)) == NULL) {
                        amf_write_byte(buf, AMF3_UNDEFINED);
                    }
                    else {
                        amf3_serialize_var(buf, val, var_hash);
                    }
                }
            }
        }
    }
}

static void amf3_serialize_object(amf_serialize_output buf, zval *val, amf_context_data_t *var_hash)
{
    const char *class_name = Z_TYPE_P(val) == IS_RESOURCE ? "" : ZSTR_VAL(Z_OBJCE_P(val)->name);
    size_t class_name_len = Z_TYPE_P(val) == IS_RESOURCE ? "" : ZSTR_LEN(Z_OBJCE_P(val)->name);
    uint32_t object_index;
    int explicit_type_is_set = 0;
    zval *amfc_type, explicit_type;

    /* if the object is already in cache then just go for it */
    if (amf_cache_object_typed(var_hash, val, &object_index, 1, OCA_LOOKUP_ONLY, -1) == FAILURE) {
        if ((amfc_type = zend_hash_index_find(&(var_hash->objtypes), object_index)) != NULL) {
            amf3_write_head(buf, amfc_type, ((int)object_index << 1));
        }
        else {
            amf3_write_objecthead(buf, ((int)object_index << 1));
        }
        return;
    }

    explicit_type_is_set = amf_get_explicit_type(val, &explicit_type);

    if ((strcmp(class_name, "stdClass") == 0 && explicit_type_is_set == 0)
        || (explicit_type_is_set == 1 && Z_TYPE(explicit_type) == IS_STRING && Z_STRLEN(explicit_type) == 0)
    ) {
        amf_cache_object(&(var_hash->objects), Z_OBJPROP_P(val), &object_index, &(var_hash->next_object_index), OCA_LOOKUP_AND_ADD);
        amf3_serialize_object_anonymous(buf, Z_OBJPROP_P(val), var_hash);
    }
    else {
        int rtype = AMFC_TYPEDOBJECT;
        zval rval;

        rtype = amf_invoke_serialize_callback(&rval, val, var_hash);

        if (Z_TYPE(rval) == IS_RESOURCE) {
            php_error_docref(NULL, E_NOTICE, "amf encoding callback. Resources should be transformed to something");
            amf_write_byte(buf, AMF3_UNDEFINED);
            return;
        }

        switch (rtype) {
            case AMFC_RAW:
                if (Z_TYPE(rval) == IS_STRING) {
                    amf_write_string_zval(buf, &rval);
                    zval_ptr_dtor(&rval);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_RAW requires a string");
                }
                break;
            case AMFC_OBJECT:
                if (amf_cache_object_typed(var_hash, &rval, &object_index, 1, OCA_LOOKUP_AND_ADD, AMFC_OBJECT) == FAILURE) {
                    amf3_write_objecthead(buf, (int)object_index << 1);
                }
                else if (Z_TYPE(rval) == IS_OBJECT) {
                    amf3_serialize_object_anonymous(buf, Z_OBJPROP(rval), var_hash);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_OBJECT requires an object or an array");
                }
                break;
            case AMFC_TYPEDOBJECT: {
                int free_remote_class_name = 0;
                char *remote_class_name = NULL;
                if (explicit_type_is_set == 1 && Z_TYPE(explicit_type) == IS_STRING && Z_STRLEN(explicit_type) > 0) {
                    remote_class_name = Z_STRVAL(explicit_type);
                }
                else if (memchr(class_name, '\\', class_name_len)) {
                    free_remote_class_name = 1;
                    remote_class_name = guard_emalloc(class_name_len);
                    memset(remote_class_name, '\0', class_name_len);
                    remote_class_name = backslash_to_dot(class_name, remote_class_name, class_name_len);
                    if (remote_class_name[class_name_len] != '\0') {
                        remote_class_name[class_name_len] = '\0';
                    }
                }
                else {
                    remote_class_name = class_name;
                }
                if (amf_cache_object_typed(var_hash, &rval, &object_index, 1, OCA_LOOKUP_AND_ADD, AMFC_TYPEDOBJECT) == FAILURE) {
                    amf3_write_objecthead(buf, (int)object_index << 1);
                }
                else if (Z_TYPE(rval) == IS_OBJECT) {
                    amf3_serialize_object_typed(buf, Z_OBJPROP(rval), remote_class_name, var_hash);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_TYPEDOBJECT requires an object or an array");
                }
                zval_ptr_dtor(&rval);
                if (free_remote_class_name == 1) {
                    efree(remote_class_name);
                }
            }    break;
            case AMFC_ARRAY:
                if (amf_cache_object_typed(var_hash, &rval, &object_index, 1, OCA_LOOKUP_AND_ADD, AMFC_ARRAY) == FAILURE) {
                    amf3_write_objecthead(buf, (int)object_index << 1);
                }
                else if (Z_TYPE(rval) == IS_ARRAY) {
                    amf3_serialize_array(buf, Z_ARRVAL(rval), var_hash);
                }
                else if (Z_TYPE(rval) == IS_OBJECT) {
                    amf3_serialize_array(buf, Z_OBJPROP(rval), var_hash);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_ARRAY requires an object or an array");
                }
                break;
            case AMFC_DATE:
                if (amf_cache_object_typed(var_hash, val, &object_index, 1, OCA_LOOKUP_AND_ADD, AMFC_DATE) == FAILURE) {
                    amf_write_byte(buf, AMF3_DATE);
                    amf3_write_uint29(buf, (int)object_index << 1);
                }
                else if (Z_TYPE(rval) == IS_DOUBLE) {
                    amf_write_byte(buf, AMF3_DATE);
                    amf3_write_uint29(buf, 1);
                    amf_write_double(buf, Z_DVAL(rval), var_hash);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_DATE requires a double");
                }
                break;
            case AMFC_XML:
                if (amf_cache_object_typed(var_hash, val, &object_index, 1, OCA_LOOKUP_AND_ADD, AMFC_XML) == FAILURE) {
                    amf_write_byte(buf, AMF3_XML);
                    amf3_write_uint29(buf, (int)object_index << 1);
                }
                else if (Z_TYPE(rval) == IS_STRING) {
                    amf_write_byte(buf, AMF3_XML);
                    amf3_write_string_zval(buf, &rval, var_hash);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_XML requires a string");
                }
                zval_ptr_dtor(&rval);
                break;
            case AMFC_XMLDOCUMENT:
                if (amf_cache_object_typed(var_hash, val, &object_index, 1, OCA_LOOKUP_AND_ADD, AMFC_XMLDOCUMENT) == FAILURE) {
                    amf_write_byte(buf, AMF3_XMLDOCUMENT);
                    amf3_write_uint29(buf, (int)object_index << 1);
                }
                else if (Z_TYPE(rval) == IS_STRING) {
                    amf_write_byte(buf, AMF3_XMLDOCUMENT);
                    amf3_write_string_zval(buf, &rval, var_hash);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_XML requires a string");
                }
                zval_ptr_dtor(&rval);
                break;
            case AMFC_BYTEARRAY:
                if (amf_cache_object_typed(var_hash, val, &object_index, 1, OCA_LOOKUP_AND_ADD, AMFC_BYTEARRAY) == FAILURE) {
                    amf_write_byte(buf, AMF3_BYTEARRAY);
                    amf3_write_uint29(buf, (int)object_index << 1);
                }
                else if (Z_TYPE(rval) == IS_STRING) {
                    amf_write_byte(buf, AMF3_BYTEARRAY);
                    amf3_write_string_zval(buf, &rval, var_hash);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_BYTEARRAY requires a string");
                }
                break;
            case AMFC_VECTOR_OBJECT:
                if (amf_cache_object_typed(var_hash, &rval, &object_index, 1, OCA_LOOKUP_AND_ADD, AMFC_VECTOR_OBJECT) == FAILURE) {
                    amf_write_byte(buf, AMF3_VECTOR_OBJECT);
                    amf3_write_uint29(buf, (int)object_index << 1);
                }
                else if (Z_TYPE(rval) == IS_ARRAY) {
                    amf3_serialize_vector(buf, Z_ARRVAL(rval), var_hash);
                }
                else if (Z_TYPE(rval) == IS_OBJECT) {
                    amf3_serialize_vector(buf, Z_OBJPROP(rval), var_hash);
                }
                else {
                    amf_write_byte(buf, AMF3_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_ARRAY requires an object or an array");
                }
                zval_ptr_dtor(&rval);
                break;
            case AMFC_ANY:
                amf3_serialize_var(buf, &rval, var_hash);
                break;
            case AMFC_NONE:
                amf_write_byte(buf, AMF3_UNDEFINED);
                break;
            default:
                amf_write_byte(buf, AMF3_UNDEFINED);
                php_error_docref(NULL, E_NOTICE, "amf encoding callback. unknown type %d", rtype);
                break;
        }
    }
}

static void amf3_serialize_object_anonymous(amf_serialize_output buf, HashTable *ht, amf_context_data_t *var_hash)
{
    zend_ulong hdx;
    zend_string *key;
    zval *val;

    {
        int memberCount = 0;
        zend_ulong var_no = var_hash->next_trait_index++;
        const int isDynamic = AMF_CLASS_DYNAMIC;
        const int isExternalizable = 0;  /* AMF_CLASS_EXTERNALIZABLE */

        zend_hash_index_add_empty_element(&(var_hash->traits), var_no);
        amf3_write_objecthead(buf, memberCount << AMF_CLASS_MEMBERCOUNT_SHIFT | isExternalizable | isDynamic | AMF_INLINE_CLASS | AMF_INLINE_ENTITY);
        amf3_write_string(buf, "", 0, var_hash);
    }

    ZEND_HASH_FOREACH_KEY_VAL(ht, hdx, key, val) {
        if (key == NULL) {
            char str[32];
            sprintf(str, "%d", hdx);
            size_t len = strlen(str);

            amf3_write_string(buf, str, len, var_hash);
        }
        else {
            /* Don't write private/protected properties or explicit type */
            if (&key[0] == 0 || strcmp(ZSTR_VAL(key), "_explicitType") == 0) {
                continue;
            }
            amf3_write_string(buf, ZSTR_VAL(key), ZSTR_LEN(key), var_hash);
        }

        /* Add element even if it's not OK, since we already wrote the length of the array before */
        if (Z_TYPE_P(val) == IS_UNDEF) {
            amf_write_byte(buf, AMF0_UNDEFINED);
        }
        else {
            amf3_serialize_var(buf, val, var_hash);
        }
    } ZEND_HASH_FOREACH_END();

    amf3_write_emptystring(buf);
}

/** from zend_operators.c */
static int my_hash_zval_identical_function(zval *z1, zval *z2)
{
    zval result;

    /* is_identical_function() returns 1 in case of identity and 0 in case
     * of a difference;
     * whereas this comparison function is expected to return 0 on identity,
     * and non zero otherwise.
     */
    ZVAL_DEREF(z1);
    ZVAL_DEREF(z2);
    if (is_identical_function(&result, z1, z2) == FAILURE) {
        return 1;
    }
    return Z_TYPE(result) != IS_TRUE;
}

/** serializes a Hash Table as AMF3 plain object */
static void amf3_serialize_object_typed(amf_serialize_output buf, HashTable *ht, const char *class_name, amf_context_data_t *var_hash)
{
    size_t class_name_len = strlen(class_name);
    zend_string *key;
    zval *val, *data, ti, properties, reference;
    ulong ref;
    HashTable *htCached = NULL, *htCachedProps = NULL, *htProperties = NULL;
    int isDynamic = 0; /* AMF_CLASS_DYNAMIC */
    int isExternalizable = 0;  /* AMF_CLASS_EXTERNALIZABLE */
    int propCount = 0;
    int cleanup = 1;

    /* ti (traits info) is an associative array containing the reference, and an array of property names */
    array_init(&ti);
    array_init(&properties);

    add_assoc_zval(&ti, "properties", &properties);
    htProperties = Z_ARRVAL(properties);

    ZEND_HASH_FOREACH_STR_KEY(ht, key) {
        /* Don't write private/protected properties or explicit type */
        if (&key[0] == 0 || strcmp(ZSTR_VAL(key), "_explicitType") == 0) {
            continue;
        }
        propCount++;
        add_assoc_null(&properties, ZSTR_VAL(key));
    } ZEND_HASH_FOREACH_END();

    if ((val = zend_hash_str_find(&(var_hash->traits), class_name, class_name_len)) != NULL) {
        htCached = Z_ARRVAL_P(val);
        if ((data = zend_hash_str_find(htCached, "properties", sizeof("properties") - 1)) == NULL) {
            php_error_docref(NULL, E_NOTICE, "amf traits info problem");
        }
        else {
            htCachedProps = Z_ARRVAL_P(data);
            if (zend_hash_compare(htCachedProps, htProperties, (compare_func_t)my_hash_zval_identical_function, 1) == 0) {
                if ((data = zend_hash_str_find(htCached, "reference", sizeof("reference") - 1)) == NULL) {
                    php_error_docref(NULL, E_NOTICE, "amf traits info problem");
                }
                else {
                    ref = (int)Z_LVAL_P(data) << AMF_CLASS_SHIFT | AMF_INLINE_ENTITY;
                    amf3_write_objecthead(buf, ref);
                }
            }
            else {
                isDynamic = AMF_CLASS_DYNAMIC;
                ref = 3 | isExternalizable | isDynamic | (0 << 4);
                amf3_write_objecthead(buf, ref);
                amf3_write_string(buf, class_name, class_name_len, var_hash);
            }
        }
    }
    else {
        cleanup = 0;
        ZVAL_LONG(&reference, var_hash->next_trait_index++);
        add_assoc_zval(&ti, "reference", &reference);
        zend_hash_str_add(&(var_hash->traits), (char *)class_name, class_name_len, &ti);

        ref = 3 | isExternalizable | isDynamic | (propCount << 4);
        amf3_write_objecthead(buf, ref);
        amf3_write_string(buf, class_name, class_name_len, var_hash);

        ZEND_HASH_FOREACH_STR_KEY(htProperties, key) {
            amf3_write_string(buf, ZSTR_VAL(key), ZSTR_LEN(key), var_hash);
        } ZEND_HASH_FOREACH_END();
    }

    if (isDynamic == 0) {
        ZEND_HASH_FOREACH_STR_KEY_VAL_IND(ht, key, val) {
            /* Don't write private/protected properties or explicit type */
            if (&key[0] == 0 || strcmp(ZSTR_VAL(key), "_explicitType") == 0) {
                continue;
            }
            amf3_serialize_var(buf, val, var_hash);
        } ZEND_HASH_FOREACH_END();
    }
    else {
        ZEND_HASH_FOREACH_STR_KEY_VAL_IND(ht, key, val) {
            /* Don't write private/protected properties or explicit type */
            if (&key[0] == 0 || strcmp(ZSTR_VAL(key), "_explicitType") == 0) {
                continue;
            }
            amf3_write_string(buf, ZSTR_VAL(key), ZSTR_LEN(key), var_hash);
            amf3_serialize_var(buf, val, var_hash);
        } ZEND_HASH_FOREACH_END();

        amf3_write_emptystring(buf);
    }

    if (cleanup == 1) {
        zval_ptr_dtor(&ti);
    }
}

/** */
static void amf3_serialize_vector(amf_serialize_output buf, HashTable *ht, amf_context_data_t *var_hash)
{
    if (zend_hash_num_elements(ht) == 0) {
        return;
    }

    zend_ulong hdx;
    zend_string *key;
    zval *val, *type, *fixed, *data;
    HashTable *htData;
    
    if ((type = zend_hash_str_find_ind(ht, "type", sizeof("type") - 1)) == NULL || Z_TYPE_P(type) != IS_LONG) {
        return;
    }
    if ((fixed = zend_hash_str_find_ind(ht, "fixed", sizeof("fixed") - 1)) == NULL || (Z_TYPE_P(fixed) != IS_TRUE && Z_TYPE_P(fixed) != IS_FALSE)) {
        return;
    }
    if ((data = zend_hash_str_find_ind(ht, "data", sizeof("data") - 1)) == NULL || Z_TYPE_P(data) != IS_ARRAY) {
        return;
    }

    htData = Z_ARRVAL_P(data);
    size_t len = zend_hash_num_elements(htData);

    if (len > AMF3_UINT29_MAX) {
        php_error_docref(NULL, E_NOTICE, "amf3 cannot write vector with more than %d items", AMF3_UINT29_MAX);
        return;
    }

    amf_write_byte(buf, (int)Z_LVAL_P(type));
    amf3_write_uint29(buf, ((int)len << 1) | AMF_INLINE_ENTITY);
    amf_write_byte(buf, Z_TYPE_P(fixed) == IS_TRUE ? 1 : 0);

    if (Z_LVAL_P(type) == AMF3_VECTOR_INT) {
        ZEND_HASH_FOREACH_KEY_VAL_IND(htData, hdx, key, val) {
            if (key != NULL || hdx > ZEND_LONG_MAX || Z_TYPE_P(val) != IS_LONG) {
                amf_write_byte(buf, AMF3_UNDEFINED);
            }
            else {
                amf3_write_vector_int(buf, (int)Z_LVAL_P(val), var_hash);
            }
        } ZEND_HASH_FOREACH_END();
    }
    else if (Z_LVAL_P(type) == AMF3_VECTOR_UINT) {
        ZEND_HASH_FOREACH_KEY_VAL_IND(htData, hdx, key, val) {
            if (key != NULL || (Z_TYPE_P(val) != IS_LONG && Z_TYPE_P(val) != IS_DOUBLE)) {
                amf_write_byte(buf, AMF3_UNDEFINED);
            }
            else {
                if (Z_TYPE_P(val) == IS_LONG) {
                    amf3_write_vector_uint(buf, (uint)Z_LVAL_P(val), var_hash);
                }
                else {
                    amf3_write_vector_uint(buf, (uint)Z_DVAL_P(val), var_hash);
                }
            }
        } ZEND_HASH_FOREACH_END();
    }
    else if (Z_LVAL_P(type) == AMF3_VECTOR_DOUBLE) {
        ZEND_HASH_FOREACH_KEY_VAL_IND(htData, hdx, key, val) {
            if (key != NULL || hdx > ZEND_LONG_MAX || (Z_TYPE_P(val) != IS_LONG && Z_TYPE_P(val) != IS_DOUBLE)) {
                amf_write_byte(buf, AMF3_UNDEFINED);
            }
            else {
                if (Z_TYPE_P(val) == IS_LONG) {
                    amf3_write_vector_double(buf, (double)Z_LVAL_P(val), var_hash);
                }
                else {
                    amf3_write_vector_double(buf, Z_DVAL_P(val), var_hash);
                }
            }
        } ZEND_HASH_FOREACH_END();
    }
    else if (Z_LVAL_P(type) == AMF3_VECTOR_OBJECT) {
        ZEND_HASH_FOREACH_KEY_VAL_IND(htData, hdx, key, val) {
            if (key != NULL || hdx > ZEND_LONG_MAX || Z_TYPE_P(val) != IS_OBJECT) {
                amf_write_byte(buf, AMF3_UNDEFINED);
            }
            else if (hdx == 0) {
                const char *class_name = "";
                size_t class_name_len = 0;
                int explicit_type_is_set = 0;
                zval explicit_type;

                class_name = ZSTR_VAL(Z_OBJ_P(val)->ce->name);
                explicit_type_is_set = amf_get_explicit_type(val, &explicit_type);

                if ((strcmp(class_name, "stdClass") == 0 && explicit_type_is_set == 0)
                    || (explicit_type_is_set == 1 && Z_TYPE(explicit_type) == IS_STRING && Z_STRLEN(explicit_type) == 0)
                ) {
                    class_name = "Object";
                    class_name_len = (int)strlen(class_name);
                }
                else if (explicit_type_is_set == 1) {
                    class_name = Z_STRVAL(explicit_type);
                }
                else {
                    char *tmp = (char *)class_name;
                    zend_string *zclass_name;
                    zclass_name = php_str_to_str(tmp, strlen(tmp), "\\", sizeof("\\") - 1, ".", sizeof(".") - 1);
                    class_name = ZSTR_VAL(zclass_name);
                    class_name_len = ZSTR_LEN(zclass_name);
                }
                amf3_write_string(buf, class_name, (int)class_name_len, var_hash);
                amf3_serialize_object(buf, val, var_hash);
            }
            else {
                amf3_serialize_object(buf, val, var_hash);
            }
        } ZEND_HASH_FOREACH_END();
    }
}

static void amf0_serialize_array(amf_serialize_output buf, HashTable *ht, amf_context_data_t *var_hash);
static void amf0_serialize_object(amf_serialize_output buf, zval *val, amf_context_data_t *var_hash);
static void amf0_serialize_object_data(amf_serialize_output buf, HashTable *ht, int is_array, amf_context_data_t *var_hash);

static void amf0_serialize_var(amf_serialize_output buf, zval *val, amf_context_data_t *var_hash)
{
    switch (Z_TYPE_P(val)) {
        case IS_TRUE:
        case IS_FALSE:
            amf_write_byte(buf, AMF0_BOOLEAN);
            amf_write_byte(buf, Z_LVAL_P(val) ? 1 : 0);
            return;
        case IS_NULL:
            amf_write_byte(buf, AMF0_NULL);
            return;
        case IS_LONG:
            amf_write_byte(buf, AMF0_NUMBER);
            amf_write_double(buf, (double)Z_LVAL_P(val), var_hash);
            return;
        case IS_DOUBLE:
            if (Z_DVAL_P(val) > AMF_U32_MAX) {
                convert_to_string(val);
                amf_write_byte(buf, AMF0_STRING);
                amf0_write_short(buf, (int)Z_STRLEN_P(val));
                amf_write_string_zval(buf, val);
            }
            else {
                amf_write_byte(buf, AMF0_NUMBER);
                amf_write_double(buf, Z_DVAL_P(val), var_hash);
            }
            return;
        case IS_STRING:
            amf0_write_string_zval(buf, val, var_hash);
            return;
        case IS_RESOURCE:
        case IS_OBJECT:
            amf0_serialize_object(buf, val, var_hash);
            return;
        case IS_ARRAY: {
            uint32_t object_index;
            if (amf_cache_object(&(var_hash->objects0), Z_ARRVAL_P(val), &object_index, &(var_hash->next_object0_index), OCA_LOOKUP_AND_ADD) == FAILURE) {
                amf_write_byte(buf, AMF0_REFERENCE);
                amf0_write_short(buf, object_index);
            }
            else {
                amf0_serialize_array(buf, Z_ARRVAL_P(val), var_hash);
            }
        }    return;
        default:
            php_error_docref(NULL, E_NOTICE, "amf cannot understand php type %d", Z_TYPE_P(val));
            amf_write_byte(buf, AMF0_UNDEFINED);
            return;
    }
}

/** serializes an array in AMF0 format */
static void amf0_serialize_array(amf_serialize_output buf, HashTable *ht, amf_context_data_t *var_hash)
{
    int num_elements = zend_hash_num_elements(ht);
    if (num_elements == 0) {
        static char emptyArray[] = { 10,0,0,0,0 };
        amf_write_string(buf, emptyArray, 5);
        return;
    } 
    else {
        zend_ulong hdx;
        zend_string *key;
        zval *val;
        
        int has_negative = 0; 
        zend_long str_count = 0, num_count = 0, max = -1;

        ZEND_HASH_FOREACH_KEY(ht, hdx, key) {
            if (key == NULL) {
                if (hdx > ZEND_LONG_MAX) {
                    has_negative = 1;
                    str_count++;
                }
                else {
                    num_count++;
                    if ((zend_long)hdx > max) {
                        max = (zend_long)hdx;
                    }
                }
            }
            else {
                str_count++;
            }

        } ZEND_HASH_FOREACH_END();

        if (num_count > AMF_U32_MAX || max > AMF_U32_MAX) {
            php_error_docref(NULL, E_NOTICE, "amf0 cannot write array with more than %d items", AMF_U32_MAX);
            return;
        }

        /* key with name or negative indices means mixed array */
        if (num_count > 0 && (str_count > 0 || has_negative)) {
            amf_write_byte(buf, AMF0_MIXEDARRAY);
            amf0_write_int(buf, num_elements);
            amf0_serialize_object_data(buf, ht, 1, var_hash);
        }
        else if (num_count > 0) { /* numeric keys only */
            int i;
            amf_write_byte(buf, AMF0_ARRAY);
            if (max == num_count - 1) {
                /* dense array */
                amf0_write_int(buf, (int)num_count);
                /* lookup the key, if non-existent use 0x6 undefined */
                for (i = 0; i < num_count; i++) {
                    if ((val = zend_hash_index_find(ht, i)) == NULL) {
                        amf_write_byte(buf, AMF0_UNDEFINED);
                    }
                    else {
                        amf0_serialize_var(buf, val, var_hash);
                    }
                }
            }
            else {
                /* sparse array */
                amf0_write_int(buf, (int)max + 1);
                for (i = 0; i < max + 1; i++) {
                    if ((val = zend_hash_index_find(ht, i)) == NULL) {
                        amf_write_byte(buf, AMF0_UNDEFINED);
                    }
                    else {
                        amf0_serialize_var(buf, val, var_hash);
                    }
                }
            }
        }
        else { /* string keys only */
            amf_write_byte(buf, AMF0_OBJECT);
            amf0_serialize_object_data(buf, ht, 1, var_hash);
        }
        return;
    }
}

static void amf0_serialize_object(amf_serialize_output buf, zval *val, amf_context_data_t *var_hash)
{
    const char *class_name = Z_TYPE_P(val) == IS_RESOURCE ? "" : ZSTR_VAL(Z_OBJCE_P(val)->name);
    size_t class_name_len = Z_TYPE_P(val) == IS_RESOURCE ? "" : ZSTR_LEN(Z_OBJCE_P(val)->name);
    uint32_t object_index;
    int explicit_type_is_set = 0;
    zval *tmp, explicit_type;

    /* if the object is already in cache then just go for it */
    if (amf_cache_object_typed(var_hash, val, &object_index, 1, OCA_LOOKUP_ONLY, -1) == FAILURE) {
        amf_write_byte(buf, AMF0_REFERENCE);
        amf0_write_short(buf, object_index);
        return;
    }

    explicit_type_is_set = amf_get_explicit_type(val, &explicit_type);

    if ((strcmp(class_name, "stdClass") == 0 && explicit_type_is_set == 0)
        || (explicit_type_is_set == 1 && Z_TYPE(explicit_type) == IS_STRING && Z_STRLEN(explicit_type) == 0)
    ) {
        amf_write_byte(buf, AMF0_OBJECT);
        amf0_serialize_object_data(buf, Z_OBJPROP_P(val), 0, var_hash);
    }
    else {
        int rtype = AMFC_TYPEDOBJECT;
        zval rval;

        rtype = amf_invoke_serialize_callback(&rval, val, var_hash);

        if (Z_TYPE(rval) == IS_RESOURCE) {
            php_error_docref(NULL, E_NOTICE, "amf encoding callback. Resources should be transformed to something");
            amf_write_byte(buf, AMF0_UNDEFINED);
            return; 
        }

        switch (rtype) {
            case AMFC_RAW:
                amf_write_string_zval(buf, &rval);
                zval_ptr_dtor(&rval);
                break;
            case AMFC_XML:
            case AMFC_XMLDOCUMENT:
                if (Z_STRLEN(rval) > AMF_U32_MAX) {
                    zval_ptr_dtor(&rval);
                    php_error_docref(NULL, E_NOTICE, "amf0 cannot write xml strings longer than %d", AMF_U32_MAX);
                    return;
                }
                amf_write_byte(buf, AMF0_XML);
                amf0_write_int(buf, (int)Z_STRLEN(rval));
                amf_write_string_zval(buf, &rval);
                zval_ptr_dtor(&rval);
                break;
            case AMFC_OBJECT:
                if (Z_TYPE(rval) == IS_OBJECT) {
                    if (amf_cache_object_typed(var_hash, &rval, &object_index, 0, OCA_LOOKUP_AND_ADD, AMFC_OBJECT) == FAILURE) {
                        amf_write_byte(buf, AMF0_REFERENCE);
                        amf0_write_short(buf, object_index);
                    }
                    else {
                        amf_write_byte(buf, AMF0_OBJECT);
                        amf0_serialize_object_data(buf, Z_OBJPROP(rval), 0, var_hash);
                    }
                }
                else {
                    amf_write_byte(buf, AMF0_NULL);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_OBJECT requires an object");
                }
                break;
            case AMFC_ARRAY:
                if (amf_cache_object_typed(var_hash, &rval, &object_index, 0, OCA_LOOKUP_AND_ADD, AMFC_ARRAY) == FAILURE) {
                    amf_write_byte(buf, AMF0_REFERENCE);
                    amf0_write_short(buf, object_index);
                }
                else if (Z_TYPE(rval) == IS_ARRAY) {
                    amf0_serialize_array(buf, Z_ARRVAL(rval), var_hash);
                }
                else if (Z_TYPE(rval) == IS_OBJECT) {
                    amf0_serialize_array(buf, Z_OBJPROP(rval), var_hash);
                }
                else {
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_ARRAY requires an object or an array");
                }
                break;
            case AMFC_TYPEDOBJECT:
                if (amf_cache_object_typed(var_hash, &rval, &object_index, 0, OCA_LOOKUP_AND_ADD, AMFC_TYPEDOBJECT) == FAILURE) {
                    amf_write_byte(buf, AMF0_REFERENCE);
                    amf0_write_short(buf, object_index);
                }
                else {
                    int free_remote_class_name = 0;
                    char *remote_class_name = NULL;
                    if (explicit_type_is_set == 1 && Z_TYPE(explicit_type) == IS_STRING && Z_STRLEN(explicit_type) > 0) {
                        remote_class_name = Z_STRVAL(explicit_type);
                    }
                    else if (memchr(class_name, '\\', class_name_len)) {
                        free_remote_class_name = 1;
                        remote_class_name = guard_emalloc(class_name_len);
                        memset(remote_class_name, '\0', class_name_len);
                        remote_class_name = backslash_to_dot(class_name, remote_class_name, class_name_len);
                        if (remote_class_name[class_name_len] != '\0') {
                            remote_class_name[class_name_len] = '\0';
                        }
                    }
                    else {
                        remote_class_name = class_name;
                    }
                    amf_write_byte(buf, AMF0_TYPEDOBJECT);
                    if (Z_TYPE(rval) == IS_OBJECT) {
                        amf0_write_shortstring(buf, remote_class_name, var_hash);
                        amf0_serialize_object_data(buf, Z_OBJPROP(rval), 0, var_hash);
                    }
                    else {
                        amf0_write_shortstring(buf, remote_class_name, var_hash);
                        amf0_serialize_object_data(buf, Z_ARRVAL(rval), 0, var_hash);
                    }
                    zval_ptr_dtor(&rval);
                    if (free_remote_class_name == 1) {
                        efree(remote_class_name);
                    }
                }
                break;
            case AMFC_ANY:
                amf0_serialize_var(buf, &rval, var_hash);
                break;
            case AMFC_EXTERNAL:
                /*amf3_write_objecthead(buf, AMF_INLINE_CLASS | AMF_INLINE_ENTITY);
                amf3_write_string(buf, class_name, class_name_len, AMF_STRING_AS_TEXT, var_hash);
                amf3_serialize_var(buf, rval, var_hash);*/
                break;
            case AMFC_NONE:
                amf_write_byte(buf, AMF0_UNDEFINED);
                break;
            case AMFC_BYTEARRAY:
                if (Z_TYPE(rval) == IS_STRING) {
                    amf0_write_string_zval(buf, &rval, var_hash);
                }
                else {
                    amf_write_byte(buf, AMF0_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_BYTEARRAY requires a string");
                }
                break;
            case AMFC_DATE:
                if (Z_TYPE(rval) == IS_DOUBLE) {
                    amf_write_byte(buf, AMF0_DATE);
                    amf_write_double(buf, Z_DVAL(rval), var_hash);
                    amf0_write_short(buf, 0);
                }
                else {
                    amf_write_byte(buf, AMF0_UNDEFINED);
                    php_error_docref(NULL, E_NOTICE, "amf encoding callback. AMFC_DATE requires a double");
                }
                break;
            default:
                amf_write_byte(buf, AMF0_UNDEFINED);
                php_error_docref(NULL, E_NOTICE, "amf encoding callback. unknown type %d", rtype);
                break;
        }
    }
}

static void amf0_serialize_object_data(amf_serialize_output buf, HashTable *ht, int is_array, amf_context_data_t *var_hash)
{
    zend_ulong hdx;
    zend_string *key;
    zval *val;

    ZEND_HASH_FOREACH_KEY_VAL_IND(ht, hdx, key, val) {
        if (key == NULL) {
            char str[32];
            sprintf(str, "%d", hdx);
            size_t len = strlen(str);

            amf0_write_short(buf, (int)len);
            amf_write_string(buf, str, len);
        }
        else {
            /* Don't write protected properties or explicit type */
            if (is_array == 0 && (&key[0] == 0 || strcmp(ZSTR_VAL(key), "_explicitType") == 0)) {
                continue;
            }
            amf0_write_short(buf, (int)ZSTR_LEN(key));
            amf_write_string(buf, ZSTR_VAL(key), (int)ZSTR_LEN(key));
        }

        /* Add element even if it's not OK, since we already wrote the length of the array before */
        if (Z_TYPE_P(val) == IS_UNDEF) {
            amf_write_byte(buf, AMF0_UNDEFINED);
        }
        else {
            amf0_serialize_var(buf, val, var_hash);
        }
    } ZEND_HASH_FOREACH_END();

    amf0_write_objectend(buf);
}

/**
 * encodes a string into amf format
 * \param value to be ancoded
 * \param flags for encoding AMF_AMF3 AMF_BIGENDIAN
 * \param callback (array or single functionreference)
 */
PHP_FUNCTION(amf_encode)
{
    amf_context_data_t var_hash;
    zval *val, *zFlags;
    int flags = 0;
    
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
            if (zend_parse_parameters(1, "z", &val) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            break;
        case 2:
            if (zend_parse_parameters(2, "zz", &val, &zFlags) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            convert_to_long_ex(zFlags);
            flags = (int)Z_LVAL_P(zFlags);
            break;
        default:
            if (zend_parse_parameters((ZEND_NUM_ARGS() > 4 ? 4 : ZEND_NUM_ARGS()), "zz|f", &val, &zFlags, &(var_hash.fci), &(var_hash.fci_cache)) == FAILURE || Z_TYPE_P(zFlags) != IS_LONG) {
                WRONG_PARAM_COUNT;
            }
            convert_to_long_ex(zFlags);
            flags = (int)Z_LVAL_P(zFlags);
            break;
    }
    var_hash.flags = flags;
    
    amf_SERIALIZE_CTOR(var_hash)
    if ((flags & AMF_AMF3) != 0) {
        amf_write_byte(pbuf, AMF0_AMF3);
        amf3_serialize_var(pbuf, val, &var_hash);
    }
    else {
        amf0_serialize_var(pbuf, val, &var_hash);
    }
#ifdef amf_USE_STRING_BUILDER

    amf_serialize_output_get(pbuf, return_value);
    amf_serialize_output_dtor(&buf);

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

/* }}} */



/* Deserialize {{{ ***********************************************************/
/*****************************************************************************/
/*****************************************************************************/

/** reads short integer in AMF0 format */
static int amf_read_short(const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
{
    const unsigned char *cp = *p;
    *p += 2;
    return ((cp[0] << 8) | cp[1]);
}

/** reads integer in AMF0 format */
static int amf_read_int(const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
{
    const unsigned char *cp = *p;
    *p += 4;
    return ((cp[0] << 24) | (cp[1] << 16) | (cp[2] << 8) | cp[3]);
}

/** reads double in AMF0 format, eventually flipping it for bigendian */
static double amf_read_double(const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
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
    }
    else {
        memcpy(d.cval, cp, 8);
    }
    return d.dval;
}

/** reads integer in AMF3 format */
static int amf3_read_uint29(const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
{
    const unsigned char *cp = *p;

    int acc = *cp++;
    int mask, r, tmp;
    if (acc < 128) {
        *p = cp;
        return acc;
    }
    else {
        acc = (acc & 0x7f) << 7;
        tmp = *cp++;
        if (tmp < 128) {
            acc = acc | tmp;
        }
        else {
            acc = (acc | (tmp & 0x7f)) << 7;
            tmp = *cp++;
            if (tmp < 128) {
                acc = acc | tmp;
            }
            else {
                acc = (acc | (tmp & 0x7f)) << 8;
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
    mask = 1 << 28;  /* mask */
    r = -(acc & mask) | acc;
    return r;
}

/**
 * read string in AMF format (is_long: 2=short;4=long)
 */
static int amf0_read_string(zval *rval, const unsigned char **p, const unsigned char *max, int is_long, amf_context_data_t *var_hash)
{
    int len = is_long == 2 ? amf_read_short(p, max, var_hash) : amf_read_int(p, max, var_hash);
    const unsigned char *cp = *p;
    *p += len;
    ZVAL_STRINGL(rval, (char *)cp, len);
    return SUCCESS;
}

/**
 * read string in AMF format of specified length
 */
static int amf_read_string(zval *rval, const unsigned char **p, const unsigned char *max, int len, amf_context_data_t *var_hash)
{
    const unsigned char *cp = *p;
    *p += len;
    ZVAL_STRINGL(rval, (char *)cp, len);
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
static int amf3_read_string(zval *rval, const unsigned char **p, const unsigned char *max, int store_reference, amf_context_data_t *var_hash)
{
    int len = amf3_read_uint29(p, max, var_hash);
    if (len == 1) {
        ZVAL_EMPTY_STRING(rval);
    }
    else if ((len & AMF_INLINE_ENTITY) != 0) {
        const unsigned char *cp = *p;
        len >>= 1;
        *p += len;

        ZVAL_STRINGL(rval, (char *)cp, len);

        if (store_reference == 1) {
            zend_hash_index_update(&(var_hash->strings), zend_hash_num_elements(&(var_hash->strings)), (void *)rval);
            Z_ADDREF_P(rval);
        }
        
    }
    else {
        int result = amf_get_from_cache(&(var_hash->strings), rval, (len >> 1));
        if (result == SUCCESS) {
            Z_ADDREF_P(rval);
        }
        return result;
        
    }
    return SUCCESS;
}

/** read integer in AMF3 vector format */
static int amf3_read_vector_int(const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
{
    union aligned {
        int ival;
        char cval[4];
    } i;
    const unsigned char *cp = *p;
    *p += 4;
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        i.cval[0] = cp[3]; i.cval[1] = cp[2]; i.cval[2] = cp[1]; i.cval[3] = cp[0];
    }
    else {
        memcpy(i.cval, cp, 4);
    }
    return i.ival;
}

/** read unsigned integer in AMF3 vector format */
static unsigned int amf3_read_vector_uint(const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
{
    union aligned {
        unsigned int Ival;
        char cval[4];
    } i;
    const unsigned char *cp = *p;
    *p += 4;
    if ((var_hash->flags & AMF_BIGENDIAN) != 0) {
        i.cval[0] = cp[3]; i.cval[1] = cp[2]; i.cval[2] = cp[1]; i.cval[3] = cp[0];
    }
    else {
        memcpy(i.cval, cp, 4);
    }
    return i.Ival;
}

/** read double in AMF0 format, eventually flipping it for bigendian */
static double amf3_read_vector_double(const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
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
    }
    else {
        memcpy(d.cval, cp, 8);
    }
    return d.dval;
}

static int amf_invoke_deserialize_callback(zval *rval, int evt, zval *arg, int shared, amf_context_data_t *var_hash)
{
    int result;
    zval params[2];

    ZVAL_LONG(&params[0], evt);

    if (arg == NULL) {
        ZVAL_NULL(&params[1]);
    }
    else {
        ZVAL_COPY_VALUE(&params[1], arg);
    }

    var_hash->fci.params = params;
    var_hash->fci.param_count = 2;
    var_hash->fci.no_separation = 0;
    var_hash->fci.retval = rval;

    if ((result = zend_call_function(&(var_hash->fci), &(var_hash->fci_cache))) == FAILURE) {

    }
    else {
       
    }

    zval_ptr_dtor(&params[0]);
    zval_ptr_dtor(&params[1]);

    return result;
}

/**
 * generic deserialization in AMF3 format
 * \param rval a zval already allocated
 */
static int amf3_deserialize_var(zval *rval, const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
{
    const int type = **p;
    int handle;

    *p = *p + 1;
    switch (type) {
        case AMF3_UNDEFINED:
        case AMF3_NULL:
            ZVAL_NULL(rval);
            break;
        case AMF3_FALSE:
            ZVAL_BOOL(rval, 0);
            break;
        case AMF3_TRUE:
            ZVAL_BOOL(rval, 1);
            break;
        case AMF3_INTEGER:
            ZVAL_LONG(rval, amf3_read_uint29(p, max, var_hash));
            break;
        case AMF3_NUMBER:
            ZVAL_DOUBLE(rval, amf_read_double(p, max, var_hash));
            break;
        case AMF3_STRING:
            if (amf3_read_string(rval, p, max, 1, var_hash) == FAILURE) {
                php_error_docref(NULL, E_NOTICE, "amf cannot lookup string");
                return FAILURE;
            }
            break;
        case AMF3_XMLDOCUMENT:
        case AMF3_XML:
        case AMF3_BYTEARRAY:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                int event = type == AMF3_BYTEARRAY ? AMFE_POST_BYTEARRAY : type == AMF3_XML ? AMFE_POST_XML : AMFE_POST_XMLDOCUMENT;
                if (amf_read_string(rval, p, max, handle >> 1, var_hash) == FAILURE) {
                    const char *name = type == AMF3_BYTEARRAY ? "bytearray" : "xmlstring";
                    php_error_docref(NULL, E_NOTICE, "amf cannot read string for %s", name);
                    return FAILURE;
                }
                amf_invoke_deserialize_callback(rval, event, rval, 1, var_hash);
                amf_put_in_cache(&(var_hash->objects), rval);
            }
            else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    const char *objtype = type == AMF3_BYTEARRAY ? "bytearray" : type == AMF3_XML ? "xml" : "xmldocument";
                    php_error_docref(NULL, E_NOTICE, "amf cannot lookup %s for %d", objtype, handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_DATE:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                double d = amf_read_double(p, max, var_hash);
                ZVAL_DOUBLE(rval, d);
                amf_invoke_deserialize_callback(rval, AMFE_POST_DATE, rval, 1, var_hash);
                amf_put_in_cache(&(var_hash->objects), rval);
            }
            else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL, E_NOTICE, "amf cannot lookup date %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_ARRAY:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                int i;
                int max_index = handle >> 1;
                HashTable *output = NULL;
                array_init_size(rval, max_index);
                output = HASH_OF(rval);
                amf_put_in_cache(&(var_hash->objects), rval);

                while (1) {
                    zval key, value;
                    char *end;
                    char tmp[32];
                    int len;
                    int i;

                    ZVAL_NULL(&key);
                    if (amf3_read_string(&key, p, max, 1, var_hash) == FAILURE) {
                        break;
                    }
                    if (Z_STRLEN(key) == 0) {
                        zval_ptr_dtor(&key);
                        break;
                    }
                    ZVAL_NULL(&value);
                    if (amf3_deserialize_var(&value, p, max, var_hash) == FAILURE) {
                        php_error_docref(NULL, E_NOTICE, "amf cannot deserialize key %s", Z_STRVAL(key));
                        zval_ptr_dtor(&key);
                        zval_ptr_dtor(&value);
                        break;
                    }
                    len = (int)Z_STRLEN(key);
                    if (len < sizeof(tmp)) {
                        memcpy(tmp, Z_STRVAL(key), len);
                        tmp[len] = 0;
                        i = strtoul(tmp, &end, 10);
                    }
                    else {
                        i = 0;
                    }

                    /* TODO: test for key as 0 and key as " */
                    if (i != 0 && (end == NULL || *end == 0)) {
                        zend_hash_index_update(output, i, &value);
                    }
                    else {
                        add_assoc_zval(rval, Z_STRVAL(key), &value);
                    }
                    zval_ptr_dtor(&key);
                }

                for (i = 0; i < max_index; i++) {
                    if (**p == AMF3_UNDEFINED) {
                        *p = *p + 1;
                    }
                    else {
                        zval value;
                        ZVAL_NULL(&value);
                        if (amf3_deserialize_var(&value, p, max, var_hash) == FAILURE) {
                            zval_ptr_dtor(&value);
                            php_error_docref(NULL, E_NOTICE, "amf cannot deserialize array item %d", i);
                            return FAILURE;
                        }
                        add_index_zval(rval, i, &value);
                    }
                }
            }
            else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL, E_NOTICE, "amf cannot lookup array %d", handle >> 1);
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
                int iMember;
                int bIsArray = 0;
                int iSuccess = FAILURE;
                zval zClassDef;
                HashTable *htClassDef;
                zval class_name;
                ZVAL_NULL(&class_name);
                
                bInlineclassDef = (handle & AMF_INLINE_CLASS) != 0;

                if (bInlineclassDef == 0) {
                    zval *tmp;

                    int iClassDef = (handle >> AMF_CLASS_SHIFT);
                    if (amf_get_from_cache(&(var_hash->traits), &zClassDef, iClassDef) == FAILURE) {
                        zval_ptr_dtor(&zClassDef);
                        php_error_docref(NULL, E_NOTICE, "amf cannot find class by number %d", iClassDef);
                        return FAILURE;
                    }
                    htClassDef = HASH_OF(&zClassDef);

                    /* extract information from classdef packed into the first element */
                    handle = (int)amf_get_index_long(htClassDef, 0, 0);
                    nClassMemberCount = handle >> AMF_CLASS_MEMBERCOUNT_SHIFT;
                    bTypedObject = (handle & 1) != 0;  /* special */
                    iExternalizable = handle & AMF_CLASS_EXTERNAL;
                    iDynamicObject = handle & AMF_CLASS_DYNAMIC;

                    if ((tmp = zend_hash_index_find(htClassDef, 1)) != NULL) {
                        ZVAL_COPY_VALUE(&class_name, tmp);
                    }
                }
                else {
                    iExternalizable = handle & AMF_CLASS_EXTERNAL;
                    iDynamicObject = handle & AMF_CLASS_DYNAMIC;
                    nClassMemberCount = handle >> AMF_CLASS_MEMBERCOUNT_SHIFT;

                    amf3_read_string(&class_name, p, max, 1, var_hash);
                    bTypedObject = Z_STRLEN(class_name) > 0;

                    /* A classdef is an array containing the reference, the class_name, then list of property names */
                    array_init_size(&zClassDef, nClassMemberCount + 2);
                    add_next_index_long(&zClassDef, (bTypedObject ? 1 : 0) | nClassMemberCount << AMF_CLASS_MEMBERCOUNT_SHIFT | iDynamicObject | iExternalizable);
                    if (add_next_index_zval(&zClassDef, &class_name) == SUCCESS) {
                        if (bTypedObject) {
                            Z_ADDREF(class_name);
                        }
                    }

                    for (iMember = 0; iMember < nClassMemberCount; iMember++) {
                        zval key;
                        ZVAL_NULL(&key);
                        if (amf3_read_string(&key, p, max, 1, var_hash) == FAILURE) {
                            zval_ptr_dtor(&key);
                            break;
                        }
                        if (add_next_index_zval(&zClassDef, &key) == SUCCESS) {
                            Z_ADDREF(key);
                        }
                        zval_ptr_dtor(&key);
                    }

                    amf_put_in_cache(&(var_hash->traits), &zClassDef);
                }

                /* callback for externalizable or class_names not null */
                if (iExternalizable != 0 || (Z_TYPE(class_name) == IS_STRING && Z_STRLEN(class_name) != 0)) {
                    if ((iSuccess = amf_invoke_deserialize_callback(rval, iExternalizable != 0 ? AMFE_MAP_EXTERNALIZABLE : AMFE_MAP, &class_name, 0, var_hash)) == SUCCESS) {
                        if (Z_TYPE_P(rval) == IS_ARRAY) {
                            bIsArray = 1;
                        }
                        else if (Z_TYPE_P(rval) == IS_OBJECT) {
                            bIsArray = 0;
                        }
                        else {
                            php_error_docref(NULL, E_NOTICE, "amf3 deserialize callback must return an array or object");
                            iSuccess = FAILURE; /* nor an object or an array */
                        }
                    }
                }

                if (iExternalizable != 0) {
                    if (iSuccess == FAILURE || Z_TYPE_P(rval) == IS_NULL) {
                        amf_put_in_cache(&(var_hash->objects), NULL);
                        amf3_deserialize_var(rval, p, max, var_hash);
                    }
                    else {
                        amf_put_in_cache(&(var_hash->objects), rval);
                    }
                }
                else {
                    /* default behaviour */
                    if (iSuccess == FAILURE || Z_TYPE_P(rval) == IS_NULL) {
                        if ((var_hash->flags & AMF_OBJECT_AS_ASSOC) != 0) {
                            array_init_size(rval, nClassMemberCount);
                            bIsArray = 1;
                        }
                        else {
                            if (bTypedObject != 0) {
                                zend_class_entry *ce;
                                if ((ce = zend_lookup_class(Z_STR(class_name))) == NULL) {
                                    php_error_docref(NULL, E_NOTICE, "amf cannot find class entry %s", Z_STRVAL(class_name));
                                    object_init(rval);
                                }
                                else {
                                    object_init_ex(rval, ce);
                                }
                            }
                            else {
                                object_init(rval);
                            }
                        }
                    }

                    amf_put_in_cache(&(var_hash->objects), rval);

                    for (iMember = 0; iMember < nClassMemberCount; iMember++) {
                        zval *key, value;
                        if ((key = zend_hash_index_find(Z_ARRVAL(zClassDef), iMember + 2)) == NULL) {
                            php_error_docref(NULL, E_NOTICE, "amf cannot find index for class member %d over %d", iMember, nClassMemberCount);
                            return FAILURE;
                        }
                        ZVAL_NULL(&value);
                        if (amf3_deserialize_var(&value, p, max, var_hash) == FAILURE) {
                            zval_ptr_dtor(&value);
                            php_error_docref(NULL, E_NOTICE, "amf cannot read value for class member");
                            return FAILURE;
                        }
                        if (bIsArray == 1) {
                            add_assoc_zval(rval, Z_STRVAL_P(key), &value);
                        }
                        else {
                            add_property_zval(rval, Z_STRVAL_P(key), &value);
                            zval_ptr_dtor(&value);
                        }
                    }

                    if (iDynamicObject != 0) {
                        while (1) {
                            zval key, value;
                            ZVAL_NULL(&key);
                            if (amf3_read_string(&key, p, max, 1, var_hash) == FAILURE) {
                                zval_ptr_dtor(&key);
                                php_error_docref(NULL, E_NOTICE, "amf cannot understand key name %X", "");
                                break;
                            }
                            if (Z_STRLEN(key) == 0) {
                                zval_ptr_dtor(&key);
                                break;
                            }
                            ZVAL_NULL(&value);
                            if (amf3_deserialize_var(&value, p, max, var_hash) == FAILURE) {
                                php_error_docref(NULL, E_NOTICE, "amf cannot deserialize member %s", Z_STRVAL(key));
                                zval_ptr_dtor(&key);
                                zval_ptr_dtor(&value);
                                return FAILURE;
                            }
                            if (bIsArray == 1) {
                                add_assoc_zval(rval, Z_STRVAL(key), &value);
                            }
                            else {
                                add_property_zval(rval, Z_STRVAL(key), &value);
                                zval_ptr_dtor(&value);
                            }
                            zval_ptr_dtor(&key);
                        }
                    }

                    if (bIsArray == 1) {
                        if (bTypedObject != 0) {
                            add_assoc_zval(rval, "_explicitType", &class_name);
                            Z_ADDREF(class_name);
                        }
                    }
                    else if ((var_hash->flags & AMF_POST_DECODE) != 0) {
                        amf_invoke_deserialize_callback(rval, AMFE_POST_OBJECT, rval, 0, var_hash);
                    }
                }
            }
            else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL, E_NOTICE, "amf cannot lookup object %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_VECTOR_INT:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                const unsigned char *cp = *p;
                int iIndex;
                int vectorLen = handle >> 1;
                zval zvVectorData, zvVectorFixed;

                ZVAL_BOOL(&zvVectorFixed, *cp++);
                *p = cp;

                amf_invoke_deserialize_callback(rval, AMFE_VECTOR_INT, rval, 1, var_hash);
                amf_put_in_cache(&(var_hash->objects), rval);
                
                array_init_size(&zvVectorData, vectorLen);

                for (iIndex = 0; iIndex < vectorLen; iIndex++) {
                    int item = amf3_read_vector_int(p, max, var_hash);
                    zval newval;
                    ZVAL_LONG(&newval, item);
                    add_index_zval(&zvVectorData, iIndex, &newval);
                }
                zend_update_property(Z_OBJCE_P(rval), rval, "data", sizeof("data") - 1, &zvVectorData);
                zval_ptr_dtor(&zvVectorData);
            }
            else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL, E_NOTICE, "amf cannot lookup vector %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_VECTOR_UINT:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                const unsigned char *cp = *p;
                int iIndex;
                int vectorLen = handle >> 1;
                zval zvVectorData, zvVectorFixed;

                ZVAL_BOOL(&zvVectorFixed, *cp++);
                *p = cp;

                amf_invoke_deserialize_callback(rval, AMFE_VECTOR_UINT, rval, 1, var_hash);
                amf_put_in_cache(&(var_hash->objects), rval);

                array_init_size(&zvVectorData, vectorLen);

                for (iIndex = 0; iIndex < vectorLen; iIndex++) {
                    unsigned int item = amf3_read_vector_uint(p, max, var_hash);
                    zval newval;
                    if (item > INT_MAX) {
                        ZVAL_DOUBLE(&newval, item);
                    }
                    else {
                        ZVAL_LONG(&newval, item);
                    }
                    add_index_zval(&zvVectorData, iIndex, &newval);
                }
                zend_update_property(Z_OBJCE_P(rval), rval, "data", sizeof("data") - 1, &zvVectorData);
                zval_ptr_dtor(&zvVectorData);
            }
            else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL, E_NOTICE, "amf cannot lookup vector %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_VECTOR_DOUBLE:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                const unsigned char *cp = *p;
                int iIndex;
                int vectorLen = handle >> 1;
                zval zvVectorData, zvVectorFixed;

                ZVAL_BOOL(&zvVectorFixed, *cp++);
                *p = cp;

                amf_invoke_deserialize_callback(rval, AMFE_VECTOR_DOUBLE, rval, 1, var_hash);
                amf_put_in_cache(&(var_hash->objects), rval);

                array_init_size(&zvVectorData, vectorLen);

                for (iIndex = 0; iIndex < vectorLen; iIndex++) {
                    double item = amf3_read_vector_double(p, max, var_hash);
                    zval newval;
                    ZVAL_DOUBLE(&newval, item);
                    add_index_zval(&zvVectorData, iIndex, &newval);
                }
                zend_update_property(Z_OBJCE_P(rval), rval, "data", sizeof("data") - 1, &zvVectorData);
                zval_ptr_dtor(&zvVectorData);
            }
            else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL, E_NOTICE, "amf cannot lookup vector %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        case AMF3_VECTOR_OBJECT:
            handle = amf3_read_uint29(p, max, var_hash);
            if ((handle & AMF_INLINE_ENTITY) != 0) {
                const unsigned char *cp = *p;
                int iIndex;
                int vectorLen = handle >> 1;
                zval zvVectorData, zvVectorFixed, zvVectorClassName;

                ZVAL_BOOL(&zvVectorFixed, *cp++);
                *p = cp;

                if (amf3_read_string(&zvVectorClassName, p, max, 1, var_hash) == FAILURE) {
                    zval_ptr_dtor(&zvVectorData);
                    zval_ptr_dtor(&zvVectorFixed);
                    zval_ptr_dtor(&zvVectorClassName);
                    php_error_docref(NULL, E_NOTICE, "amf cannot understand vector class_name %X", "");
                    break;
                }

                amf_invoke_deserialize_callback(rval, AMFE_VECTOR_OBJECT, rval, 1, var_hash);
                amf_put_in_cache(&(var_hash->objects), rval);

                array_init_size(&zvVectorData, vectorLen);

                for (iIndex = 0; iIndex < vectorLen; iIndex++) {
                    zval newval;
                    if (amf3_deserialize_var(&newval, p, max, var_hash) == FAILURE) {
                        zval_ptr_dtor(&newval);
                        php_error_docref(NULL, E_NOTICE, "amf cannot deserialize array item %d", iIndex);
                        return FAILURE;
                    }
                    add_index_zval(&zvVectorData, iIndex, &newval);
                }
                zend_update_property(Z_OBJCE_P(rval), rval, "data", sizeof("data") - 1, &zvVectorData);
                zval_ptr_dtor(&zvVectorData);
                zval_ptr_dtor(&zvVectorClassName);
            }
            else {
                if (amf_get_from_cache(&(var_hash->objects), rval, (handle >> 1)) == FAILURE) {
                    php_error_docref(NULL, E_NOTICE, "amf cannot lookup vector %d", handle >> 1);
                    return FAILURE;
                }
                zval_add_ref(rval);
            }
            break;
        default:
            php_error_docref(NULL, E_NOTICE, "amf unknown AMF3 type %d", type);
            return FAILURE;
    }
    return SUCCESS;
}

static int amf0_read_object_data(zval *rval, const unsigned char **p, const unsigned char *max, zval *class_name, int as_array, int max_index, amf_context_data_t *var_hash);

static int amf0_deserialize_var(zval *rval, const unsigned char **p, const unsigned char *max, amf_context_data_t *var_hash)
{
    const unsigned char *cursor = *p;
    int type = *cursor++;
    *p = cursor;

    switch (type) {
        case AMF0_NUMBER:
            ZVAL_DOUBLE(rval, amf_read_double(p, max, var_hash));
            break;
        case AMF0_ENDOBJECT:
            return FAILURE;
        case AMF0_BOOLEAN:
            ZVAL_BOOL(rval, *cursor++);
            *p = cursor;
            break;
        case AMF0_OBJECT:
            /* AMF0 read object: key=value up to AMF0_ENDOBJECT */
            return amf0_read_object_data(rval, p, max, NULL, 0, 0, var_hash);
        case AMF0_DATE: {
            double d = amf_read_double(p, max, var_hash);
            amf_read_short(p, max, var_hash);
            ZVAL_DOUBLE(rval, d);
            amf_invoke_deserialize_callback(rval, AMFE_POST_DATE, rval, 0, var_hash);
        }   break;
        case AMF0_STRING:
            return amf0_read_string(rval, p, max, 2, var_hash);
        case AMF0_NULL:
        case AMF0_UNDEFINED:
            ZVAL_NULL(rval);
            break;
        case AMF0_REFERENCE: {
            int object_index = amf_read_short(p, max, var_hash);
            if (amf_get_from_cache(&(var_hash->objects0), rval, object_index) == FAILURE) {
                php_error_docref(NULL, E_NOTICE, "cannot find object reference %d", object_index);
                return FAILURE;
            }
            zval_add_ref(rval);
        }   break;
        case AMF0_MIXEDARRAY: {
            /* AMF0 Mixed: I(maxindex) then key=value up to AMF0_ENDOBJECT */
            int maxIndex = amf_read_int(p, max, var_hash);
            return amf0_read_object_data(rval, p, max, NULL, 1, maxIndex, var_hash);
        }   break;
        case AMF0_ARRAY: {
            int index;
            int length = amf_read_int(p, max, var_hash);
            array_init_size(rval, length);
            amf_put_in_cache(&(var_hash->objects0), rval);

            for (index = 0; index < length; index++) {
                if (**p == AMF0_UNDEFINED) {
                    *p = *p + 1;
                }
                else {
                    zval value;
                    ZVAL_NULL(&value);
                    if (amf0_deserialize_var(&value, p, max, var_hash) == FAILURE) {
                        php_error_docref(NULL, E_NOTICE, "amf bad deserialized value for array index %d", index);
                        zval_ptr_dtor(&value);
                        return FAILURE;
                    }
                    add_index_zval(rval, index, &value);
                }
            }
        }   break;
        case AMF0_TYPEDOBJECT: {
            zval class_name;
            ZVAL_NULL(&class_name);
            if (amf0_read_string(&class_name, p, max, 2, var_hash) == FAILURE) {
                zval_ptr_dtor(&class_name);
                return FAILURE;
            }
            if (amf0_read_object_data(rval, p, max, &class_name, 0, 0, var_hash) == FAILURE) {
                return FAILURE;
            }
        }   break;
        case AMF0_LONGSTRING:
            return amf0_read_string(rval, p, max, 4, var_hash);
        case AMF0_XML:
            if (amf0_read_string(rval, p, max, 4, var_hash) == FAILURE) {
                return FAILURE;
            }
            amf_invoke_deserialize_callback(rval, AMFE_POST_XML, rval, 0, var_hash);
            break;
        case AMF0_AMF3:
            var_hash->flags |= AMF_AMF3;
            return amf3_deserialize_var(rval, p, max, var_hash);
        case AMF0_MOVIECLIP:
        case AMF0_UNSUPPORTED:
        case AMF0_RECORDSET:
            php_error_docref(NULL, E_NOTICE, "amf unsupported AMF type %d", type);
            return FAILURE;
        default:
            php_error_docref(NULL, E_NOTICE, "amf unknown AMF type %d", type);
            return FAILURE;
    }

    return SUCCESS;
}

static int amf0_read_object_data(zval *rval, const unsigned char **p, const unsigned char *max, zval *class_name, int as_array, int max_index, amf_context_data_t *var_hash)
{
    HashTable *output = NULL;
    int callback_done = 0;

    if (as_array == 0 && class_name != NULL && Z_STRLEN_P(class_name) != 0) {
        if (amf_invoke_deserialize_callback(rval, AMFE_MAP, class_name, 0, var_hash) == SUCCESS) {
            if (Z_TYPE_P(rval) == IS_ARRAY) {
                as_array = 1;
                callback_done = 1;
                output = Z_ARRVAL_P(rval);
            }
            else if (Z_TYPE_P(rval) == IS_OBJECT) {
                callback_done = 1;
                output = Z_OBJPROP_P(rval);
            }
        }
    }

    if (callback_done == 0) {
        if (as_array == 1 || (var_hash->flags & AMF_OBJECT_AS_ASSOC) != 0) {
            array_init_size(rval, max_index);
            as_array = 1;
            output = Z_ARRVAL_P(rval);
        }
        else if (class_name != NULL) {
            /* build the corresponding class */
            zend_class_entry *ce;

            if ((ce = zend_lookup_class(Z_STR_P(class_name))) == NULL) {
                php_error_docref(NULL, E_NOTICE, "amf cannot find class %s\n", Z_STRVAL_P(class_name));
                object_init(rval);
            }
            else {
                object_init_ex(rval, ce);
            }
            output = Z_OBJPROP_P(rval);
        }
        else {
            object_init(rval);
            output = Z_OBJPROP_P(rval);
        }
    }

    amf_put_in_cache(&(var_hash->objects0), rval);

    while (1) {
        zval key, value;
        ZVAL_NULL(&key);
        if (amf0_read_string(&key, p, max, 2, var_hash) == FAILURE) {
            zval_ptr_dtor(&key);
            php_error_docref(NULL, E_NOTICE, "amf cannot read string in array/object");
            return FAILURE;
        }
        if (**p == AMF0_ENDOBJECT) {
            *p = *p + 1;
            zval_ptr_dtor(&key);
            break;
        }
        ZVAL_NULL(&value);
        if (amf0_deserialize_var(&value, p, max, var_hash) == FAILURE) {
            php_error_docref(NULL, E_NOTICE, "amf cannot deserialize key <%s>", Z_STRVAL(key));
            zval_ptr_dtor(&key);
            zval_ptr_dtor(&value);
            return FAILURE;
        }
        if (as_array == 1) {
            /* try to convert the string into a number */
            char *end;
            char tmp[32];
            int len = (int)Z_STRLEN(key);
            int i;
            if (len < sizeof(tmp)) {
                memcpy(tmp, Z_STRVAL(key), len);
                tmp[len] = 0;
                i = strtoul(tmp, &end, 10);
            }
            else {
                i = 0;
            }

            /* TODO test for key as 0 and key as " */
            if (i != 0 && (end == NULL || *end == 0)) {
                zend_hash_index_update(output, i, &value);
            }
            else {
                add_assoc_zval(rval, Z_STRVAL(key), &value);
            }
        }
        else if (Z_STRLEN_P(&key) > 0) {
            add_property_zval(rval, Z_STRVAL(key), &value);
            zval_ptr_dtor(&value);
        }
        else {
            php_error_docref(NULL, E_NOTICE, "amf cannot set empty \"\" property for an object. Use AMF_OBJECT_AS_ASSOC flag");
        }
        zval_ptr_dtor(&key);
    }

    if (Z_TYPE_P(rval) == IS_ARRAY) {
        if (class_name != NULL) {
            add_assoc_zval(rval, "_explicitType", class_name);
            Z_ADDREF_P(class_name);
        }
    }
    else if ((var_hash->flags & AMF_POST_DECODE) != 0) {
        amf_invoke_deserialize_callback(rval, AMFE_POST_OBJECT, rval, 0, var_hash);
    }
    return SUCCESS;
}


PHP_FUNCTION(amf_decode)
{
    amf_context_data_t var_hash;
    zval rval, *zInput = NULL, *zFlags = NULL, *zOffset = NULL;
    int offset = 0;
    int flags = 0;
    
    switch (ZEND_NUM_ARGS()) {
        case 0:
            WRONG_PARAM_COUNT;
            return;
        case 1:
            if (zend_parse_parameters(1, "z", &zInput) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            break;
        case 2:
            if (zend_parse_parameters(2, "zz", &zInput, &zFlags) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            convert_to_long_ex(zFlags);
            flags = (int)Z_LVAL_P(zFlags);
            break;
        default:
            if (zend_parse_parameters((ZEND_NUM_ARGS() > 3 ? 4 : 3), "zzz/|f", &zInput, &zFlags, &zOffset, &(var_hash.fci), &(var_hash.fci_cache)) == FAILURE) {
                WRONG_PARAM_COUNT;
            }
            convert_to_long_ex(zFlags);
            convert_to_long_ex(zOffset);
            flags = (int)Z_LVAL_P(zFlags);
            offset = (int)Z_LVAL_P(zOffset);
            break;
    }
    var_hash.flags = flags;

    if (Z_TYPE_P(zInput) == IS_STRING) {
        const unsigned char *p = (unsigned char *)Z_STRVAL_P(zInput) + offset;
        const unsigned char *p0 = p;
        ZVAL_NULL(&rval);

        if (Z_STRLEN_P(zInput) == 0) {
            RETURN_FALSE;
        }
        amf_DESERIALIZE_CTOR(var_hash);
        if (amf0_deserialize_var(&rval, &p, p + Z_STRLEN_P(zInput) - offset, &var_hash) == FAILURE) {
            amf_DESERIALIZE_DTOR(var_hash);
            php_error_docref(NULL, E_NOTICE, "Error at offset %ld of %d bytes", (long)((char *)p - Z_STRVAL_P(zInput)), Z_STRLEN_P(zInput));
            RETURN_FALSE;
        }
        if (zFlags != NULL) {
            zval_dtor(zFlags);
            ZVAL_LONG(zFlags, var_hash.flags);
        }
        if (zOffset != NULL) {
            zval_dtor(zOffset);
            ZVAL_LONG(zOffset, offset + p - p0);
        }
        amf_DESERIALIZE_DTOR(var_hash);

        *return_value = rval;
    }
    else {
        php_error_docref(NULL, E_NOTICE, "amf_decode requires a string argument");
        RETURN_FALSE;
    }
}

/* }}} */
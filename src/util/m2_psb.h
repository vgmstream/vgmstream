#ifndef _M2_PSB_H_
#define _M2_PSB_H_

#include "../streamfile.h"

/* M2's PSB (Packaged Struct Binary) is binary format similar to JSON with a tree-like structure of
 * string keys = multitype values (objects, arrays, bools, strings, ints, raw data and so on)
 * but better packed (like support of ints of all sizes).
 *
 * It's used to access values in different M2 formats, including audio containers (MSound::SoundArchive)
 * so rather than data accessing by offsets they just use "key" = values.
 */


/* opaque struct */
typedef struct psb_context_t psb_context_t;

/* represents an object in the tree */
typedef struct {
    psb_context_t* ctx;
    void* data;
} psb_node_t;


/* open a PSB */
psb_context_t* psb_init(STREAMFILE* sf);
void psb_close(psb_context_t* ctx);

/* get base root object */
int psb_get_root(psb_context_t* ctx, psb_node_t* p_root);

typedef enum {
    PSB_TYPE_NULL = 0x0,
    PSB_TYPE_BOOL = 0x1,
    PSB_TYPE_INTEGER = 0x2,
    PSB_TYPE_FLOAT = 0x3,
    PSB_TYPE_STRING = 0x4,
    PSB_TYPE_DATA = 0x5, /* possibly "userdata" */
    PSB_TYPE_ARRAY = 0x6,
    PSB_TYPE_OBJECT = 0x7, /* also "table" */
    PSB_TYPE_UNKNOWN = 0x8, /* error */
} psb_type_t;

/* get current type */
psb_type_t psb_node_get_type(const psb_node_t* node);

/* get item count (valid for 'array/object' nodes) */
int psb_node_get_count(const psb_node_t* node);

/* get key string of sub-node N (valid for 'object' node) */
const char* psb_node_get_key(const psb_node_t* node, int index);

/* get sub-node from node at index (valid for 'array/object') */
int psb_node_by_index(const psb_node_t* node, int index, psb_node_t* p_out);

/* get sub-node from node at key (valid for 'object') */
int psb_node_by_key(const psb_node_t* node, const char* key, psb_node_t* p_out);

typedef struct {
    uint32_t offset;
    uint32_t size;
} psb_data_t;

typedef union {
    int bln;
    int32_t num;
    double dbl;
    float flt;
    const char* str;
    int count;
    psb_data_t data;
} psb_result_t;

/* generic result (returns all to 0 on failure) */
psb_result_t psb_node_get_result(psb_node_t* node);

/* helpers */
const char* psb_node_get_string(const psb_node_t* node, const char* key);
float       psb_node_get_float(const psb_node_t* node, const char* key);
int32_t     psb_node_get_integer(const psb_node_t* node, const char* key);
int         psb_node_get_bool(const psb_node_t* node, const char* key);
psb_data_t  psb_node_get_data(const psb_node_t* node, const char* key);
int         psb_node_exists(const psb_node_t* node, const char* key);

/* print in JSON-style (for debugging) */
void psb_print(psb_context_t* ctx);

#endif

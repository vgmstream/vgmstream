#include <string.h>
#include "m2_psb.h"
#include "../util.h"
#include "log.h"

/* Code below roughly follows original m2lib internal API b/c why not. Rather than pre-parsing the tree
 * to struct/memory, seems it re-reads bytes from buf as needed (there might be some compiler optims going on too).
 * Always LE even on X360.
 *
 * Info from: decompiled exes and parts (mainly key decoding) from exm2lib by asmodean (http://asmodean.reverse.net/),
 * also https://github.com/number201724/psbfile and https://github.com/UlyssesWu/FreeMote
 *
 * PSB defines a header with offsets to sections within the header, binary format being type-value (where type could be
 * int8/int16/float/list/object/etc). Example:
 *   21                             // object: root (x2 listN + other data)
 *     0D 04 0D 06,0B,0D,0E         //   list8[4]: key indexes (#0 "id", #1 "spec", #2 "version", #3 "voice"; found in a separate "key names" table)
 *     0D 04 0D 00,02,04,09         //   list8[4]: byte offsets of next 4 items
 *     15 02		                //   #0 string8: string key #2 ("pc")
 *     1E 5C8F823F                  //   #1 float32: 1.02
 *     05 02                        //   #2 int8: 2
 *     21                           //   #3 object
 *       0D 02 0D 02,05             //     list8[2]: key indexes
 *       0D 02 0D 00,02             //     list8[2]: byte offsets
 *       19 00                      //     #0 resource8: resource #0 (offset/size found in a separate "data resource" table)
 *       20                         //     #1 array: loops
 *          0D 02 0D 00,04          //       list8[2]
 *          07 D69107               //       #0 int24
 *          07 31A45C               //       #1 int24
 * 
 * A game would then position on root object (offset in header) and ask for key "version". 
 * M2 lib finds the index for that key (#2), skips to that offset (0x04), reads type float32, returns 1.02
 */
//TODO: add validations on buf over max size (like reading u32 on edge buf[len-1])


/******************************************************************************/
/* DEFS */

#define PSB_VERSION2  2  /* older (x360/ps3) games */
#define PSB_VERSION3  3  /* current games */
#define PSB_MAX_HEADER  0x40000  /* max seen ~0x1000 (alloc'd) */


/* Internal type used in binary data, that defines bytes used to store value.
 * A common optimization is (type - base-1) to convert to used bytes (like NUMBER_16 - 0x04 = 2).
 * Often M2 code seems to ignore max sizes and casts to int32, no concept of signed/unsigned either.
 * Sometimes M2 code converts to external type to do general checks too. */
typedef enum {
    PSB_ITYPE_NONE = 0x0,

    PSB_ITYPE_NULL = 0x1,

    PSB_ITYPE_TRUE = 0x2,
    PSB_ITYPE_FALSE = 0x3,

    PSB_ITYPE_INTEGER_0 = 0x4,
    PSB_ITYPE_INTEGER_8 = 0x5,
    PSB_ITYPE_INTEGER_16 = 0x6,
    PSB_ITYPE_INTEGER_24 = 0x7,
    PSB_ITYPE_INTEGER_32 = 0x8,
    PSB_ITYPE_INTEGER_40 = 0x9, /* assumed, decomp does same as 32b due to int cast (compiler over-optimization?) */
    PSB_ITYPE_INTEGER_48 = 0xA, /* same */
    PSB_ITYPE_INTEGER_56 = 0xB,
    PSB_ITYPE_INTEGER_64 = 0xC,

    PSB_ITYPE_LIST_8  = 0xD,
    PSB_ITYPE_LIST_16 = 0xE,
    PSB_ITYPE_LIST_24 = 0xF,
    PSB_ITYPE_LIST_32 = 0x10,
    PSB_ITYPE_LIST_40 = 0x11, /* assumed, no refs in code (same up to 64) */
    PSB_ITYPE_LIST_48 = 0x12,
    PSB_ITYPE_LIST_56 = 0x13,
    PSB_ITYPE_LIST_64 = 0x14,

    PSB_ITYPE_STRING_8 = 0x15,
    PSB_ITYPE_STRING_16 = 0x16,
    PSB_ITYPE_STRING_24 = 0x17,
    PSB_ITYPE_STRING_32 = 0x18,

    PSB_ITYPE_DATA_8 = 0x19,
    PSB_ITYPE_DATA_16 = 0x1A,
    PSB_ITYPE_DATA_24 = 0x1B,
    PSB_ITYPE_DATA_32 = 0x1C,
    PSB_ITYPE_DATA_40 = 0x22, /* assumed, some refs in code (same up to 64) */
    PSB_ITYPE_DATA_48 = 0x23,
    PSB_ITYPE_DATA_56 = 0x24,
    PSB_ITYPE_DATA_64 = 0x25,

    PSB_ITYPE_FLOAT_0 = 0x1D,
    PSB_ITYPE_FLOAT_32 = 0x1E,
    PSB_ITYPE_DOUBLE_64 = 0x1F,

    PSB_ITYPE_ARRAY = 0x20,
    PSB_ITYPE_OBJECT = 0x21,
} psb_itype_t;


typedef struct {
    int bytes;          /* total bytes (including headers) to skip this list */
    int count;          /* number of entries */
    int esize;          /* size per entry */
    uint8_t* edata;     /* start of entries */
} list_t;

struct psb_context_t {
	uint32_t header_id;
	uint16_t version;
	uint16_t encrypt_value;
	uint32_t encrypt_offset;
	uint32_t keys_offset;

	uint32_t strings_list_offset;
	uint32_t strings_data_offset;
	uint32_t data_offsets_offset;
	uint32_t data_sizes_offset;

	uint32_t data_offset; /* also "resources" */
	uint32_t root_offset; /* initial node */
	uint32_t unknown; /* hash/crc? (v3) */

    /* main buf and derived stuff */
    uint8_t* buf;
    int buf_len;
    list_t strings_list;
    uint8_t* strings_data;
    int strings_data_len;
    list_t data_offsets_list;
    list_t data_sizes_list;

    /* keys buf */
    char* keys;
    int* keys_pos;
    int keys_count;
};

/******************************************************************************/
/* COMMON */

/* output seems to be signed but some of M2 code casts to unsigned, not sure if important for indexes (known cases never get too high) */
static uint32_t item_get_int(int size, uint8_t* buf) {
    switch (size) {
        case 1:
            return get_u8(buf);
        case 2:
            return get_u16le(buf);
        case 3:
            return (get_u16le(buf+0x01) << 8) | get_u8(buf);
            //return get_u24le(buf+0x01);
        case 4:
            return get_u32le(buf);
        default:
            return 0;
    }
}

static int list_get_count(uint8_t* buf) {
    uint8_t itype = buf[0];
    switch (itype) {
        case PSB_ITYPE_LIST_8:
        case PSB_ITYPE_LIST_16:
        case PSB_ITYPE_LIST_24:
        case PSB_ITYPE_LIST_32: {
            int size = itype - PSB_ITYPE_LIST_8 + 1;
            return item_get_int(size, &buf[1]);
        }
        default:
            return 0;
    }
}

static uint32_t list_get_entry(list_t* lst, uint32_t index) {
    uint8_t* buf = &lst->edata[index * lst->esize];
    return item_get_int(lst->esize, buf);
}

static int list_init(list_t* lst, uint8_t* buf) {
    int count_size, count, entry_size;
    uint8_t count_itype, entry_itype;

    /* ex. 0D 04 0D 00,01,02,03 */

    /* get count info (0D + 04) */
    count_itype = buf[0];
    switch (count_itype) {
        case PSB_ITYPE_LIST_8:
        case PSB_ITYPE_LIST_16:
        case PSB_ITYPE_LIST_24:
        case PSB_ITYPE_LIST_32:
            count_size = count_itype - PSB_ITYPE_LIST_8 + 1;
            count = item_get_int(count_size, &buf[1]);
            break;
        default:
            goto fail;
    }

    /* get entry info (0D + 00,01,02,03) */
    entry_itype = buf[1 + count_size];
    switch (entry_itype) {
        case PSB_ITYPE_LIST_8:
        case PSB_ITYPE_LIST_16:
        case PSB_ITYPE_LIST_24:
        case PSB_ITYPE_LIST_32:
            entry_size = entry_itype - PSB_ITYPE_LIST_8 + 1;
            break;
        default:
            goto fail;
    }

    lst->bytes = 1 + count_size + 1 + entry_size * count;
    lst->count = count;
    lst->esize = entry_size;
    lst->edata = &buf[1 + count_size + 1];
    return 1;
fail:
    memset(lst, 0, sizeof(list_t));
    return 0;
}

/* when a function that should modify p_out fails, memset just in case wasn't init and p_out is chained */
static void node_error(psb_node_t* p_out) {
    if (!p_out)
        return;
    p_out->ctx = NULL;
    p_out->data = NULL;
}


/******************************************************************************/
/* INIT */


/* Keys seems to use a kind of linked list where each element points to next and char is encoded
 * with a distance-based metric. Notice it's encoded in reverse order, so it's tuned to save
 * common prefixes (like bgmXXX in big archives). Those aren't that common, and to encode N chars
 * often needs x2/x3 bytes (and it's slower) so it's probably more of a form of obfuscation. */
int decode_key(list_t* kidx1, list_t* kidx2, list_t* kidx3, char* str, int str_len, int index) {
    int i;

    uint32_t entry_point = list_get_entry(kidx3, index);
    uint32_t point = list_get_entry(kidx2, entry_point);

    for (i = 0; i < str_len; i++) {
        uint32_t next = list_get_entry(kidx2, point);
        uint32_t diff = list_get_entry(kidx1, next);
        uint32_t curr = point - diff;

        str[i] = (char)curr;

        point = next;
        if (!point)
            break;
    }

    if (i == str_len) {
        vgm_logi("PSBLIB: truncated key (report)\n");
    }
    else {
        i++;
    }

    str[i] = '\0';
    return i;
}

/* Keys are packed in a particular format (see get_key_string), and M2 code seems to do some unknown
 * pre-parse, so for now do a simple copy to string buf to simplify handling and returning. */
int init_keys(psb_context_t* ctx) {
    list_t kidx1, kidx2, kidx3;
    uint8_t* buf = &ctx->buf[ctx->keys_offset];
    int i, j, pos;
    char key[256]; /* ~50 aren't too uncommon (used in names) */
    int keys_size;


    /* character/diff table */
    if (!list_init(&kidx1, &buf[0]))
        goto fail;
    /* next point table */
    if (!list_init(&kidx2, &buf[kidx1.bytes]))
        goto fail;
    /* entry point table */
    if (!list_init(&kidx3, &buf[kidx1.bytes + kidx2.bytes]))
        goto fail;

    ctx->keys_count = kidx3.count;
    ctx->keys_pos = malloc(sizeof(int) * ctx->keys_count);
    if (!ctx->keys_pos) goto fail;


    /* packed lists are usually *bigger* than final raw strings, but put some extra size just in case */
    keys_size = (kidx1.bytes + kidx2.bytes + kidx3.bytes) * 2;
    ctx->keys = malloc(keys_size);
    if (!ctx->keys) goto fail;

    pos = 0;
    for (i = 0; i < kidx3.count; i++) {
        int key_len = decode_key(&kidx1, &kidx2, &kidx3, key, sizeof(key), i);

        /* could realloc but meh */
        if (pos + key_len > keys_size)
            goto fail;

        /* copy key in reverse (strrev + memcpy C99 only) */
        for (j = 0; j < key_len; j++) {
            ctx->keys[pos + key_len - 1 - j] = key[j];
        }
        ctx->keys[pos + key_len] = '\0';

        ctx->keys_pos[i] = pos;

        pos += key_len + 1;
    }

    return 1;
fail:
    vgm_logi("PSBLIB: failed getting keys\n");
    return 0;
}

psb_context_t* psb_init(STREAMFILE* sf) {
    psb_context_t* ctx;
    uint8_t header[0x2c];
    int bytes;

    ctx = calloc(1, sizeof(psb_context_t));
    if (!ctx) goto fail;

    bytes = read_streamfile(header, 0x00, sizeof(header), sf);
    if (bytes != sizeof(header)) goto fail;

	ctx->header_id = get_u32be(header + 0x00);
	ctx->version = get_u16le(header + 0x04);
	ctx->encrypt_value = get_u32le(header + 0x06);
	ctx->encrypt_offset = get_u32le(header + 0x08);
	ctx->keys_offset = get_u32le(header + 0x0c);

	ctx->strings_list_offset = get_u32le(header + 0x10);
	ctx->strings_data_offset = get_u32le(header + 0x14);
	ctx->data_offsets_offset = get_u32le(header + 0x18);
	ctx->data_sizes_offset = get_u32le(header + 0x1c);

	ctx->data_offset = get_u32le(header + 0x20);
	ctx->root_offset = get_u32le(header + 0x24);
    if (ctx->version >= PSB_VERSION3)
        ctx->unknown = get_u32le(header + 0x28);

    /* some validations, not sure if checked by M2 */
    if (ctx->header_id != get_id32be("PSB\0"))
        goto fail;
    if (ctx->version != PSB_VERSION2 && ctx->version != PSB_VERSION3)
        goto fail;

    /* not seen */
    if (ctx->encrypt_value != 0)
        goto fail;
    /* 0 in some v2 */
    if (ctx->encrypt_offset != 0 && ctx->encrypt_offset != ctx->keys_offset)
        goto fail;

    /* data should be last as it's used to read buf */
    if (ctx->keys_offset >= ctx->data_offset ||
        ctx->strings_list_offset >= ctx->data_offset ||
        ctx->strings_data_offset >= ctx->data_offset ||
        ctx->data_offsets_offset >= ctx->data_offset ||
        ctx->data_sizes_offset >= ctx->data_offset ||
        ctx->root_offset >= ctx->data_offset)
        goto fail;


    /* copy data for easier access */
    ctx->buf_len = (int)ctx->data_offset;
    if (ctx->buf_len < 0 || ctx->buf_len > PSB_MAX_HEADER)
        goto fail;

    ctx->buf = malloc(ctx->buf_len);
    if (!ctx->buf) goto fail;

    bytes = read_streamfile(ctx->buf, 0x00, ctx->buf_len, sf);
    if (bytes != ctx->buf_len) goto fail;


    /* lists pointing to string block */
    if (!list_init(&ctx->strings_list, &ctx->buf[ctx->strings_list_offset]))
        goto fail;

    /* block of plain c-strings (all null-terminated, including last) */
    ctx->strings_data = &ctx->buf[ctx->strings_data_offset];
    ctx->strings_data_len = ctx->data_offsets_offset - ctx->strings_data_offset;
    if (ctx->strings_data_len < 0 || ctx->strings_data[ctx->strings_data_len - 1] != '\0') /* just in case to avoid overruns */
        goto fail;


    /* lists to access resources */
    if (!list_init(&ctx->data_offsets_list, &ctx->buf[ctx->data_offsets_offset]))
        goto fail;
    if (!list_init(&ctx->data_sizes_list, &ctx->buf[ctx->data_sizes_offset]))
        goto fail;


    /* prepare key strings for easier handling */
    if (!init_keys(ctx))
        goto fail;

    return ctx;
fail:
    psb_close(ctx);
    vgm_logi("PSBLIB: init error (report)\n");
    return NULL;
}

void psb_close(psb_context_t* ctx) {
    if (!ctx)
        return;

    free(ctx->keys_pos);
    free(ctx->keys);
    free(ctx->buf);
    free(ctx);
}

int psb_get_root(psb_context_t* ctx, psb_node_t* p_root) {
    if (!ctx || !p_root)
        return 0;
    p_root->ctx = ctx;
    p_root->data = &ctx->buf[ctx->root_offset];

    return 1;
}


/******************************************************************************/
/* NODES */

psb_type_t psb_node_get_type(const psb_node_t* node) {
    uint8_t* buf;
    uint8_t itype;

    if (!node || !node->data)
        goto fail;

    buf = node->data;
    itype = buf[0];
    switch (itype) {
        case PSB_ITYPE_NULL:
            return PSB_TYPE_NULL;

        case PSB_ITYPE_TRUE:
        case PSB_ITYPE_FALSE:
            return PSB_TYPE_BOOL;

        case PSB_ITYPE_INTEGER_0:
        case PSB_ITYPE_INTEGER_8:
        case PSB_ITYPE_INTEGER_16:
        case PSB_ITYPE_INTEGER_24:
        case PSB_ITYPE_INTEGER_32:
        case PSB_ITYPE_INTEGER_40:
        case PSB_ITYPE_INTEGER_48:
        case PSB_ITYPE_INTEGER_56:
        case PSB_ITYPE_INTEGER_64:
            return PSB_TYPE_INTEGER;

        case PSB_ITYPE_STRING_8:
        case PSB_ITYPE_STRING_16:
        case PSB_ITYPE_STRING_24:
        case PSB_ITYPE_STRING_32:
            return PSB_TYPE_STRING;

        case PSB_ITYPE_DATA_8:
        case PSB_ITYPE_DATA_16:
        case PSB_ITYPE_DATA_24:
        case PSB_ITYPE_DATA_32:
        case PSB_ITYPE_DATA_40:
        case PSB_ITYPE_DATA_48:
        case PSB_ITYPE_DATA_56:
        case PSB_ITYPE_DATA_64:
            return PSB_TYPE_DATA;

        case PSB_ITYPE_FLOAT_0:
        case PSB_ITYPE_FLOAT_32:
        case PSB_ITYPE_DOUBLE_64:
            return PSB_TYPE_FLOAT;

        case PSB_ITYPE_ARRAY:
            return PSB_TYPE_ARRAY;

        case PSB_ITYPE_OBJECT:
            return PSB_TYPE_OBJECT;

        /* M2 just aborts for other internal types (like lists) */
        default:
            goto fail;
    }

fail:
    return PSB_TYPE_UNKNOWN;
}

int psb_node_get_count(const psb_node_t* node) {
    uint8_t* buf;

    if (!node || !node->data)
        goto fail;

    buf = node->data;
    switch (buf[0]) {
        case PSB_ITYPE_ARRAY:
        case PSB_ITYPE_OBJECT:
            /* both start with a list, that can be used as count */
            return list_get_count(&buf[1]);
        default:
            return 0;
    }
fail:
    return -1;
}

int psb_node_by_index(const psb_node_t* node, int index, psb_node_t* p_out) {
    uint8_t* buf;

    if (!node || !node->data)
        goto fail;

    buf = node->data;
    switch (buf[0]) {
        case PSB_ITYPE_ARRAY: {
            list_t offsets;
            int skip;

            list_init(&offsets, &buf[1]);
            skip = list_get_entry(&offsets, index);

            p_out->ctx = node->ctx;
            p_out->data = &buf[1 + offsets.bytes + skip];
            return 1;
        }

        case PSB_ITYPE_OBJECT: {
            list_t keys, offsets;
            int skip;

            list_init(&keys, &buf[1]);
            list_init(&offsets, &buf[1 + keys.bytes]);
            skip = list_get_entry(&offsets, index);

            p_out->ctx = node->ctx;
            p_out->data = &buf[1 + keys.bytes + offsets.bytes + skip];
            return 1;
        }

        default:
            goto fail;
    }
fail:
    vgm_logi("PSBLIB: cannot get node at index %i\n", index);
    node_error(p_out);
    return 0;
}


int psb_node_by_key(const psb_node_t* node, const char* key, psb_node_t* p_out) {
    int i;
    int max;

    if (!node || !node->ctx)
        goto fail;

    max = psb_node_get_count(node);
    if (max < 0 || max > node->ctx->keys_count)
        goto fail;

    for (i = 0; i < max; i++) {
        const char* key_test = psb_node_get_key(node, i);
        if (!key_test)
            goto fail;

        //todo could improve by getting strlen(key) + ctx->key_len + check + strncmp
        if (strcmp(key_test, key) == 0)
            return psb_node_by_index(node, i, p_out);
    }

fail:
    //VGM_LOG("psblib: cannot get node at key '%s'\n", key); /* not uncommon to query */
    node_error(p_out);
    return 0;
}


const char* psb_node_get_key(const psb_node_t* node, int index) {
    uint8_t* buf;
    int pos;

    if (!node || !node->ctx || !node->data)
        goto fail;

    buf = node->data;
    switch (buf[0]) {
        case PSB_ITYPE_OBJECT: {
            list_t keys;
            int keys_index;

            list_init(&keys, &buf[1]);
            keys_index = list_get_entry(&keys, index);
            if (keys_index < 0 || keys_index > node->ctx->keys_count)
                goto fail;

            pos = node->ctx->keys_pos[keys_index];
            return &node->ctx->keys[pos];
        }

        default:
            goto fail;
    }

fail:
    vgm_logi("PSBLIB: cannot get key at index '%i'\n", index);
    return NULL;
}


psb_result_t psb_node_get_result(psb_node_t* node) {
    uint8_t* buf;
    uint8_t itype;
    psb_result_t res = {0};
    int size, index, skip;

    if (!node || !node->ctx || !node->data)
        goto fail;

    buf = node->data;
    itype = buf[0];
    switch (itype) {
        case PSB_ITYPE_NULL:
            break;

        case PSB_ITYPE_TRUE:
        case PSB_ITYPE_FALSE:
            res.bln = (itype == PSB_ITYPE_TRUE);
            break;

        case PSB_ITYPE_INTEGER_0:
            res.num = 0;
            break;

        case PSB_ITYPE_INTEGER_8:
        case PSB_ITYPE_INTEGER_16:
        case PSB_ITYPE_INTEGER_24:
        case PSB_ITYPE_INTEGER_32:
            size = itype - PSB_ITYPE_INTEGER_8 + 1;

            res.num = item_get_int(size, &buf[1]);
            break;

        case PSB_ITYPE_INTEGER_40:
        case PSB_ITYPE_INTEGER_48:
        case PSB_ITYPE_INTEGER_56:
        case PSB_ITYPE_INTEGER_64:
            vgm_logi("PSBLIB: not implemented (report)\n");
            break;

        case PSB_ITYPE_STRING_8:
        case PSB_ITYPE_STRING_16:
        case PSB_ITYPE_STRING_24:
        case PSB_ITYPE_STRING_32: {
            size = itype - PSB_ITYPE_STRING_8 + 1;
            index = item_get_int(size, &buf[1]);
            skip = list_get_entry(&node->ctx->strings_list, index);

            res.str = (const char*)&node->ctx->strings_data[skip]; /* null-terminated (validated on open) */
            if (skip >= node->ctx->strings_data_len) { /* shouldn't happen */
                vgm_logi("PSBLIB: bad skip over strings\n");
                res.str = "";
            }
            break;
        }

        case PSB_ITYPE_DATA_8:
        case PSB_ITYPE_DATA_16:
        case PSB_ITYPE_DATA_24:
        case PSB_ITYPE_DATA_32:
            size = itype - PSB_ITYPE_DATA_8 + 1;
            index = item_get_int(size, &buf[1]);

            res.data.offset = list_get_entry(&node->ctx->data_offsets_list, index);
            res.data.size = list_get_entry(&node->ctx->data_sizes_list, index);

            res.data.offset += node->ctx->data_offset;
            break;

        case PSB_ITYPE_DATA_40:
        case PSB_ITYPE_DATA_48:
        case PSB_ITYPE_DATA_56:
        case PSB_ITYPE_DATA_64:
            vgm_logi("PSBLIB: not implemented (report)\n");
            break;

        case PSB_ITYPE_FLOAT_0:
            res.flt = 0.0f;
            break;

        case PSB_ITYPE_FLOAT_32:
            res.flt = get_f32le(&buf[1]);
            break;

        case PSB_ITYPE_DOUBLE_64:
            res.dbl = get_d64le(&buf[1]);
            res.flt = (float)res.dbl; /* doubles seem ignored */
            break;

        case PSB_ITYPE_ARRAY:
        case PSB_ITYPE_OBJECT:
            res.count = list_get_count(&buf[1]);
            break;

        default:
            goto fail;
    }

    return res;
fail:
    return res; /* should be all null */

}

/******************************************************************************/
/* HELPERS */

static int get_expected_node(const psb_node_t* node, const char* key, psb_node_t* p_out, psb_type_t expected) {
    if (!psb_node_by_key(node, key, p_out))
        goto fail;
    if (psb_node_get_type(p_out) != expected)
        goto fail;
    return 1;
fail:
    return 0;
}


/* M2 coerces values (like float to bool) but it's kinda messy so whatevs */
const char* psb_node_get_string(const psb_node_t* node, const char* key) {
    psb_node_t out;
    if (!get_expected_node(node, key, &out, PSB_TYPE_STRING))
        return NULL;
    return psb_node_get_result(&out).str;
}

float psb_node_get_float(const psb_node_t* node, const char* key) {
    psb_node_t out;
    if (!get_expected_node(node, key, &out, PSB_TYPE_FLOAT))
        return 0.0f;
    return psb_node_get_result(&out).flt;
}

int32_t psb_node_get_integer(const psb_node_t* node, const char* key) {
    psb_node_t out;
    if (!get_expected_node(node, key, &out, PSB_TYPE_INTEGER))
        return 0;
    return psb_node_get_result(&out).num;
}

int psb_node_get_bool(const psb_node_t* node, const char* key) {
    psb_node_t out;
    if (!get_expected_node(node, key, &out, PSB_TYPE_BOOL))
        return 0;
    return psb_node_get_result(&out).bln;
}

psb_data_t  psb_node_get_data(const psb_node_t* node, const char* key) {
    psb_node_t out;
    if (!get_expected_node(node, key, &out, PSB_TYPE_DATA)) {
        psb_data_t data = {0};
        return data;
    }
    return psb_node_get_result(&out).data;
}

int psb_node_exists(const psb_node_t* node, const char* key) {
    psb_node_t out;
    if (!psb_node_by_key(node, key, &out))
        return 0;
    return 1;
}


/******************************************************************************/
/* ETC */

#define PSB_DEPTH_STEP 2

static void print_internal(psb_node_t* curr, int depth) {
    int i;
    psb_node_t node = { 0 };
    const char* key;
    psb_type_t type;
    psb_result_t res;

    if (!curr)
        return;

    type = psb_node_get_type(curr);
    res = psb_node_get_result(curr);
    switch (type) {
        case PSB_TYPE_NULL:
            printf("%s,\n", "null");
            break;

        case PSB_TYPE_BOOL:
            printf("%s,\n", (res.bln == 1 ? "true" : "false"));
            break;

        case PSB_TYPE_INTEGER:
            printf("%i,\n", res.num);
            break;

        case PSB_TYPE_FLOAT:
            printf("%f,\n", res.flt);
            break;

        case PSB_TYPE_STRING:
            printf("\"%s\",\n", res.str);
            break;

        case PSB_TYPE_DATA:
            printf("<0x%08x,0x%08x>\n", res.data.offset, res.data.size);
            break;

        case PSB_TYPE_ARRAY:
            printf("[\n");

            for (i = 0; i < res.count; i++) {
                psb_node_by_index(curr, i, &node);

                printf("%*s", depth + PSB_DEPTH_STEP, "");
                print_internal(&node, depth + PSB_DEPTH_STEP);
            }

            printf("%*s],\n", depth, "");
            break;

        case PSB_TYPE_OBJECT:
            printf("{\n");

            for (i = 0; i < res.count; i++) {
                key = psb_node_get_key(curr, i);
                psb_node_by_index(curr, i, &node);

                printf("%*s\"%s\": ", depth + PSB_DEPTH_STEP, "", key);
                print_internal(&node, depth + PSB_DEPTH_STEP);
            }

            printf("%*s},\n", depth, "");
            break;

        default:
            printf("???,\n");
            break;
    }
}

void psb_print(psb_context_t* ctx) {
    psb_node_t node = { 0 };

    psb_get_root(ctx, &node);
    print_internal(&node, 0);
}

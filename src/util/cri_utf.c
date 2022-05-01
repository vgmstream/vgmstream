#include "cri_utf.h"
#include "log.h"

#define UTF_MAX_SCHEMA_SIZE       0x8000    /* arbitrary max */
#define COLUMN_BITMASK_FLAG       0xf0
#define COLUMN_BITMASK_TYPE       0x0f

enum columna_flag_t {
    COLUMN_FLAG_NAME            = 0x10,     /* column has name (may be empty) */
    COLUMN_FLAG_DEFAULT         = 0x20,     /* data is found relative to schema start (typically constant value for all rows) */
    COLUMN_FLAG_ROW             = 0x40,     /* data is found relative to row start */
    COLUMN_FLAG_UNDEFINED       = 0x80      /* shouldn't exist */
};

enum column_type_t {
    COLUMN_TYPE_UINT8           = 0x00,
    COLUMN_TYPE_SINT8           = 0x01,
    COLUMN_TYPE_UINT16          = 0x02,
    COLUMN_TYPE_SINT16          = 0x03,
    COLUMN_TYPE_UINT32          = 0x04,
    COLUMN_TYPE_SINT32          = 0x05,
    COLUMN_TYPE_UINT64          = 0x06,
    COLUMN_TYPE_SINT64          = 0x07,
    COLUMN_TYPE_FLOAT           = 0x08,
    COLUMN_TYPE_DOUBLE          = 0x09,
    COLUMN_TYPE_STRING          = 0x0a,
    COLUMN_TYPE_VLDATA          = 0x0b,
    COLUMN_TYPE_UINT128         = 0x0c, /* for GUIDs */
    COLUMN_TYPE_UNDEFINED       = -1
};

struct utf_context {
    STREAMFILE* sf;
    uint32_t table_offset;

    /* header */
    uint32_t table_size;
    uint16_t version;
    uint16_t rows_offset;
    uint32_t strings_offset;
    uint32_t data_offset;
    uint32_t name_offset;
    uint16_t columns;
    uint16_t row_width;
    uint32_t rows;

    uint8_t* schema_buf;
    struct utf_column_t {
        uint8_t flag;
        uint8_t type;
        const char* name;
        uint32_t offset;
    } *schema;

    /* derived */
    uint32_t schema_offset;
    uint32_t schema_size;
    uint32_t rows_size;
    uint32_t data_size;
    uint32_t strings_size;
    char* string_table;
    const char* table_name;
};


/* @UTF table context creation */
utf_context* utf_open(STREAMFILE* sf, uint32_t table_offset, int* p_rows, const char** p_row_name) {
    utf_context* utf = NULL;
    uint8_t buf[0x20];
    int bytes;

    utf = calloc(1, sizeof(utf_context));
    if (!utf) goto fail;

    utf->sf = sf;
    utf->table_offset = table_offset;

    bytes = read_streamfile(buf, table_offset, sizeof(buf), sf);
    if (bytes != sizeof(buf)) goto fail;

    /* load table header */
    if (get_u32be(buf + 0x00) != get_id32be("@UTF"))
        goto fail;
    utf->table_size     = get_u32be(buf + 0x04) + 0x08;
    utf->version        = get_u16be(buf + 0x08);
    utf->rows_offset    = get_u16be(buf + 0x0a) + 0x08;
    utf->strings_offset = get_u32be(buf + 0x0c) + 0x08;
    utf->data_offset    = get_u32be(buf + 0x10) + 0x08;
    utf->name_offset    = get_u32be(buf + 0x14); /* within string table */
    utf->columns        = get_u16be(buf + 0x18);
    utf->row_width      = get_u16be(buf + 0x1a);
    utf->rows           = get_u32be(buf + 0x1c);

    utf->schema_offset  = 0x20;
    utf->schema_size    = utf->rows_offset - utf->schema_offset;
    utf->rows_size      = utf->strings_offset - utf->rows_offset;
    utf->strings_size   = utf->data_offset - utf->strings_offset;
    utf->data_size      = utf->table_size - utf->data_offset;


    /* 00: early (32b rows_offset?), 01: +2017 (no apparent differences) */
    if (utf->version != 0x00 && utf->version != 0x01) {
        vgm_logi("@UTF: unknown version\n");
        goto fail;
    }
    if (utf->table_offset + utf->table_size > get_streamfile_size(sf))
        goto fail;
    if (utf->rows_offset > utf->table_size || utf->strings_offset > utf->table_size || utf->data_offset > utf->table_size)
        goto fail;
    if (utf->strings_size <= 0 || utf->name_offset > utf->strings_size)
        goto fail;
    /* no rows is possible for empty tables (have schema and columns names but no data) [PES 2013 (PC)] */
    if (utf->columns <= 0 /*|| utf->rows <= 0 || utf->rows_width <= 0*/)
        goto fail;
    if (utf->schema_size >= UTF_MAX_SCHEMA_SIZE)
        goto fail;

    /* load sections linearly (to optimize stream) */
    {
        /* schema section: small so keep it around (useful to avoid re-reads on column values) */
        utf->schema_buf = malloc(utf->schema_size);
        if (!utf->schema_buf) goto fail;

        bytes = read_streamfile(utf->schema_buf, utf->table_offset + utf->schema_offset, utf->schema_size, sf);
        if (bytes != utf->schema_size) goto fail;

        /* row section: skip, mid to big (0x10000~0x50000) so not preloaded for now */

        /* string section: low to mid size but used to return c-strings */
        utf->string_table = calloc(utf->strings_size + 1, sizeof(char));
        if (!utf->string_table) goto fail;

        bytes = read_streamfile((unsigned char*)utf->string_table, utf->table_offset + utf->strings_offset, utf->strings_size, sf);
        if (bytes != utf->strings_size) goto fail;

        /* data section: skip (may be big with memory AWB) */
    }

    /* load column schema */
    {
        int i;
        uint32_t value_size, column_offset = 0;
        int schema_pos = 0;

        utf->table_name = utf->string_table + utf->name_offset;

        utf->schema = malloc(utf->columns * sizeof(struct utf_column_t));
        if (!utf->schema) goto fail;

        for (i = 0; i < utf->columns; i++) {
            uint8_t info = get_u8(utf->schema_buf + schema_pos + 0x00);
            uint32_t name_offset = get_u32be(utf->schema_buf + schema_pos + 0x01);

            if (name_offset > utf->strings_size)
                goto fail;
            schema_pos += 0x01 + 0x04;

            utf->schema[i].flag = info & COLUMN_BITMASK_FLAG;
            utf->schema[i].type = info & COLUMN_BITMASK_TYPE;
            utf->schema[i].name = NULL;
            utf->schema[i].offset = 0;

            /* known flags are name+default or name+row, but name+default+row is mentioned in VGMToolbox
             * even though isn't possible in CRI's craft utils (meaningless), and no name is apparently possible */
            if ( (utf->schema[i].flag == 0) ||
                !(utf->schema[i].flag & COLUMN_FLAG_NAME) ||
                ((utf->schema[i].flag & COLUMN_FLAG_DEFAULT) && (utf->schema[i].flag & COLUMN_FLAG_ROW)) ||
                 (utf->schema[i].flag & COLUMN_FLAG_UNDEFINED) ) {
                vgm_logi("@UTF: unknown column flag combo found\n");
                goto fail;
            }

            switch (utf->schema[i].type) {
                case COLUMN_TYPE_UINT8:
                case COLUMN_TYPE_SINT8:
                    value_size = 0x01;
                    break;
                case COLUMN_TYPE_UINT16:
                case COLUMN_TYPE_SINT16:
                    value_size = 0x02;
                    break;
                case COLUMN_TYPE_UINT32:
                case COLUMN_TYPE_SINT32:
                case COLUMN_TYPE_FLOAT:
                case COLUMN_TYPE_STRING:
                    value_size = 0x04;
                    break;
                case COLUMN_TYPE_UINT64:
                case COLUMN_TYPE_SINT64:
              //case COLUMN_TYPE_DOUBLE:
                case COLUMN_TYPE_VLDATA:
                    value_size = 0x08;
                    break;
              //case COLUMN_TYPE_UINT128:
              //    value_size = 0x16;
                default:
                    vgm_logi("@UTF: unknown column type\n");
                    goto fail;
            }

            if (utf->schema[i].flag & COLUMN_FLAG_NAME) {
                utf->schema[i].name = utf->string_table + name_offset;
            }

            if (utf->schema[i].flag & COLUMN_FLAG_DEFAULT) {
                utf->schema[i].offset = schema_pos;
                schema_pos += value_size;
            }

            if (utf->schema[i].flag & COLUMN_FLAG_ROW) {
                utf->schema[i].offset = column_offset;
                column_offset += value_size;
            }
        }
    }

#if 0
    VGM_LOG("- %s\n", utf->table_name);
    VGM_LOG("utf_o=%08x (%x)\n", utf->table_offset, utf->table_size);
    VGM_LOG(" sch_o=%08x (%x), c=%i\n", utf->table_offset + utf->schema_offset, utf->schema_size, utf->columns);
    VGM_LOG(" row_o=%08x (%x), r=%i\n", utf->table_offset + utf->rows_offset, utf->rows_size, utf->rows);
    VGM_LOG(" str_o=%08x (%x)\n", utf->table_offset + utf->strings_offset, utf->strings_size);
    VGM_LOG(" dat_o=%08x (%x))\n", utf->table_offset + utf->data_offset, utf->data_size);
#endif

    /* write info */
    if (p_rows) *p_rows = utf->rows;
    if (p_row_name) *p_row_name = utf->string_table + utf->name_offset;

    return utf;
fail:
    utf_close(utf);
    vgm_logi("@UTF: init failure\n");
    return NULL;
}

void utf_close(utf_context* utf) {
    if (!utf) return;

    free(utf->string_table);
    free(utf->schema_buf);
    free(utf->schema);
    free(utf);
}


int utf_get_column(utf_context* utf, const char* column) {
    int i;

    /* find target column */
    for (i = 0; i < utf->columns; i++) {
        struct utf_column_t* col = &utf->schema[i];

        if (col->name == NULL || strcmp(col->name, column) != 0)
            continue;
        return i;
    }

    return -1;
}

typedef struct {
    enum column_type_t type;
    union {
        int8_t   s8;
        uint8_t  u8;
        int16_t  s16;
        uint16_t u16;
        int32_t  s32;
        uint32_t u32;
        int64_t  s64;
        uint64_t u64;
        float    flt;
        double   dbl;
        struct utf_data_t {
            uint32_t offset;
            uint32_t size;
        } data;
#if 0
      struct utf_u128_t {
          uint64_t hi;
          uint64_t lo;
      } value_u128;
#endif
        const char* str;
    } value;
} utf_result_t;

static int utf_query(utf_context* utf, int row, int column, utf_result_t* result) {

    if (row >= utf->rows || row < 0)
        goto fail;
    if (column >= utf->columns || column < 0)
        goto fail;

    /* get target column */
    {
        struct utf_column_t* col = &utf->schema[column];
        uint32_t data_offset = 0;
        uint8_t* buf = NULL;

        result->type = col->type;

        if (col->flag & COLUMN_FLAG_DEFAULT) {
            if (utf->schema_buf)
                buf = utf->schema_buf + col->offset;
            else
                data_offset = utf->table_offset + utf->schema_offset + col->offset;
        }
        else if (col->flag & COLUMN_FLAG_ROW) {
            data_offset = utf->table_offset + utf->rows_offset + row * utf->row_width + col->offset;
        }
        else {
            /* shouldn't happen */
            memset(&result->value, 0, sizeof(result->value));
            return 1; /* ??? */
        }


        /* read row/constant value (use buf if available) */
        switch (col->type) {
            case COLUMN_TYPE_UINT8:
                result->value.u8 = buf ? get_u8(buf) : read_u8(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_SINT8:
                result->value.s8 = buf ? get_s8(buf) : read_s8(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_UINT16:
                result->value.u16 = buf ? get_u16be(buf) : read_u16be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_SINT16:
                result->value.s16 = buf ? get_s16be(buf) : read_s16be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_UINT32:
                result->value.u32 = buf ? get_u32be(buf) : read_u32be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_SINT32:
                result->value.s32 = buf ? get_s32be(buf) : read_s32be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_UINT64:
                result->value.u64 = buf ? get_u64be(buf) : read_u64be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_SINT64:
                result->value.s64 = buf ? get_s64be(buf) : read_s64be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_FLOAT:
                result->value.flt = buf ? get_f32be(buf) : read_f32be(data_offset, utf->sf);
                break;
#if 0
            case COLUMN_TYPE_DOUBLE:
                result->value.dbl = buf ? get_d64be(buf) : read_d64be(data_offset, utf->sf);
                break;
#endif
            case COLUMN_TYPE_STRING: {
                uint32_t name_offset = buf ? get_u32be(buf) : read_u32be(data_offset, utf->sf);
                if (name_offset > utf->strings_size)
                    goto fail;
                result->value.str = utf->string_table + name_offset;
                break;
            }
            case COLUMN_TYPE_VLDATA:
                result->value.data.offset = buf ? get_u32be(buf + 0x0) : read_u32be(data_offset + 0x00, utf->sf);
                result->value.data.size   = buf ? get_u32be(buf + 0x4) : read_u32be(data_offset + 0x04, utf->sf);
                break;
#if 0
            case COLUMN_TYPE_UINT128:
                result->value.value_u128.hi = buf ? get_u32be(buf + 0x0) : read_u64be(data_offset + 0x00, utf->sf);
                result->value.value_u128.lo = buf ? get_u32be(buf + 0x4) : read_u64be(data_offset + 0x08, utf->sf);
                break;
#endif
            default:
                goto fail;
        }
    }

    return 1;
fail:
    return 0;
}

static int utf_query_value(utf_context* utf, int row, int column, void* value, enum column_type_t type) {
    utf_result_t result = {0};
    int valid;

    valid = utf_query(utf, row, column, &result);
    if (!valid || result.type != type)
        return 0;

    switch(result.type) {
        case COLUMN_TYPE_UINT8:  (*(uint8_t*)value)  = result.value.u8; break;
        case COLUMN_TYPE_SINT8:  (*(int8_t*)value)   = result.value.s8; break;
        case COLUMN_TYPE_UINT16: (*(uint16_t*)value) = result.value.u16; break;
        case COLUMN_TYPE_SINT16: (*(int16_t*)value)  = result.value.s16; break;
        case COLUMN_TYPE_UINT32: (*(uint32_t*)value) = result.value.u32; break;
        case COLUMN_TYPE_SINT32: (*(int32_t*)value)  = result.value.s32; break;
        case COLUMN_TYPE_UINT64: (*(uint64_t*)value) = result.value.u64; break;
        case COLUMN_TYPE_SINT64: (*(int64_t*)value)  = result.value.s64; break;
        case COLUMN_TYPE_STRING: (*(const char**)value) = result.value.str; break;
        default:
            return 0;
    }

    return 1;
}

int utf_query_col_s8(utf_context* utf, int row, int column, int8_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_SINT8);
}
int utf_query_col_u8(utf_context* utf, int row, int column, uint8_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_UINT8);
}
int utf_query_col_s16(utf_context* utf, int row, int column, int16_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_SINT16);
}
int utf_query_col_u16(utf_context* utf, int row, int column, uint16_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_UINT16);
}
int utf_query_col_s32(utf_context* utf, int row, int column, int32_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_SINT32);
}
int utf_query_col_u32(utf_context* utf, int row, int column, uint32_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_UINT32);
}
int utf_query_col_s64(utf_context* utf, int row, int column, int64_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_SINT64);
}
int utf_query_col_u64(utf_context* utf, int row, int column, uint64_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_UINT64);
}
int utf_query_col_string(utf_context* utf, int row, int column, const char** value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_STRING);
}

int utf_query_col_data(utf_context* utf, int row, int column, uint32_t* p_offset, uint32_t* p_size) {
    utf_result_t result = {0};
    int valid;

    valid = utf_query(utf, row, column, &result);
    if (!valid || result.type != COLUMN_TYPE_VLDATA)
        return 0;

    if (p_offset) *p_offset = utf->table_offset + utf->data_offset + result.value.data.offset;
    if (p_size) *p_size = result.value.data.size;
    return 1;
}


int utf_query_s8(utf_context* utf, int row, const char* column_name, int8_t* value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_SINT8);
}
int utf_query_u8(utf_context* utf, int row, const char* column_name, uint8_t* value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_UINT8);
}
int utf_query_s16(utf_context* utf, int row, const char* column_name, int16_t* value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_SINT16);
}
int utf_query_u16(utf_context* utf, int row, const char* column_name, uint16_t* value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_UINT16);
}
int utf_query_s32(utf_context* utf, int row, const char* column_name, int32_t* value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_SINT32);
}
int utf_query_u32(utf_context* utf, int row, const char* column_name, uint32_t* value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_UINT32);
}
int utf_query_s64(utf_context* utf, int row, const char* column_name, int64_t* value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_SINT64);
}
int utf_query_u64(utf_context* utf, int row, const char* column_name, uint64_t* value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_UINT64);
}
int utf_query_string(utf_context* utf, int row, const char* column_name, const char** value) {
    return utf_query_value(utf, row, utf_get_column(utf, column_name), (void*)value, COLUMN_TYPE_STRING);
}

int utf_query_data(utf_context* utf, int row, const char* column_name, uint32_t* p_offset, uint32_t* p_size) {
    return utf_query_col_data(utf, row, utf_get_column(utf, column_name), p_offset, p_size);
}

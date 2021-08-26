#include "cri_utf.h"
#include "../util/log.h"

#define COLUMN_BITMASK_FLAG       0xf0
#define COLUMN_BITMASK_TYPE       0x0f

enum columna_flag_t {
	COLUMN_FLAG_NAME            = 0x10,
	COLUMN_FLAG_DEFAULT         = 0x20,
	COLUMN_FLAG_ROW             = 0x40,
	COLUMN_FLAG_UNDEFINED       = 0x80 /* shouldn't exist */
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

typedef struct {
    int found;
    enum column_type_t type;
    union {
        int8_t   value_s8;
        uint8_t  value_u8;
        int16_t  value_s16;
        uint16_t value_u16;
        int32_t  value_s32;
        uint32_t value_u32;
        int64_t  value_s64;
        uint64_t value_u64;
        float    value_float;
        double   value_double;
        struct utf_data_t {
            uint32_t offset;
            uint32_t size;
        } value_data;
      //struct utf_u128_t {
      //    uint64_t hi;
      //    uint64_t lo;
      //} value_u128;
        const char *value_string;
    } value;
} utf_result_t;

struct utf_context {
    STREAMFILE *sf;
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
    struct utf_column_t {
        uint8_t flag;
        uint8_t type;
        const char *name;
        uint32_t offset;
    } *schema;

    /* derived */
    uint32_t schema_offset;
    uint32_t strings_size;
    char *string_table;
    const char *table_name;
};


/* @UTF table context creation */
utf_context* utf_open(STREAMFILE* sf, uint32_t table_offset, int* p_rows, const char** p_row_name) {
    utf_context* utf = NULL;


    utf = calloc(1, sizeof(utf_context));
    if (!utf) goto fail;

    utf->sf = sf;
    utf->table_offset = table_offset;

    /* check header */
    if (read_u32be(table_offset + 0x00, sf) != 0x40555446) /* "@UTF" */
        goto fail;

    /* load table header */
    utf->table_size      = read_u32be(table_offset + 0x04, sf) + 0x08;
    utf->version         = read_u16be(table_offset + 0x08, sf);
    utf->rows_offset     = read_u16be(table_offset + 0x0a, sf) + 0x08;
    utf->strings_offset  = read_u32be(table_offset + 0x0c, sf) + 0x08;
    utf->data_offset     = read_u32be(table_offset + 0x10, sf) + 0x08;
    utf->name_offset     = read_u32be(table_offset + 0x14, sf); /* within string table */
    utf->columns         = read_u16be(table_offset + 0x18, sf);
    utf->row_width       = read_u16be(table_offset + 0x1a, sf);
    utf->rows            = read_u32be(table_offset + 0x1c, sf);

    utf->schema_offset   = 0x20;
    utf->strings_size    = utf->data_offset - utf->strings_offset;

    /* 00: early (32b rows_offset?), 01: +2017 (no apparent differences) */
    if (utf->version != 0x00 && utf->version != 0x01) {
        vgm_logi("@UTF: unknown version\n");
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


    /* load string table */
    {
        size_t read;

        utf->string_table = calloc(utf->strings_size + 1, sizeof(char));
        if (!utf->string_table) goto fail;

        utf->table_name = utf->string_table + utf->name_offset;

        read = read_streamfile((unsigned char*)utf->string_table, utf->table_offset + utf->strings_offset, utf->strings_size, sf);
        if (utf->strings_size != read) goto fail;
    }


    /* load column schema */
    {
        int i;
        uint32_t value_size, column_offset = 0;
        uint32_t schema_offset = utf->table_offset + utf->schema_offset;


        utf->schema = malloc(sizeof(struct utf_column_t) * utf->columns);
        if (!utf->schema) goto fail;

        for (i = 0; i < utf->columns; i++) {
            uint8_t info = read_u8(schema_offset + 0x00, sf);
            uint32_t name_offset = read_u32be(schema_offset + 0x01, sf);
            if (name_offset > utf->strings_size)
                goto fail;
            schema_offset += 0x01 + 0x04;


            utf->schema[i].flag = info & COLUMN_BITMASK_FLAG;
            utf->schema[i].type = info & COLUMN_BITMASK_TYPE;
            utf->schema[i].name = NULL;
            utf->schema[i].offset = 0;

            /* known flags are name+default or name+row, but name+default+row is mentioned in VGMToolbox
             * even though isn't possible in CRI's craft utils, and no name is apparently possible */
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
                /* data is found relative to schema start */
                utf->schema[i].offset = schema_offset - (utf->table_offset + utf->schema_offset);
                schema_offset += value_size;
            }

            if (utf->schema[i].flag & COLUMN_FLAG_ROW) {
                /* data is found relative to row start */
                utf->schema[i].offset = column_offset;
                column_offset += value_size;
            }
        }
    }

    /* next section is row and variable length data (pointed above) then end of table */

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
    free(utf->schema);
    free(utf);
}


static int utf_query(utf_context* utf, int row, const char* column, utf_result_t* result) {
    int i;


    result->found = 0;

    if (row >= utf->rows || row < 0)
        goto fail;

    /* find target column */
    for (i = 0; i < utf->columns; i++) {
        struct utf_column_t *col = &utf->schema[i];
        uint32_t data_offset;

        if (col->name == NULL || strcmp(col->name, column) != 0)
            continue;

        result->found = 1;
        result->type = col->type;

        if (col->flag & COLUMN_FLAG_DEFAULT) {
            data_offset = utf->table_offset + utf->schema_offset + col->offset;
        }
        else if (col->flag & COLUMN_FLAG_ROW) {
            data_offset = utf->table_offset + utf->rows_offset + row * utf->row_width + col->offset;
        }
        else {
            data_offset = 0;
        }

        /* ignore zero value */
        if (data_offset == 0) {
            memset(&result->value, 0, sizeof(result->value)); /* just in case... */
            break;
        }

        /* read row/constant value */
        switch (col->type) {
            case COLUMN_TYPE_UINT8:
                result->value.value_u8 = read_u8(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_SINT8:
                result->value.value_s8 = read_s8(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_UINT16:
                result->value.value_u16 = read_u16be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_SINT16:
                result->value.value_s16 = read_s16be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_UINT32:
                result->value.value_u32 = read_u32be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_SINT32:
                result->value.value_s32 = read_s32be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_UINT64:
                result->value.value_u64 = read_u64be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_SINT64:
                result->value.value_s64 = read_s64be(data_offset, utf->sf);
                break;
            case COLUMN_TYPE_FLOAT: {
                result->value.value_float = read_f32be(data_offset, utf->sf);
                break;
            }
#if 0
            case COLUMN_TYPE_DOUBLE: {
                result->value.value_double = read_d64be(data_offset, utf->sf);
                break;
            }
#endif
            case COLUMN_TYPE_STRING: {
                uint32_t name_offset = read_u32be(data_offset, utf->sf);
                if (name_offset > utf->strings_size)
                    goto fail;
                result->value.value_string = utf->string_table + name_offset;
                break;
            }

            case COLUMN_TYPE_VLDATA:
                result->value.value_data.offset = read_u32be(data_offset + 0x00, utf->sf);
                result->value.value_data.size   = read_u32be(data_offset + 0x04, utf->sf);
                break;
#if 0
            case COLUMN_TYPE_UINT128: {
                result->value.value_u128.hi = read_u64be(data_offset + 0x00, utf->sf);
                result->value.value_u128.lo = read_u64be(data_offset + 0x08, utf->sf);
                break;
            }
#endif
            default:
                goto fail;
        }

        break; /* column found and read */
    }

    return 1;
fail:
    return 0;
}

static int utf_query_value(utf_context* utf, int row, const char* column, void* value, enum column_type_t type) {
    utf_result_t result = {0};
    int valid;

    valid = utf_query(utf, row, column, &result);
    if (!valid || !result.found || result.type != type)
        return 0;

    switch(result.type) {
        case COLUMN_TYPE_UINT8:  (*(uint8_t*)value)  = result.value.value_u8; break;
        case COLUMN_TYPE_SINT8:  (*(int8_t*)value)   = result.value.value_s8; break;
        case COLUMN_TYPE_UINT16: (*(uint16_t*)value) = result.value.value_u16; break;
        case COLUMN_TYPE_SINT16: (*(int16_t*)value)  = result.value.value_s16; break;
        case COLUMN_TYPE_UINT32: (*(uint32_t*)value) = result.value.value_u32; break;
        case COLUMN_TYPE_SINT32: (*(int32_t*)value)  = result.value.value_s32; break;
        case COLUMN_TYPE_UINT64: (*(uint64_t*)value) = result.value.value_u64; break;
        case COLUMN_TYPE_SINT64: (*(int64_t*)value)  = result.value.value_s64; break;
        case COLUMN_TYPE_STRING: (*(const char**)value) = result.value.value_string; break;
        default:
            return 0;
    }

    return 1;
}

int utf_query_s8(utf_context* utf, int row, const char* column, int8_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_SINT8);
}
int utf_query_u8(utf_context* utf, int row, const char* column, uint8_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_UINT8);
}
int utf_query_s16(utf_context* utf, int row, const char* column, int16_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_SINT16);
}
int utf_query_u16(utf_context* utf, int row, const char* column, uint16_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_UINT16);
}
int utf_query_s32(utf_context* utf, int row, const char* column, int32_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_SINT32);
}
int utf_query_u32(utf_context* utf, int row, const char* column, uint32_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_UINT32);
}
int utf_query_s64(utf_context* utf, int row, const char* column, int64_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_SINT64);
}
int utf_query_u64(utf_context* utf, int row, const char* column, uint64_t* value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_UINT64);
}
int utf_query_string(utf_context* utf, int row, const char* column, const char** value) {
    return utf_query_value(utf, row, column, (void*)value, COLUMN_TYPE_STRING);
}

int utf_query_data(utf_context* utf, int row, const char* column, uint32_t* p_offset, uint32_t* p_size) {
    utf_result_t result = {0};
    int valid;

    valid = utf_query(utf, row, column, &result);
    if (!valid || !result.found || result.type != COLUMN_TYPE_VLDATA)
        return 0;

    if (p_offset) *p_offset = utf->table_offset + utf->data_offset + result.value.value_data.offset;
    if (p_size) *p_size = result.value.value_data.size;
    return 1;
}

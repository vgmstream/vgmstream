#ifndef _ACB_UTF_H_
#define _ACB_UTF_H_

/* CRI @UTF (Universal Table Format?) is a generic database-like table made of
 * rows/columns that contain numbers/strings/ binarydata, which also can be other tables.
 *
 * A table starts with "@UTF" and defines some values (row/data/string offsets, counts, etc)
 * then schema (columns type+name), then rows, string table and binary data. Formats using @UTF
 * store and read data by row number + column name. Being a generic table with no fixed schema
 * CRI uses it for different purposes (.acf: cues, .cpk: files, .aax: bgm, .usm: video, etc).
 *
 * (adapted from hcs's code to do multiple querys in the same table)
 */

// todo divide into some .c file and use for other @UTF parsing

/* API */
typedef struct utf_context utf_context; /* opaque struct */
/*static*/ utf_context* utf_open(STREAMFILE *streamfile, uint32_t table_offset, int* rows, const char* *row_name);
/*static*/ void utf_close(utf_context *utf);

/*static*/ int utf_query_s8(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, int8_t* value);
/*static*/ int utf_query_s16(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, int16_t* value);
/*static*/ int utf_query_string(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, const char* *value);
/*static*/ int utf_query_data(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, uint32_t *offset, uint32_t *size);

/* ************************************************* */
/* INTERNALS */

/* possibly 3b+5b from clUTF decompilation */
#define COLUMN_BITMASK_STORAGE      0xf0
#define COLUMN_BITMASK_TYPE         0x0f

#define COLUMN_STORAGE_ZERO         0x10
#define COLUMN_STORAGE_CONSTANT     0x30
#define COLUMN_STORAGE_ROW          0x50
//#define COLUMN_STORAGE_CONSTANT2    0x70 /* from vgmtoolbox */

#define COLUMN_TYPE_SINT8           0x00
#define COLUMN_TYPE_UINT8           0x01
#define COLUMN_TYPE_SINT16          0x02
#define COLUMN_TYPE_UINT16          0x03
#define COLUMN_TYPE_SINT32          0x04
#define COLUMN_TYPE_UINT32          0x05
#define COLUMN_TYPE_SINT64          0x06
//#define COLUMN_TYPE_UINT64          0x07
#define COLUMN_TYPE_FLOAT           0x08
//#define COLUMN_TYPE_DOUBLE          0x09
#define COLUMN_TYPE_STRING          0x0a
#define COLUMN_TYPE_DATA            0x0b


typedef struct {
    uint32_t offset;
    uint32_t size;
} utf_data_t;
typedef struct {
    int valid; /* table is valid */
    int found;
    int type; /* one of COLUMN_TYPE_* */
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
        utf_data_t value_data;
        const char *value_string;
    } value;
} utf_result;


typedef struct {
    uint8_t flags;
    const char *name;
    uint32_t offset;
} utf_column;

struct utf_context {
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
    /*const*/ utf_column *schema;

    /* derived */
    uint32_t schema_offset;
    uint32_t strings_size;
    /*const*/ char *string_table;
    const char *table_name;
};


/* @UTF table reading, abridged */
/*static*/ utf_context* utf_open(STREAMFILE *streamfile, uint32_t table_offset, int* rows, const char* *row_name) {
    utf_context* utf = NULL;


    utf = calloc(1, sizeof(utf_context));
    if (!utf) goto fail;

    utf->table_offset = table_offset;

    /* check header */
    if (read_32bitBE(table_offset + 0x00, streamfile) != 0x40555446) /* "@UTF" */
        goto fail;

    /* load table header  */
    utf->table_size      = read_32bitBE(table_offset + 0x04, streamfile) + 0x08;
    utf->version         = read_16bitBE(table_offset + 0x08, streamfile);
    utf->rows_offset     = read_16bitBE(table_offset + 0x0a, streamfile) + 0x08;
    utf->strings_offset  = read_32bitBE(table_offset + 0x0c, streamfile) + 0x08;
    utf->data_offset     = read_32bitBE(table_offset + 0x10, streamfile) + 0x08;
    utf->name_offset     = read_32bitBE(table_offset + 0x14, streamfile); /* within string table */
    utf->columns         = read_16bitBE(table_offset + 0x18, streamfile);
    utf->row_width       = read_16bitBE(table_offset + 0x1a, streamfile);
    utf->rows            = read_32bitBE(table_offset + 0x1c, streamfile);

    utf->schema_offset   = 0x20;
    utf->strings_size    = utf->data_offset - utf->strings_offset;

    /* 00: early (32b rows_offset?), 01: +2017 (no apparent differences) */
    if (utf->version != 0x00 && utf->version != 0x01) {
        VGM_LOG("@UTF: unknown version\n");
    }
    if (utf->table_offset + utf->table_size > get_streamfile_size(streamfile))
        goto fail;
    if (utf->rows == 0 || utf->rows_offset > utf->table_size || utf->data_offset > utf->table_size)
        goto fail;
    if (utf->name_offset > utf->strings_size)
        goto fail;


    /* load string table */
    {
        size_t read;

        utf->string_table = calloc(utf->strings_size + 1, sizeof(char));
        if (!utf->string_table) goto fail;

        utf->table_name = utf->string_table + utf->name_offset;

        read = read_streamfile((unsigned char*)utf->string_table, utf->table_offset + utf->strings_offset, utf->strings_size, streamfile);
        if (utf->strings_size != read) goto fail;
    }


    /* load column schema */
    {
        int i;
        uint32_t value_size, column_offset = 0;
        uint32_t schema_offset = utf->table_offset + utf->schema_offset;


        utf->schema = malloc(sizeof(utf_column) * utf->columns);
        if (!utf->schema) goto fail;

        for (i = 0; i < utf->columns; i++) {
            uint8_t flags = read_8bit(schema_offset + 0x00, streamfile);
            uint32_t name_offset = read_32bitBE(schema_offset + 0x01, streamfile);
            if (name_offset > utf->strings_size)
                goto fail;

            utf->schema[i].flags = flags;
            utf->schema[i].name = utf->string_table + name_offset;
            schema_offset += 0x01 + 0x04;

            switch (utf->schema[i].flags & COLUMN_BITMASK_TYPE) {
                case COLUMN_TYPE_SINT8:
                case COLUMN_TYPE_UINT8:
                    value_size = 0x01;
                    break;
                case COLUMN_TYPE_SINT16:
                case COLUMN_TYPE_UINT16:
                    value_size = 0x02;
                    break;
                case COLUMN_TYPE_SINT32:
                case COLUMN_TYPE_UINT32:
                case COLUMN_TYPE_FLOAT:
                case COLUMN_TYPE_STRING:
                    value_size = 0x04;
                    break;
                case COLUMN_TYPE_SINT64:
                //case COLUMN_TYPE_UINT64:
                //case COLUMN_TYPE_DOUBLE:
                case COLUMN_TYPE_DATA:
                    value_size = 0x08;
                    break;
                default:
                    VGM_LOG("@UTF: unknown column type\n");
                    goto fail;
            }

            switch (utf->schema[i].flags & COLUMN_BITMASK_STORAGE) {
                case COLUMN_STORAGE_ROW:
                    utf->schema[i].offset = column_offset;
                    column_offset += value_size;
                    break;
                case COLUMN_STORAGE_CONSTANT:
                //case COLUMN_STORAGE_CONSTANT2:
                    utf->schema[i].offset = schema_offset - (utf->table_offset + utf->schema_offset); /* relative to schema */
                    schema_offset += value_size;
                    break;
                case COLUMN_STORAGE_ZERO:
                    utf->schema[i].offset = 0; /* ? */
                    break;
                default:
                    VGM_LOG("@UTF: unknown column storage\n");
                    goto fail;
            }
        }
    }


    /* write info */
    if (rows) *rows = utf->rows;
    if (row_name) *row_name = utf->string_table + utf->name_offset;

    return utf;
fail:
    utf_close(utf);
    return NULL;
}

/*static*/ void utf_close(utf_context *utf) {
    if (!utf) return;

    free(utf->string_table);
    free(utf->schema);
    free(utf);
}


static int utf_query(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, utf_result* result) {
    int i;


    /* fill in the default stuff */
    result->valid = 0;
    result->found = 0;

    if (row >= utf->rows || row < 0)
        goto fail;

    /* find target column */
    for (i = 0; i < utf->columns; i++) {
        utf_column* col = &utf->schema[i];
        uint32_t data_offset;

        if (strcmp(col->name, column) != 0)
            continue;

        result->found = 1;
        result->type = col->flags & COLUMN_BITMASK_TYPE;

        switch (col->flags & COLUMN_BITMASK_STORAGE) {
            case COLUMN_STORAGE_ROW:
                data_offset = utf->table_offset + utf->rows_offset + row * utf->row_width + col->offset;
                break;
            case COLUMN_STORAGE_CONSTANT:
            //case COLUMN_STORAGE_CONSTANT2:
                data_offset = utf->table_offset + utf->schema_offset + col->offset;
                break;
            case COLUMN_STORAGE_ZERO:
                data_offset = 0;
                memset(&result->value, 0, sizeof(result->value));
                break;
            default:
                goto fail;
        }

        /* ignore zero value */
        if (!data_offset)
            break;

        /* read row/constant value */
        switch (col->flags & COLUMN_BITMASK_TYPE) {
            case COLUMN_TYPE_SINT8:
                result->value.value_u8 = read_8bit(data_offset, streamfile);
                break;
            case COLUMN_TYPE_UINT8:
                result->value.value_u8 = (uint8_t)read_8bit(data_offset, streamfile);
                break;
            case COLUMN_TYPE_SINT16:
                result->value.value_s16 = read_16bitBE(data_offset, streamfile);
                break;
            case COLUMN_TYPE_UINT16:
                result->value.value_u16 = (uint16_t)read_16bitBE(data_offset, streamfile);
                break;
            case COLUMN_TYPE_SINT32:
                result->value.value_s32 = read_32bitBE(data_offset, streamfile);
                break;
            case COLUMN_TYPE_UINT32:
                result->value.value_u32 = (uint32_t)read_32bitBE(data_offset, streamfile);
                break;
            case COLUMN_TYPE_SINT64:
                result->value.value_s64 = read_64bitBE(data_offset, streamfile);
                break;
#if 0
            case COLUMN_TYPE_UINT64:
                result->value.value_u64 = read_64bitBE(data_offset, streamfile);
                break;
#endif
            case COLUMN_TYPE_FLOAT: {
                union { //todo inline function?
                    float float_value;
                    uint32_t int_value;
                } cnv;

                if (sizeof(float) != 4) {
                    VGM_LOG("@UTF: can't convert float\n");
                    goto fail;
                }

                cnv.int_value = (uint32_t)read_32bitBE(data_offset, streamfile);
                result->value.value_float = cnv.float_value;
                break;
            }
#if 0
            case COLUMN_TYPE_DOUBLE: {
                union {
                    double float_value;
                    uint64_t int_value;
                } cnv;

                if (sizeof(double) != 8) {
                    VGM_LOG("@UTF: can't convert double\n");
                    goto fail;
                }

                cnv.int_value = (uint64_t)read_64bitBE(data_offset, streamfile);
                result->value.value_float = cnv.float_value;
                break;
            }
#endif
            case COLUMN_TYPE_STRING: {
                uint32_t name_offset = read_32bitBE(data_offset, streamfile);
                if (name_offset > utf->strings_size)
                    goto fail;
                result->value.value_string = utf->string_table + name_offset;
                break;
            }

            case COLUMN_TYPE_DATA:
                result->value.value_data.offset = read_32bitBE(data_offset + 0x00, streamfile);
                result->value.value_data.size   = read_32bitBE(data_offset + 0x04, streamfile);
                break;

            default:
                goto fail;
        }

        break; /* column found and read */
    }

    result->valid = 1;
    return 1;
fail:
    return 0;
}

////////////////////////////////////////////////////////////

static int utf_query_value(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, void* value, int type) {
    utf_result result = {0};

    utf_query(streamfile, utf, row, column, &result);
    if (!result.valid || !result.found || result.type != type)
        return 0;

    switch(result.type) {
        case COLUMN_TYPE_SINT8:  (*(int8_t*)value)   = result.value.value_s8; break;
        case COLUMN_TYPE_UINT8:  (*(uint8_t*)value)  = result.value.value_u8; break;
        case COLUMN_TYPE_SINT16: (*(int16_t*)value)  = result.value.value_s16; break;
        case COLUMN_TYPE_UINT16: (*(uint16_t*)value) = result.value.value_u16; break;
        case COLUMN_TYPE_SINT32: (*(int32_t*)value)  = result.value.value_s32; break;
        case COLUMN_TYPE_UINT32: (*(uint32_t*)value) = result.value.value_u32; break;
        case COLUMN_TYPE_SINT64: (*(int64_t*)value)  = result.value.value_s64; break;
        //case COLUMN_TYPE_UINT64: (*(uint64_t*)value) = result.value.value_u64; break;
        case COLUMN_TYPE_STRING: (*(const char**)value) = result.value.value_string; break;
        default:
            return 0;
    }

    return 1;
}

/*static*/ int utf_query_s8(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, int8_t* value) {
    return utf_query_value(streamfile, utf, row, column, (void*)value, COLUMN_TYPE_SINT8);
}
/*static*/ int utf_query_s16(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, int16_t* value) {
    return utf_query_value(streamfile, utf, row, column, (void*)value, COLUMN_TYPE_SINT16);
}
/*static*/ int utf_query_string(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, const char* *value) {
    return utf_query_value(streamfile, utf, row, column, (void*)value, COLUMN_TYPE_STRING);
}

/*static*/ int utf_query_data(STREAMFILE *streamfile, utf_context *utf, int row, const char* column, uint32_t *offset, uint32_t *size) {
    utf_result result = {0};

    utf_query(streamfile, utf, row, column, &result);
    if (!result.valid || !result.found || result.type != COLUMN_TYPE_DATA)
        return 0;

    if (offset) *offset = utf->table_offset + utf->data_offset + result.value.value_data.offset;
    if (size) *size = result.value.value_data.size;
    return 1;
}

#endif /* _ACB_UTF_H_ */

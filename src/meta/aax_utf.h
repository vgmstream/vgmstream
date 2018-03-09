#ifndef _AAX_UTF_H_
#define _AAX_UTF_H_

struct utf_query
{
    /* if 0 */
    const char *name;
    int index;
};

struct offset_size_pair
{
    uint32_t offset;
    uint32_t size;
};

struct utf_query_result
{
    int valid;  /* table is valid */
    int found;
    int type;   /* one of COLUMN_TYPE_* */
    union
    {
        uint64_t value_u64;
        uint32_t value_u32;
        uint16_t value_u16;
        uint8_t value_u8;
        float value_float;
        struct offset_size_pair value_data;
        uint32_t value_string;
    } value;

    /* info for the queried table */
    uint32_t rows;
    uint32_t name_offset;
    uint32_t string_table_offset;
    uint32_t data_offset;
};


#define COLUMN_STORAGE_MASK         0xf0
#define COLUMN_STORAGE_PERROW       0x50
#define COLUMN_STORAGE_CONSTANT     0x30
#define COLUMN_STORAGE_ZERO         0x10

#define COLUMN_TYPE_MASK            0x0f
#define COLUMN_TYPE_DATA            0x0b
#define COLUMN_TYPE_STRING          0x0a
#define COLUMN_TYPE_FLOAT           0x08
#define COLUMN_TYPE_8BYTE           0x06
#define COLUMN_TYPE_4BYTE           0x04
#define COLUMN_TYPE_2BYTE2          0x03
#define COLUMN_TYPE_2BYTE           0x02
#define COLUMN_TYPE_1BYTE2          0x01
#define COLUMN_TYPE_1BYTE           0x00

struct utf_column_info
{
    uint8_t type;
    const char *column_name;
    long constant_offset;
};

struct utf_table_info
{
    long table_offset;
    uint32_t table_size;
    uint32_t schema_offset;
    uint32_t rows_offset;
    uint32_t string_table_offset;
    uint32_t data_offset;
    const char *string_table;
    const char *table_name;
    uint16_t columns;
    uint16_t row_width;
    uint32_t rows;

    const struct utf_column_info *schema;
};

/* @UTF table reading, abridged */
static struct utf_query_result analyze_utf(STREAMFILE *infile, const long offset, const struct utf_query *query)
{
    unsigned char buf[4];
    struct utf_table_info table_info;
    char *string_table = NULL;
    struct utf_column_info * schema = NULL;
    struct utf_query_result result;
    uint32_t table_name_string;
    int string_table_size;

    result.valid = 0;

    table_info.table_offset = offset;

    /* check header */
    {
        static const char UTF_signature[4] = "@UTF"; /* intentionally unterminated */
        if (4 != read_streamfile(buf, offset, 4, infile)) goto cleanup_error;
        if (memcmp(buf, UTF_signature, sizeof(UTF_signature)))
        {
            goto cleanup_error;
        }
    }

    /* get table size */
    table_info.table_size = read_32bitBE(offset+4, infile);

    table_info.schema_offset = 0x20;
    table_info.rows_offset = read_32bitBE(offset+8, infile);
    table_info.string_table_offset = read_32bitBE(offset+0xc,infile);
    table_info.data_offset = read_32bitBE(offset+0x10,infile);
    table_name_string = read_32bitBE(offset+0x14,infile);
    table_info.columns = read_16bitBE(offset+0x18,infile);
    table_info.row_width = read_16bitBE(offset+0x1a,infile);
    table_info.rows = read_32bitBE(offset+0x1c,infile);

    /* allocate for string table */
    string_table_size = table_info.data_offset-table_info.string_table_offset;
    string_table = malloc(string_table_size+1);
    if (!string_table) goto cleanup_error;
    table_info.string_table = string_table;
    memset(string_table, 0, string_table_size+1);

    /* load schema */
    schema = malloc(sizeof(struct utf_column_info) * table_info.columns);
    if (!schema) goto cleanup_error;

    {
        int i;
        long schema_current_offset = table_info.schema_offset;
        for (i = 0; i < table_info.columns; i++)
        {
            schema[i].type = read_8bit(schema_current_offset,infile);
            schema_current_offset ++;
            schema[i].column_name = string_table + read_32bitBE(schema_current_offset,infile);
            schema_current_offset += 4;

            if ((schema[i].type & COLUMN_STORAGE_MASK) == COLUMN_STORAGE_CONSTANT)
            {
                schema[i].constant_offset = schema_current_offset;
                switch (schema[i].type & COLUMN_TYPE_MASK)
                {
                    case COLUMN_TYPE_8BYTE:
                    case COLUMN_TYPE_DATA:
                        schema_current_offset+=8;
                        break;
                    case COLUMN_TYPE_STRING:
                    case COLUMN_TYPE_FLOAT:
                    case COLUMN_TYPE_4BYTE:
                        schema_current_offset+=4;
                        break;
                    case COLUMN_TYPE_2BYTE2:
                    case COLUMN_TYPE_2BYTE:
                        schema_current_offset+=2;
                        break;
                    case COLUMN_TYPE_1BYTE2:
                    case COLUMN_TYPE_1BYTE:
                        schema_current_offset++;
                        break;
                    default:
                        goto cleanup_error;
                }
            }
        }
    }

    table_info.schema = schema;

    /* read string table */
    read_streamfile((unsigned char *)string_table,
            table_info.string_table_offset+8+offset,
            string_table_size, infile);
    table_info.table_name = table_info.string_table+table_name_string;

    /* fill in the default stuff */
    result.found = 0;
    result.rows = table_info.rows;
    result.name_offset = table_name_string;
    result.string_table_offset = table_info.string_table_offset;
    result.data_offset = table_info.data_offset;

    /* explore the values */
    if (query) {
        int i, j;

        for (i = 0; i < table_info.rows; i++)
        {
            uint32_t row_offset =
                table_info.table_offset + 8 + table_info.rows_offset +
                i * table_info.row_width;
            const uint32_t row_start_offset = row_offset;

            if (query && i != query->index) continue;

            for (j = 0; j < table_info.columns; j++)
            {
                uint8_t type = table_info.schema[j].type;
                long constant_offset = table_info.schema[j].constant_offset;
                int constant = 0;

                int qthis = (query && i == query->index &&
                        !strcmp(table_info.schema[j].column_name, query->name));

                if (qthis)
                {
                    result.found = 1;
                    result.type = schema[j].type & COLUMN_TYPE_MASK;
                }

                switch (schema[j].type & COLUMN_STORAGE_MASK)
                {
                    case COLUMN_STORAGE_PERROW:
                        break;
                    case COLUMN_STORAGE_CONSTANT:
                        constant = 1;
                        break;
                    case COLUMN_STORAGE_ZERO:
                        if (qthis)
                        {
                            memset(&result.value, 0,
                                    sizeof(result.value));
                        }
                        continue;
                    default:
                        goto cleanup_error;
                }

                if (1)
                {
                    long data_offset;
                    int bytes_read;

                    if (constant)
                    {
                        data_offset = constant_offset;
                    }
                    else
                    {
                        data_offset = row_offset;
                    }

                    switch (type & COLUMN_TYPE_MASK)
                    {
                        case COLUMN_TYPE_STRING:
                            {
                                uint32_t string_offset;
                                string_offset = read_32bitBE(data_offset, infile);
                                bytes_read = 4;
                                if (qthis)
                                {
                                    result.value.value_string = string_offset;
                                }
                            }
                            break;
                        case COLUMN_TYPE_DATA:
                            {
                                uint32_t vardata_offset, vardata_size;

                                vardata_offset = read_32bitBE(data_offset, infile);
                                vardata_size = read_32bitBE(data_offset+4, infile);
                                bytes_read = 8;
                                if (qthis)
                                {
                                    result.value.value_data.offset = vardata_offset;
                                    result.value.value_data.size = vardata_size;
                                }
                            }
                            break;

                        case COLUMN_TYPE_8BYTE:
                            {
                                uint64_t value =
                                    read_32bitBE(data_offset, infile);
                                value <<= 32;
                                value |=
                                    read_32bitBE(data_offset+4, infile);
                                if (qthis)
                                {
                                    result.value.value_u64 = value;
                                }
                                bytes_read = 8;
                                break;
                            }
                        case COLUMN_TYPE_4BYTE:
                            {
                                uint32_t value =
                                    read_32bitBE(data_offset, infile);
                                if (qthis)
                                {
                                    result.value.value_u32 = value;
                                }
                                bytes_read = 4;
                            }
                            break;
                        case COLUMN_TYPE_2BYTE2:
                        case COLUMN_TYPE_2BYTE:
                            {
                                uint16_t value =
                                    read_16bitBE(data_offset, infile);
                                if (qthis)
                                {
                                    result.value.value_u16 = value;
                                }
                                bytes_read = 2;
                            }
                            break;
                        case COLUMN_TYPE_FLOAT:
                            if (sizeof(float) == 4)
                            {
                                union {
                                    float float_value;
                                    uint32_t int_value;
                                } int_float;

                                int_float.int_value = read_32bitBE(data_offset, infile);
                                if (qthis)
                                {
                                    result.value.value_float = int_float.float_value;
                                }
                            }
                            else
                            {
                                read_32bitBE(data_offset, infile);
                                if (qthis)
                                {
                                    goto cleanup_error;
                                }
                            }
                            bytes_read = 4;
                            break;
                        case COLUMN_TYPE_1BYTE2:
                        case COLUMN_TYPE_1BYTE:
                            {
                                uint8_t value =
                                    read_8bit(data_offset, infile);
                                if (qthis)
                                {
                                    result.value.value_u8 = value;
                                }
                                bytes_read = 1;
                            }
                            break;
                        default:
                            goto cleanup_error;
                    }

                    if (!constant)
                    {
                        row_offset += bytes_read;
                    }
                } /* useless if end */
            } /* column for loop end */

            if (row_offset - row_start_offset != table_info.row_width)
                goto cleanup_error;

            if (query && i >= query->index) break;
        } /* row for loop end */
    } /* explore values block end */

//cleanup:

    result.valid = 1;
cleanup_error:

    if (string_table)
    {
        free(string_table);
        string_table = NULL;
    }

    if (schema)
    {
        free(schema);
        schema = NULL;
    }

    return result;
}

static struct utf_query_result query_utf(STREAMFILE *infile, const long offset, const struct utf_query *query)
{
    return analyze_utf(infile, offset, query);
}

/*static*/ struct utf_query_result query_utf_nofail(STREAMFILE *infile, const long offset, const struct utf_query *query, int *error)
{
    const struct utf_query_result result = query_utf(infile, offset, query);

    if (error)
    {
        *error = 0;
        if (!result.valid) *error = 1;
        if (query && !result.found) *error = 1;
    }

    return result;
}

static struct utf_query_result query_utf_key(STREAMFILE *infile, const long offset, int index, const char *name, int *error)
{
    struct utf_query query;
    query.index = index;
    query.name = name;

    return query_utf_nofail(infile, offset, &query, error);
}

/*static*/ uint8_t query_utf_1byte(STREAMFILE *infile, const long offset, int index, const char *name, int *error)
{
    struct utf_query_result result = query_utf_key(infile, offset, index, name, error);
    if (error)
    {
        if (result.type != COLUMN_TYPE_1BYTE) *error = 1;
    }
    return result.value.value_u8;
}

/*static*/ uint32_t query_utf_4byte(STREAMFILE *infile, const long offset, int index, const char *name, int *error)
{
    struct utf_query_result result = query_utf_key(infile, offset, index, name, error);
    if (error)
    {
        if (result.type != COLUMN_TYPE_4BYTE) *error = 1;
    }
    return result.value.value_u32;
}

/*static*/ struct offset_size_pair query_utf_data(STREAMFILE *infile, const long offset,
        int index, const char *name, int *error)
{
    struct utf_query_result result = query_utf_key(infile, offset, index, name, error);
    if (error)
    {
        if (result.type != COLUMN_TYPE_DATA) *error = 1;
    }
    return result.value.value_data;
}


#endif /* SRC_META_AAX_UTF_H_ */

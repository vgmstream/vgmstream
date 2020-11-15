#ifndef _CRI_UTF_H_
#define _CRI_UTF_H_

#include "../streamfile.h"

/* CRI @UTF (Universal Table Format?) is a generic database-like table made of rows (called records)
 * and columns (called fields) that contain numbers/strings/binary data, which also can be other tables.
 *
 * A table starts with "@UTF" and defines some values (row/data/string offsets, counts, etc)
 * then DB schema (column type+name), then rows, string table and binary data. Formats using @UTF
 * store and read data by row number + column name. Being a generic table with no fixed schema
 * CRI uses it for different purposes (.acf: cues, .cpk: files, .aax: bgm, .usm: video, etc),
 * and seems used to serialize classes/objects too.
 *
 * (adapted from hcs's code to do multiple querys in the same table)
 */

//todo move to src/util subdir

/* opaque struct */
typedef struct utf_context utf_context;

/* open a CRI UTF table at offset, returning table name and rows. Passed streamfile is used internally for next calls */
utf_context* utf_open(STREAMFILE* sf, uint32_t table_offset, int* p_rows, const char** p_row_name);
void utf_close(utf_context* utf);
/* query calls */
int utf_query_s8(utf_context* utf, int row, const char* column, int8_t* value);
int utf_query_u8(utf_context* utf, int row, const char* column, uint8_t* value);
int utf_query_s16(utf_context* utf, int row, const char* column, int16_t* value);
int utf_query_u16(utf_context* utf, int row, const char* column, uint16_t* value);
int utf_query_s32(utf_context* utf, int row, const char* column, int32_t* value);
int utf_query_u32(utf_context* utf, int row, const char* column, uint32_t* value);
int utf_query_s64(utf_context* utf, int row, const char* column, int64_t* value);
int utf_query_u64(utf_context* utf, int row, const char* column, uint64_t* value);
int utf_query_string(utf_context* utf, int row, const char* column, const char** value);
int utf_query_data(utf_context* utf, int row, const char* column, uint32_t* offset, uint32_t* size);

#endif /* _CRI_UTF_H_ */

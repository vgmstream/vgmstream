#ifndef _API_HELPERS_H_
#define _API_HELPERS_H_
#include "api.h"
#if LIBVGMSTREAM_ENABLE


/* vgmstream's helper stuff, for plugins.
 */


typedef enum {
    LIBVGMSTREAM_LOG_LEVEL_ALL      = 0,
    LIBVGMSTREAM_LOG_LEVEL_DEBUG    = 20,
    LIBVGMSTREAM_LOG_LEVEL_INFO     = 30,
    LIBVGMSTREAM_LOG_LEVEL_NONE     = 100,
} libvgmstream_loglevel_t;

typedef struct {
    libvgmstream_loglevel_t level;                  // log level
    void (*callback)(int level, const char* str);   // log callback
    bool stdout_callback;                           // use default log callback rather than user supplied
} libvgmstream_log_t;

/* defines a global log callback, as vgmstream sometimes communicates format issues to the user.
 * - note that log is currently set globally rather than per libvgmstream_t
*/
LIBVGMSTREAM_API void libvgmstream_set_log(libvgmstream_log_t* cfg);


/* Returns a list of supported extensions (WARNING: it's pretty big), such as "adx", "dsp", etc.
 * Mainly for plugins that want to know which extensions are supported.
 * - returns NULL if no size is provided
 */
LIBVGMSTREAM_API const char** libvgmstream_get_extensions(size_t* size);


/* Same as above, buf returns a list what vgmstream considers "common" formats (such as "wav", "ogg"),
 * which usually one doesn't want to associate to vgmstream.
 * - returns NULL if no size is provided
 */
LIBVGMSTREAM_API const char** libvgmstream_get_common_extensions(size_t* size);


typedef struct {
    bool is_extension;           /* set if filename is just an extension */
    bool skip_default;           /* set if shouldn't check default formats */
    bool reject_extensionless;   /* set if player can't play extensionless files */
    bool accept_unknown;         /* set to allow any extension (for txth) */
    bool accept_common;          /* set to allow known-but-common extension (when player has plugin priority) */
} libvgmstream_valid_t;

/* returns if vgmstream can parse a filename by extension, to reject some files earlier
 * - doesn't check file contents (that's only done on _open)
 * - config may be NULL
 * - mainly for plugins that fail early; libvgmstream doesn't use this
 */
LIBVGMSTREAM_API bool libvgmstream_is_valid(const char* filename, libvgmstream_valid_t* cfg);


typedef struct {
    bool force_title;           // TODO: what was this for?
    bool subsong_range;         // print a range of possible subsongs after title 'filename#1~N'
    bool remove_extension;      // remove extension from passed filename
    bool remove_archive;        // remove '(archive)|(subfile)' format of some plugins
    const char* filename;       // base file's filename
                                // ** note that sometimes vgmstream doesn't have/know the original name, so it's needed again here
} libvgmstream_title_t;

/* get a simple title for plugins, derived from internal stream name if available
 * - valid after _open
 */
LIBVGMSTREAM_API int libvgmstream_get_title(libvgmstream_t* lib, libvgmstream_title_t* cfg, char* buf, int buf_len);


/* Writes a description of the current song into dst. Will always be null-terminated.
 * - returns < 0 if file was truncated, though will still succeed.
 */
LIBVGMSTREAM_API int libvgmstream_format_describe(libvgmstream_t* lib, char* dst, int dst_size);

#endif
#endif

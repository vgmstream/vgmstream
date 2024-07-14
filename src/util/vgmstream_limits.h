#ifndef _VGMSTREAM_LIMITS_H
#define _VGMSTREAM_LIMITS_H

enum { 
    /* Keep path limit within a reasonable stack size.
     * Windows generally only allows 260 chars in path, but other OSs have higher limits and we handle
     * UTF-8 (that typically uses 2-bytes for common non-latin codepages), plus player may append protocols
     * to paths. Most people wouldn't use huge paths though. */
    PATH_LIMIT = 4096, /* (256 * 8) * 2 = ~max_path * (other_os+extra) * codepage_bytes */

    STREAM_NAME_SIZE = 256,                 /* usually small but may need to concat multiple names */
    VGMSTREAM_MAX_CHANNELS = 64,            /* +40ch multilayers */
    VGMSTREAM_MIN_SAMPLE_RATE = 300,        /* 300 is Wwise min */
    VGMSTREAM_MAX_SAMPLE_RATE = 192000,     /* found in some FSB5 */
    VGMSTREAM_MAX_SUBSONGS = 65535,         /* +20000 isn't that uncommon */
    VGMSTREAM_MAX_NUM_SAMPLES = 1000000000, /* no ~5h vgm hopefully */
};

#endif

#include "../vgmstream.h"
#include "../util/log.h"
#include "plugins.h"
#include "mixing.h"


/* ****************************************** */
/* CONTEXT: simplifies plugin code            */
/* ****************************************** */

int vgmstream_ctx_is_valid(const char* filename, vgmstream_ctx_valid_cfg *cfg) {
    const char** extension_list;
    size_t extension_list_len;
    const char* extension;

    bool is_extension = cfg && cfg->is_extension;
    bool reject_extensionless = cfg && cfg->reject_extensionless;
    bool skip_standard = cfg && cfg->skip_standard;
    bool accept_common = cfg && cfg->accept_common;
    bool accept_unknown = cfg && cfg->accept_common;

    if (is_extension) {
        extension = filename;
    } else {
        extension = filename_extension(filename);
    }

    /* some metas accept extensionless files, but make sure it's not a path (unlikely but...) */
    if (strlen(extension) <= 0) {
        int len = strlen(filename); /* foobar passes an extension as so len may be still 0 */
        if (len <= 0 && !is_extension)
            return false;
        if (len > 1 && (filename[len - 1] == '/' || filename[len - 1] == '\\'))
            return false;
        return !reject_extensionless;
    }

    /* try in default list */
    if (!skip_standard) {
        extension_list = vgmstream_get_formats(&extension_list_len);
        for (int i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0) {
                return true;
            }
        }
    }

    /* try in common extensions */
    if (accept_common) {
        extension_list = vgmstream_get_common_formats(&extension_list_len);
        for (int i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0)
                return true;
        }
    }

    /* allow anything not in the normal list but not in common extensions */
    if (accept_unknown) {
        bool is_common = false;

        extension_list = vgmstream_get_common_formats(&extension_list_len);
        for (int i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0) {
                is_common = true;
                break;
            }
        }

        if (!is_common)
            return true;
    }

    return false;
}

void vgmstream_get_title(char* buf, int buf_len, const char* filename, VGMSTREAM* vgmstream, vgmstream_title_t* cfg) {
    const char* pos;
    char* pos2;
    char temp[1024];

    if (!buf || !buf_len)
        return;

    buf[0] = '\0';
    if (!vgmstream || !filename)
        return;

    /* name without path */
    pos = strrchr(filename, '\\');
    if (!pos)
        pos = strrchr(filename, '/');
    if (!pos)
        pos = filename;
    else
        pos++;

    /* special case for foobar that uses a (archive)|(subfile) notation when opening a 7z/zip/etc directly */
    if (cfg && cfg->remove_archive) {
        const char* subpos = strchr(pos, '|');
        if (subpos)
            pos = subpos + 1;
    }
    strncpy(buf, pos, buf_len);

    /* name without extension */
    if (cfg && cfg->remove_extension) {
        pos2 = strrchr(buf, '.');
        if (pos2 && strlen(pos2) < 15) /* too big extension = file name probably has a dot in the middle */
            pos2[0] = '\0';
    }

    {
        const char* stream_name = vgmstream->stream_name;
        int total_subsongs = vgmstream->num_streams;
        int target_subsong = vgmstream->stream_index;
        //int is_first = vgmstream->stream_index == 0;
        int show_name;

        /* special considerations for TXTP:
         * - full txtp: don't show subsong number, nor name (assumes one names .txtp as wanted)
         * - mini txtp: don't show subsong number, but show name (assumes one choses song #n in filename, but wants title)
         */
        int full_txtp = vgmstream->config.is_txtp && !vgmstream->config.is_mini_txtp;
        int mini_txtp = vgmstream->config.is_mini_txtp;

        if (target_subsong == 0)
            target_subsong = 1;

        /* show number if file has more than 1 subsong */
        if (total_subsongs > 1 && !(full_txtp || mini_txtp)) {
            if (cfg && cfg->subsong_range)
                snprintf(temp, sizeof(temp), "%s#1~%i", buf, total_subsongs);
            else
                snprintf(temp, sizeof(temp), "%s#%i", buf, target_subsong);
            strncpy(buf, temp, buf_len);
        }

        /* show name for some cases */
        show_name = (total_subsongs > 0) && (!cfg || !cfg->subsong_range);
        if (full_txtp)
            show_name = 0;
        if (cfg && cfg->force_title)
            show_name = 1;

        if (stream_name[0] != '\0' && show_name) {
            snprintf(temp, sizeof(temp), "%s (%s)", buf, stream_name);
            strncpy(buf, temp, buf_len);
        }
    }

    buf[buf_len - 1] = '\0';
}

/* ****************************************** */
/* MIXING: modifies vgmstream output          */
/* ****************************************** */

void vgmstream_mixing_enable(VGMSTREAM* vgmstream, int32_t max_sample_count, int* input_channels, int* output_channels) {
    mixing_setup(vgmstream, max_sample_count);
    mixing_info(vgmstream, input_channels, output_channels);

    setup_vgmstream(vgmstream);
}

void vgmstream_mixing_autodownmix(VGMSTREAM* vgmstream, int max_channels) {
    if (max_channels <= 0)
        return;

    /* guess mixing the best we can, using standard downmixing if possible
     * (without mapping we can't be sure if format is using a standard layout) */
    if (vgmstream->channel_layout && max_channels <= 2) {
        mixing_macro_downmix(vgmstream, max_channels);
    }
    else {
        mixing_macro_layer(vgmstream, max_channels, 0, 'e');
    }

    return;
}

void vgmstream_mixing_stereo_only(VGMSTREAM* vgmstream, int start) {
    if (start < 0)
        return;
    /* could check to avoid making mono files in edge cases but meh */

    /* remove channels before start */
    while (start) {
        mixing_push_downmix(vgmstream, 0);
        start--;
    }
    /* remove channels after stereo */
    mixing_push_killmix(vgmstream, start + 2);
}


/* ****************************************** */
/* LOG: log                                   */
/* ****************************************** */

void vgmstream_set_log_callback(int level, void* callback) {
    vgm_log_set_callback(NULL, level, 0, callback);
}

void vgmstream_set_log_stdout(int level) {
    vgm_log_set_callback(NULL, level, 1, NULL);
}

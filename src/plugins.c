#include "vgmstream.h"
#include "plugins.h"
#include "mixing.h"
#include "util/log.h"


/* ****************************************** */
/* CONTEXT: simplifies plugin code            */
/* ****************************************** */

int vgmstream_ctx_is_valid(const char* filename, vgmstream_ctx_valid_cfg *cfg) {
    const char** extension_list;
    size_t extension_list_len;
    const char* extension;
    int i;


    if (cfg->is_extension) {
        extension = filename;
    } else {
        extension = filename_extension(filename);
    }

    /* some metas accept extensionless files, but make sure it's not a path (unlikely but...) */
    if (strlen(extension) <= 0) {
        int len = strlen(filename); /* foobar passes an extension as so len may be still 0 */
        if (len <= 0 && !cfg->is_extension)
            return 0;
        if (len > 1 && (filename[len - 1] == '/' || filename[len - 1] == '\\'))
            return 0;
        return !cfg->reject_extensionless;
    }

    /* try in default list */
    if (!cfg->skip_standard) {
        extension_list = vgmstream_get_formats(&extension_list_len);
        for (i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0) {
                return 1;
            }
        }
    }

    /* try in common extensions */
    if (cfg->accept_common) {
        extension_list = vgmstream_get_common_formats(&extension_list_len);
        for (i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0)
                return 1;
        }
    }

    /* allow anything not in the normal list but not in common extensions */
    if (cfg->accept_unknown) {
        int is_common = 0;

        extension_list = vgmstream_get_common_formats(&extension_list_len);
        for (i = 0; i < extension_list_len; i++) {
            if (strcasecmp(extension, extension_list[i]) == 0) {
                is_common = 1;
                break;
            }
        }

        if (!is_common)
            return 1;
    }

    return 0;
}

void vgmstream_get_title(char* buf, int buf_len, const char* filename, VGMSTREAM* vgmstream, vgmstream_title_t* cfg) {
    const char* pos;
    char* pos2;
    char temp[1024];

    buf[0] = '\0';

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


static void copy_time(int* dst_flag, int32_t* dst_time, double* dst_time_s, int* src_flag, int32_t* src_time, double* src_time_s) {
    if (!*src_flag)
        return;
    *dst_flag = 1;
    *dst_time = *src_time;
    *dst_time_s = *src_time_s;
}

//todo reuse in txtp?
static void load_default_config(play_config_t* def, play_config_t* tcfg) {

    /* loop limit: txtp #L > txtp #l > player #L > player #l */
    if (tcfg->play_forever) {
        def->play_forever = 1;
        def->ignore_loop = 0;
    }
    if (tcfg->loop_count_set) {
        def->loop_count = tcfg->loop_count;
        def->loop_count_set = 1;
        def->ignore_loop = 0;
        if (!tcfg->play_forever)
            def->play_forever = 0;
    }

    /* fade priority: #F > #f, #d */
    if (tcfg->ignore_fade) {
        def->ignore_fade = 1;
    }
    if (tcfg->fade_delay_set) {
        def->fade_delay = tcfg->fade_delay;
        def->fade_delay_set = 1;
    }
    if (tcfg->fade_time_set) {
        def->fade_time = tcfg->fade_time;
        def->fade_time_set = 1;
    }

    /* loop priority: #i > #e > #E (respect player's ignore too) */
    if (tcfg->really_force_loop) {
        //def->ignore_loop = 0;
        def->force_loop = 0;
        def->really_force_loop = 1;
    }
    if (tcfg->force_loop) {
        //def->ignore_loop = 0;
        def->force_loop = 1;
        def->really_force_loop = 0;
    }
    if (tcfg->ignore_loop) {
        def->ignore_loop = 1;
        def->force_loop = 0;
        def->really_force_loop = 0;
    }

    copy_time(&def->pad_begin_set,  &def->pad_begin,    &def->pad_begin_s,      &tcfg->pad_begin_set,   &tcfg->pad_begin,   &tcfg->pad_begin_s);
    copy_time(&def->pad_end_set,    &def->pad_end,      &def->pad_end_s,        &tcfg->pad_end_set,     &tcfg->pad_end,     &tcfg->pad_end_s);
    copy_time(&def->trim_begin_set, &def->trim_begin,   &def->trim_begin_s,     &tcfg->trim_begin_set,  &tcfg->trim_begin,  &tcfg->trim_begin_s);
    copy_time(&def->trim_end_set,   &def->trim_end,     &def->trim_end_s,       &tcfg->trim_end_set,    &tcfg->trim_end,    &tcfg->trim_end_s);
    copy_time(&def->body_time_set,  &def->body_time,    &def->body_time_s,      &tcfg->body_time_set,   &tcfg->body_time,   &tcfg->body_time_s);

    def->is_mini_txtp = tcfg->is_mini_txtp;
    def->is_txtp = tcfg->is_txtp;
}

static void load_player_config(play_config_t* def, vgmstream_cfg_t* vcfg) {
    def->play_forever = vcfg->play_forever;
    def->ignore_loop = vcfg->ignore_loop;
    def->force_loop = vcfg->force_loop;
    def->really_force_loop = vcfg->really_force_loop;
    def->ignore_fade = vcfg->ignore_fade;

    def->loop_count = vcfg->loop_count;
    def->loop_count_set = 1;
    def->fade_delay = vcfg->fade_delay;
    def->fade_delay_set = 1;
    def->fade_time = vcfg->fade_time;
    def->fade_time_set = 1;
}

void vgmstream_apply_config(VGMSTREAM* vgmstream, vgmstream_cfg_t* vcfg) {
    play_config_t defs = {0};
    play_config_t* def = &defs; /* for convenience... */
    play_config_t* tcfg = &vgmstream->config;


    load_player_config(def, vcfg);
    def->config_set = 1;

    if (!vcfg->disable_config_override)
        load_default_config(def, tcfg);

    if (!vcfg->allow_play_forever)
        def->play_forever = 0;

    /* copy final config back */
     *tcfg = *def;

     vgmstream->config_enabled = def->config_set;
     setup_state_vgmstream(vgmstream);
}

/* ****************************************** */
/* TAGS: loads key=val tags from a file       */
/* ****************************************** */

#define VGMSTREAM_TAGS_LINE_MAX 2048

/* opaque tag state */
struct VGMSTREAM_TAGS {
    /* extracted output */
    char key[VGMSTREAM_TAGS_LINE_MAX];
    char val[VGMSTREAM_TAGS_LINE_MAX];

    /* file to find tags for */
    int targetname_len;
    char targetname[VGMSTREAM_TAGS_LINE_MAX];
    /* path of targetname */
    char targetpath[VGMSTREAM_TAGS_LINE_MAX];

    /* tag section for filename (see comments below) */
    int section_found;
    off_t section_start;
    off_t section_end;
    off_t offset;

    /* commands */
    int autotrack_on;
    int autotrack_written;
    int track_count;
    int exact_match;

    int autoalbum_on;
    int autoalbum_written;
};


static void tags_clean(VGMSTREAM_TAGS* tag) {
    int i;
    int val_len = strlen(tag->val);

    /* remove trailing spaces */
    for (i = val_len - 1; i > 0; i--) {
        if (tag->val[i] != ' ')
            break;
        tag->val[i] = '\0';
    }
}

VGMSTREAM_TAGS* vgmstream_tags_init(const char* *tag_key, const char* *tag_val) {
    VGMSTREAM_TAGS* tags = malloc(sizeof(VGMSTREAM_TAGS));
    if (!tags) goto fail;

    *tag_key = tags->key;
    *tag_val = tags->val;

    return tags;
fail:
    return NULL;
}

void vgmstream_tags_close(VGMSTREAM_TAGS *tags) {
    free(tags);
}

/* Find next tag and return 1 if found.
 *
 * Tags can be "global" @TAGS, "command" $TAGS, and "file" %TAGS for a target filename.
 * To extract tags we must find either global tags, or the filename's tag "section"
 * where tags apply: (# @TAGS ) .. (other_filename) ..(# %TAGS section).. (target_filename).
 * When a new "other_filename" is found that offset is marked as section_start, and when
 * target_filename is found it's marked as section_end. Then we can begin extracting tags
 * within that section, until all tags are exhausted. Global tags are extracted as found,
 * so they always go first, also meaning any tags after file's section are ignored.
 * Command tags have special meanings and are output after all section tags. */
int vgmstream_tags_next_tag(VGMSTREAM_TAGS* tags, STREAMFILE* tagfile) {
    off_t file_size = get_streamfile_size(tagfile);
    char currentname[VGMSTREAM_TAGS_LINE_MAX] = {0};
    char line[VGMSTREAM_TAGS_LINE_MAX];
    int ok, bytes_read, line_ok, n1,n2;

    if (!tags)
        return 0;

    /* prepare file start and skip BOM if needed */
    if (tags->offset == 0) {
        if ((uint16_t)read_16bitLE(0x00, tagfile) == 0xFFFE ||
            (uint16_t)read_16bitLE(0x00, tagfile) == 0xFEFF) {
            tags->offset = 0x02;
            if (tags->section_start == 0)
                tags->section_start = 0x02;
        }
        else if (((uint32_t)read_32bitBE(0x00, tagfile) & 0xFFFFFF00) ==  0xEFBBBF00) {
            tags->offset = 0x03;
            if (tags->section_start == 0)
                tags->section_start = 0x03;
        }
    }

    /* read lines */
    while (tags->offset <= file_size) {

        /* after section: no more tags to extract */
        if (tags->section_found && tags->offset >= tags->section_end) {

            /* write extra tags after all regular tags */
            if (tags->autotrack_on && !tags->autotrack_written) {
                sprintf(tags->key, "%s", "TRACK");
                sprintf(tags->val, "%i", tags->track_count);
                tags->autotrack_written = 1;
                return 1;
            }

            if (tags->autoalbum_on && !tags->autoalbum_written && tags->targetpath[0] != '\0') {
                const char* path;

                path = strrchr(tags->targetpath,'\\');
                if (!path) {
                    path = strrchr(tags->targetpath,'/');
                }
                if (!path) {
                    path = tags->targetpath;
                }

                sprintf(tags->key, "%s", "ALBUM");
                sprintf(tags->val, "%s", path+1);
                tags->autoalbum_written = 1;
                return 1;
            }

            goto fail;
        }

        bytes_read = read_line(line, sizeof(line), tags->offset, tagfile, &line_ok);
        if (!line_ok || bytes_read == 0) goto fail;

        tags->offset += bytes_read;


        if (tags->section_found) {
            /* find possible file tag */
            ok = sscanf(line, "# %%%[^%%]%% %[^\r\n] ", tags->key,tags->val); /* key with spaces */
            if (ok != 2)
                ok = sscanf(line, "# %%%[^ \t] %[^\r\n] ", tags->key,tags->val); /* key without */
            if (ok == 2) {
                tags_clean(tags);
                return 1;
            }
        }
        else {

            if (line[0] == '#') {
                /* find possible global command */
                ok = sscanf(line, "# $%n%[^ \t]%n %[^\r\n]", &n1, tags->key, &n2, tags->val);
                if (ok == 1 || ok == 2) {
                    int key_len = n2 - n1;
                    if (strncasecmp(tags->key, "AUTOTRACK", key_len) == 0) {
                        tags->autotrack_on = 1;
                    }
                    else if (strncasecmp(tags->key, "AUTOALBUM", key_len) == 0) {
                        tags->autoalbum_on = 1;
                    }
                    else if (strncasecmp(tags->key, "EXACTMATCH", key_len) == 0) {
                        tags->exact_match = 1;
                    }

                    continue; /* not an actual tag */
                }

                /* find possible global tag */
                ok = sscanf(line, "# @%[^@]@ %[^\r\n]", tags->key, tags->val); /* key with spaces */
                if (ok != 2)
                    ok = sscanf(line, "# @%[^ \t] %[^\r\n]", tags->key, tags->val); /* key without */
                if (ok == 2) {
                    tags_clean(tags);
                    return 1;
                }

                continue; /* next line */
            }

            /* find possible filename and section start/end
             * (.m3u seem to allow filenames with whitespaces before, make sure to trim) */
            ok = sscanf(line, " %n%[^\r\n]%n ", &n1, currentname, &n2);
            if (ok == 1)  {
                int currentname_len = n2 - n1;
                int filename_found = 0;

                /* we want to match file with the same name (case insensitive), OR a virtual .txtp with
                 * the filename inside to ease creation of tag files with config, also check end char to 
                 * tell apart the unlikely case of having both 'bgm01.ad.txtp' and 'bgm01.adp.txtp' */

                /* try exact match (strcasecmp works ok even for UTF-8) */
                if (currentname_len == tags->targetname_len &&
                        strncasecmp(currentname, tags->targetname, currentname_len) == 0) {
                    filename_found = 1;
                }
                else if (!tags->exact_match) {
                    /* try tagfile is "bgm.adx" + target is "bgm.adx #(cfg) .txtp" */
                    if (currentname_len < tags->targetname_len &&
                            strncasecmp(currentname, tags->targetname, currentname_len) == 0 &&
                            vgmstream_is_virtual_filename(tags->targetname)) {
                        char c = tags->targetname[currentname_len];
                        filename_found = (c==' ' || c == '.' || c == '#');
                    }
                    /* tagfile has "bgm.adx (...) .txtp" + target has "bgm.adx" */
                    else if (tags->targetname_len < currentname_len &&
                            strncasecmp(tags->targetname, currentname, tags->targetname_len) == 0 &&
                            vgmstream_is_virtual_filename(currentname)) {
                        char c = currentname[tags->targetname_len];
                        filename_found = (c==' ' || c == '.' || c == '#');
                    }
                }

                if (filename_found) {
                    /* section ok, start would be set before this (or be 0) */
                    tags->section_end = tags->offset;
                    tags->section_found = 1;
                    tags->offset = tags->section_start;
                }
                else {
                    /* mark new possible section */
                    tags->section_start = tags->offset;
                }

                tags->track_count++; /* new track found (target filename or not) */
                continue;
            }

            /* empty/bad line, probably */
        }
    }

    /* may reach here if read up to file_size but no section was found */

fail:
    tags->key[0] = '\0';
    tags->val[0] = '\0';
    return 0;
}


void vgmstream_tags_reset(VGMSTREAM_TAGS* tags, const char* target_filename) {
    char *path;

    if (!tags)
        return;

    memset(tags, 0, sizeof(VGMSTREAM_TAGS));

    //todo validate sizes and copy sensible max

    /* get base name */
    strcpy(tags->targetpath, target_filename);

    /* Windows CMD accepts both \\ and /, and maybe plugin uses either */
    path = strrchr(tags->targetpath,'\\');
    if (!path) {
        path = strrchr(tags->targetpath,'/');
    }
    if (path != NULL) {
        path[0] = '\0'; /* leave targetpath with path only */
        path = path+1;
    }

    if (path) {
        strcpy(tags->targetname, path);
    } else {
        tags->targetpath[0] = '\0';
        strcpy(tags->targetname, target_filename);
    }
    tags->targetname_len = strlen(tags->targetname);
}

/* ****************************************** */
/* MIXING: modifies vgmstream output          */
/* ****************************************** */

void vgmstream_mixing_enable(VGMSTREAM* vgmstream, int32_t max_sample_count, int *input_channels, int *output_channels) {
    mixing_setup(vgmstream, max_sample_count);
    mixing_info(vgmstream, input_channels, output_channels);

    /* update internals */
    mixing_info(vgmstream, &vgmstream->pstate.input_channels, &vgmstream->pstate.output_channels);
    setup_vgmstream(vgmstream);
}

void vgmstream_mixing_autodownmix(VGMSTREAM *vgmstream, int max_channels) {
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

void vgmstream_mixing_stereo_only(VGMSTREAM *vgmstream, int start) {
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

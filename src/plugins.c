#include "vgmstream.h"
#include "plugins.h"
#include "mixing.h"


/* ****************************************** */
/* CONTEXT: simplifies plugin code            */
/* ****************************************** */

int vgmstream_ctx_is_valid(const char* filename, vgmstream_ctx_valid_cfg *cfg) {
    const char ** extension_list;
    size_t extension_list_len;
    const char *extension;
    int i;


    if (cfg->is_extension) {
        extension = filename;
    } else {
        extension = filename_extension(filename);
    }

    /* some metas accept extensionless files */
    if (strlen(extension) <= 0) {
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
                ok = sscanf(line, "# $%[^ \t] %[^\r\n]", tags->key,tags->val);
                if (ok == 1 || ok == 2) {
                    if (strcasecmp(tags->key,"AUTOTRACK") == 0) {
                        tags->autotrack_on = 1;
                    }
                    else if (strcasecmp(tags->key,"AUTOALBUM") == 0) {
                        tags->autoalbum_on = 1;
                    }

                    continue; /* not an actual tag */
                }

                /* find possible global tag */
                ok = sscanf(line, "# @%[^@]@ %[^\r\n]", tags->key,tags->val); /* key with spaces */
                if (ok != 2)
                    ok = sscanf(line, "# @%[^ \t] %[^\r\n]", tags->key,tags->val); /* key without */
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

                /* we want to find file with the same name (case insensitive), OR a virtual .txtp with
                 * the filename inside (so 'file.adx' gets tags from 'file.adx#i.txtp', reading
                 * tags even if we don't open !tags.m3u with virtual .txtp directly) */

                /* strcasecmp works ok even for UTF-8 */
                if (currentname_len >= tags->targetname_len && /* starts with targetname */
                        strncasecmp(currentname, tags->targetname, tags->targetname_len) == 0) {

                    if (currentname_len == tags->targetname_len) { /* exact match */
                        filename_found = 1;
                    }
                    else if (vgmstream_is_virtual_filename(currentname)) { /* ends with .txth */
                        char c = currentname[tags->targetname_len];
                        /* tell apart the unlikely case of having both 'bgm01.ad.txtp' and 'bgm01.adp.txtp' */
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

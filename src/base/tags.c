#include "../vgmstream.h"
#include "../util/log.h"
#include "../util/reader_sf.h"
#include "../util/reader_text.h"
#include "plugins.h"

/* TAGS: loads key=val tags from a file       */

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
    bool section_found;
    off_t section_start;
    off_t section_end;
    off_t offset;

    /* commands */
    bool autotrack_on;
    bool autotrack_written;
    int track_count;
    bool exact_match;

    bool autoalbum_on;
    bool autoalbum_written;
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
    VGMSTREAM_TAGS* tags = calloc(1, sizeof(VGMSTREAM_TAGS));
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
        size_t bom_size = read_bom(tagfile);
        tags->offset = bom_size;
        if (tags->section_start == 0)
            tags->section_start = bom_size;
    }

    /* read lines */
    while (tags->offset <= file_size) {

        /* after section: no more tags to extract */
        if (tags->section_found && tags->offset >= tags->section_end) {

            /* write extra tags after all regular tags */
            if (tags->autotrack_on && !tags->autotrack_written) {
                sprintf(tags->key, "%s", "TRACK");
                sprintf(tags->val, "%i", tags->track_count);
                tags->autotrack_written = true;
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
                tags->autoalbum_written = true;
                return 1;
            }

            goto fail;
        }

        bytes_read = read_line(line, sizeof(line), tags->offset, tagfile, &line_ok);
        if (!line_ok || bytes_read == 0) goto fail;

        tags->offset += bytes_read;


        if (tags->section_found) {
            /* find possible file tag */
            ok = sscanf(line, "# %%%[^%%]%% %[^\r\n] ", tags->key, tags->val); // key with spaces
            if (ok != 2)
                ok = sscanf(line, "# %%%[^ \t] %[^\r\n] ", tags->key, tags->val); // key without
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
                        tags->autotrack_on = true;

                        // reset just in case (may be useful for discs/sections)
                        tags->track_count = 0;
                        tags->autotrack_written = false;
                    }
                    else if (strncasecmp(tags->key, "AUTOALBUM", key_len) == 0) {
                        tags->autoalbum_on = true;
                    }
                    else if (strncasecmp(tags->key, "EXACTMATCH", key_len) == 0) {
                        tags->exact_match = true;
                    }

                    continue; /* not an actual tag */
                }

                /* find possible global tag */
                ok = sscanf(line, "# @%[^@]@ %[^\r\n]", tags->key, tags->val); // key with spaces
                if (ok != 2)
                    ok = sscanf(line, "# @%[^ \t] %[^\r\n]", tags->key, tags->val); // key without
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
                    tags->section_found = true;
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

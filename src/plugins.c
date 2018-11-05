#include "vgmstream.h"
#include "plugins.h"


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

/* Tags are divided in two: "global" @TAGS and "file" %TAGS for target filename. To extract both
 * we find the filename's tag "section": (other_filename) ..(#tag section).. (target_filename).
 * When a new "other_filename" is found that offset is marked as section_start, and when target_filename
 * is found it's marked as section_end. Then we can begin extracting tags within that section, until
 * all tags are exhausted. Global tags are extracted while searching, so they always go first, and
 * also meaning any tags after the section is found are ignored. */
int vgmstream_tags_next_tag(VGMSTREAM_TAGS* tag, STREAMFILE* tagfile) {
    off_t file_size = get_streamfile_size(tagfile);
    char currentname[TAG_LINE_MAX] = {0};
    char line[TAG_LINE_MAX] = {0};
    int ok, bytes_read, line_done;


    /* prepare file start and skip BOM if needed */
    if (tag->offset == 0) {
        if ((uint16_t)read_16bitLE(0x00, tagfile) == 0xFFFE ||
            (uint16_t)read_16bitLE(0x00, tagfile) == 0xFEFF) {
            tag->offset = 0x02;
            if (tag->section_start == 0)
                tag->section_start = 0x02;
        }
        else if (((uint32_t)read_32bitBE(0x00, tagfile) & 0xFFFFFF00) ==  0xEFBBBF00) {
            tag->offset = 0x03;
            if (tag->section_start == 0)
                tag->section_start = 0x03;
        }
    }

    /* read lines */
    while (tag->offset < file_size) {
        /* no more tags to extract */
        if (tag->section_found && tag->offset >= tag->section_end) {
            goto fail;
        }

        bytes_read = get_streamfile_text_line(TAG_LINE_MAX,line, tag->offset,tagfile, &line_done);
        if (!line_done) goto fail;

        tag->offset += bytes_read;


        if (tag->section_found) {
            /* find possible file tag */
            ok = sscanf(line, "# %%%[^ \t] %[^\r\n] ", tag->key,tag->val);
            if (ok == 2) {
                tags_clean(tag);
                return 1;
            }
        }
        else {
            /* find possible global tag */
            if (line[0] == '#') {
                ok = sscanf(line, "# @%[^ \t] %[^\r\n]", tag->key,tag->val);
                if (ok == 2) {
                    tags_clean(tag);
                    return 1;
                }

                continue; /* next line */
            }

            /* find possible filename and section start/end */
            ok = sscanf(line, " %[^\r\n] ", currentname);
            if (ok == 1)  {
                if (strcasecmp(tag->targetname,currentname) == 0) { /* looks ok even for UTF-8 */
                    /* section ok, start would be set before this (or be 0) */
                    tag->section_end = tag->offset;
                    tag->section_found = 1;
                    tag->offset = tag->section_start;
                }
                else {
                    /* mark new possible section */
                    tag->section_start = tag->offset;
                }

                continue;
            }

            /* empty/bad line, probably */
        }
    }

    /* may reach here if read up to file_size but no section was found */

fail:
    tag->key[0] = '\0';
    tag->val[0] = '\0';
    return 0;
}


void vgmstream_tags_reset(VGMSTREAM_TAGS* tag, const char* target_filename) {
    const char *path;

    memset(tag, 0, sizeof(VGMSTREAM_TAGS));


    /* get base name */

    //todo Windows CMD accepts both \\ and /, better way to handle this?
    path = strrchr(target_filename,'\\');
    if (!path)
        path = strrchr(target_filename,'/');
    if (path != NULL)
        path = path+1;

    //todo validate sizes and copy sensible max
    if (path) {
        strcpy(tag->targetname, path);
    } else {
        strcpy(tag->targetname, target_filename);
    }
}

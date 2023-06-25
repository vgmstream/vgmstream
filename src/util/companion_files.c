#include "companion_files.h"
#include "paths.h"
#include "../vgmstream.h"
#include "reader_text.h"
#include "sf_utils.h"


size_t read_key_file(uint8_t* buf, size_t buf_size, STREAMFILE* sf) {
    char keyname[PATH_LIMIT];
    char filename[PATH_LIMIT];
    const char *path, *ext;
    STREAMFILE* sf_key = NULL;
    size_t keysize;

    get_streamfile_name(sf, filename, sizeof(filename));

    if (strlen(filename)+4 > sizeof(keyname)) goto fail;

    /* try to open a keyfile using variations */
    {
        ext = strrchr(filename,'.');
        if (ext!=NULL) ext = ext+1;

        path = strrchr(filename, DIR_SEPARATOR);
        if (path!=NULL) path = path+1;

        /* "(name.ext)key" */
        strcpy(keyname, filename);
        strcat(keyname, "key");
        sf_key = sf->open(sf, keyname, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (sf_key) goto found;

        /* "(name.ext)KEY" */
        /*
        strcpy(keyname+strlen(keyname)-3,"KEY");
        sf_key = sf->open(sf, keyname, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (sf_key) goto found;
        */


        /* "(.ext)key" */
        if (path) {
            strcpy(keyname, filename);
            keyname[path-filename] = '\0';
            strcat(keyname, ".");
        } else {
            strcpy(keyname, ".");
        }
        if (ext) strcat(keyname, ext);
        strcat(keyname, "key");
        sf_key = sf->open(sf, keyname, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (sf_key) goto found;

        /* "(.ext)KEY" */
        /*
        strcpy(keyname+strlen(keyname)-3,"KEY");
        sf_key = sf->open(sf, keyname, STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (sf_key) goto found;
        */

        goto fail;
    }

found:
    keysize = get_streamfile_size(sf_key);
    if (keysize > buf_size) goto fail;

    if (read_streamfile(buf, 0, keysize, sf_key) != keysize)
        goto fail;

    close_streamfile(sf_key);
    return keysize;

fail:
    close_streamfile(sf_key);
    return 0;
}

STREAMFILE* read_filemap_file(STREAMFILE* sf, int file_num) {
    return read_filemap_file_pos(sf, file_num, NULL);
}

STREAMFILE* read_filemap_file_pos(STREAMFILE* sf, int file_num, int* p_pos) {
    char filename[PATH_LIMIT];
    off_t txt_offset, file_size;
    STREAMFILE* sf_map = NULL;
    int file_pos = 0;

    sf_map = open_streamfile_by_filename(sf, ".txtm");
    if (!sf_map) goto fail;

    get_streamfile_filename(sf, filename, sizeof(filename));

    txt_offset = read_bom(sf_map);
    file_size = get_streamfile_size(sf_map);

    /* read lines and find target filename, format is (filename): value1, ... valueN */
    while (txt_offset < file_size) {
        char line[0x2000];
        char key[PATH_LIMIT] = { 0 }, val[0x2000] = { 0 };
        int ok, bytes_read, line_ok;

        bytes_read = read_line(line, sizeof(line), txt_offset, sf_map, &line_ok);
        if (!line_ok) goto fail;

        txt_offset += bytes_read;

        /* get key/val (ignores lead/trailing spaces, stops at comment/separator) */
        ok = sscanf(line, " %[^\t#:] : %[^\t#\r\n] ", key, val);
        if (ok != 2) { /* ignore line if no key=val (comment or garbage) */
            /* better way? */
            if (strcmp(line, "#@reset-pos") == 0) {
                file_pos = 0;
            }
            continue;
        }

        if (strcmp(key, filename) == 0) {
            int n;
            char subval[PATH_LIMIT];
            const char* current = val;
            int i;

            for (i = 0; i <= file_num; i++) {
                if (current[0] == '\0')
                    goto fail;

                ok = sscanf(current, " %[^\t#\r\n,]%n ", subval, &n);
                if (ok != 1)
                    goto fail;

                if (i == file_num) {
                    if (p_pos) *p_pos = file_pos;

                    close_streamfile(sf_map);
                    return open_streamfile_by_filename(sf, subval);
                }

                current += n;
                if (current[0] == ',')
                    current++;
            }
        }
        file_pos++;
    }

fail:
    close_streamfile(sf_map);
    return NULL;
}

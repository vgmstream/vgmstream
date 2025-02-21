#include <stdio.h>
#include "xmp_vgmstream.h"

/*  with <v3.8.5.62, any more than ~1000 will crash XMplay's file list screen. */
#define EXTENSION_LIST_SIZE_OLD  1000
#define EXTENSION_LIST_SIZE_OLD_VERSION 0x0308053d /* less than v3.8.5.62 */


/* Adds ext to XMPlay's extension list. */
static int add_extension(int length, char* dst, const char* ext) {

    if (length <= 1)
        return 0;

    int ext_len = strlen(ext);

    /* check if end reached or not enough room to add */
    if (ext_len + 2 > length - 2) {
        dst[0] = '\0';
        return 0;
    }

    /* copy new extension + null terminate */
    int i;
    for (i = 0; i < ext_len; i++)
        dst[i] = ext[i];
    dst[i] = '/';
    dst[i+1] = '\0';
    return i + 1;
}

/* Creates XMPlay's extension list, a single string with 2 nulls.
 * Extensions must be in this format: "Description\0extension1/.../extensionN" */
void build_extension_list(char* extension_list, int list_size, DWORD version) {
    int limit_old = EXTENSION_LIST_SIZE_OLD;
    int limit = list_size;
    if (limit > limit_old && version <= EXTENSION_LIST_SIZE_OLD_VERSION)
        limit = limit_old;

    int written = sprintf(extension_list, "%s%c", "vgmstream files",'\0');

    int ext_list_len;
    const char** ext_list = libvgmstream_get_extensions(&ext_list_len);

    for (int i = 0; i < ext_list_len; i++) {
        written += add_extension(limit - written, extension_list + written, ext_list[i]);
    }
    extension_list[written - 1] = '\0'; // remove last "/"
}


/* Get tags as an array of "key\0value\0", NULL-terminated.
 * Processes tags from the path alone (expects folders to be named in a particular way),
 * so of limited usefulness. */
char* get_tags_from_filepath_info(libvgmstream_t* infostream, XMPFUNC_MISC* xmpf_misc, const char* filepath) {
    char* tags;
    int pos = 0;

    tags = xmpf_misc->Alloc(1024);
    memset(tags, 0x00, 1024);

    if (strlen(infostream->format->stream_name) > 0) {
        memcpy(tags + pos, "title", 5);
        pos += 6;
        memcpy(tags + pos, infostream->format->stream_name, strlen(infostream->format->stream_name));
        pos += strlen(infostream->format->stream_name) + 1;
    }
    
    const char* end;
    const char* start = NULL;
    int j = 2;
    for (const char* i = filepath + strlen(filepath); i > filepath; i--)
    {
        if ((*i == '\\') && (j == 1))
        {
            start = i + 1;
            j--;
            break;
        }
        if ((*i == '\\') && (j == 2))
        {
            end = i;
            j--;
        }
    }

    //run some sanity checks

    int brace_curly = 0, brace_square = 0;
    char check_ok = 0;
    for (const char* i = filepath; *i != 0; i++)
    {
        if (*i == '(')
            brace_curly++;
        if (*i == ')')
            brace_curly--;
        if (*i == '[')
            brace_square++;
        if (*i == ']')
            brace_square--;
        if (brace_curly > 1 || brace_curly < -1 || brace_square > 1 || brace_square < -1)
            break;
    }

    if (brace_square == 0 && brace_curly == 0)
        check_ok = 1;

    if (start != NULL && strstr(filepath, "\\VGMPP\\") != NULL && check_ok == 1 && strchr(start, '(') != NULL)
    {
        char tagline[1024];
        memset(tagline, 0x00, sizeof(tagline));
        strncpy(tagline, start, end - start);
        
        char* alttitle_st;
        char* alttitle_ed;
        char* album_st;
        char* album_ed;
        char* company_st;
        char* company_ed;
        char* company2_st;
        char* company2_ed;
        char* date_st;
        char* date_ed;
        char* platform_st;
        char* platform_ed;

        if (strchr(tagline, '[') != NULL) //either alternative title or platform usually
        {
            alttitle_st = strchr(tagline, '[') + 1;
            alttitle_ed = strchr(alttitle_st, ']');
            if (strchr(alttitle_st, '[') != NULL && strchr(alttitle_st, '[') > strchr(alttitle_st, '(')) //both might be present actually
            {
                platform_st = strchr(alttitle_st, '[') + 1;
                platform_ed = strchr(alttitle_ed + 1, ']');
            }
            else
            {
                platform_st = NULL;
                platform_ed = NULL;
            }
        }
        else
        {
            platform_st = NULL;
            platform_ed = NULL;
            alttitle_st = NULL;
            alttitle_ed = NULL;
        }

        album_st = tagline;

        if (strchr(tagline, '(') < alttitle_st && alttitle_st != NULL) //square braces after curly braces -- platform
        {
            platform_st = alttitle_st;
            platform_ed = alttitle_ed;
            alttitle_st = NULL;
            alttitle_ed = NULL;
            album_ed = strchr(tagline, '('); //get normal title for now
        }
        else if (alttitle_st != NULL)
            album_ed = strchr(tagline, '[');
        else
            album_ed = strchr(tagline, '(');

        date_st = strchr(album_ed, '(') + 1; //first string in curly braces is usualy release date, I have one package with platform name there
        if (date_st == NULL)
            date_ed = NULL;
        if (date_st[0] >= 0x30 && date_st[0] <= 0x39 && date_st[1] >= 0x30 && date_st[1] <= 0x39) //check if it contains 2 digits
        {
            date_ed = strchr(date_st, ')');
        }
        else //platform?
        {
            platform_st = date_st;
            platform_ed = strchr(date_st, ')');
            date_st = strchr(platform_ed, '(') + 1;
            date_ed = strchr(date_st, ')');
        }
        
        company_st = strchr(date_ed, '(') + 1; //company name follows date
        if (company_st != NULL)
        {
            company_ed = strchr(company_st, ')');
            if (strchr(company_ed, '(') != NULL)
            {
                company2_st = strchr(company_ed, '(') + 1;
                company2_ed = strchr(company2_st, ')');
            }
            else
            {
                company2_st = NULL;
                company2_ed = NULL;
            }
        }
        else
        {
            company_st = NULL;
            company_ed = NULL;
            company2_st = NULL;
            company2_ed = NULL;
        }

        if (alttitle_st != NULL) //prefer alternative title, which is usually japanese
        {
            memcpy(tags + pos, "album", 5);
            pos += 6;
            memcpy(tags + pos, alttitle_st, alttitle_ed - alttitle_st);
            pos += alttitle_ed - alttitle_st + 1;
        }
        else
        {
            memcpy(tags + pos, "album", 5);
            pos += 6;
            memcpy(tags + pos, album_st, album_ed - album_st);
            pos += album_ed - album_st + 1;
        }

        if (date_st != NULL)
        {
            memcpy(tags + pos, "date", 4);
            pos += 5;
            memcpy(tags + pos, date_st, date_ed - date_st);
            pos += date_ed - date_st + 1;
        }

        if (date_st != NULL)
        {
            memcpy(tags + pos, "genre", 5);
            pos += 6;
            memcpy(tags + pos, platform_st, platform_ed - platform_st);
            pos += platform_ed - platform_st + 1;
        }

        if (company_st != NULL)
        {
            char combuf[256];
            memset(combuf, 0x00, sizeof(combuf));
            char tmp[128];
            memset(tmp, 0x00, sizeof(tmp));
            memcpy(tmp, company_st, company_ed - company_st);
            if (company2_st != NULL)
            {
                char tmp2[128];
                memset(tmp2, 0x00, sizeof(tmp2));
                memcpy(tmp2, company2_st, company2_ed - company2_st);
                sprintf(combuf, "\r\n\r\nDeveloper\t%s\r\nPublisher\t%s", tmp, tmp2);
            }
            else
                sprintf(combuf, "\r\n\r\nDeveloper\t%s", tmp);

            memcpy(tags + pos, "comment", 7);
            pos += 8;
            memcpy(tags + pos, combuf, strlen(combuf));
            pos += strlen(combuf) + 1;
        }
    }

    return tags; /* assuming XMPlay free()s this, since it Alloc()s it */
}

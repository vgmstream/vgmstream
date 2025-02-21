#include "in_vgmstream.h"


/* Adds ext to Winamp's extension list */
static int add_extension(char* dst, int dst_len, const char* ext) {
    int ext_len;

    ext_len = strlen(ext);
    if (dst_len <= ext_len + 1)
        return 0;

    strcpy(dst, ext); /* seems winamp uppercases this if needed */
    dst[ext_len] = ';';

    return ext_len + 1;
}

/* Creates Winamp's extension list, a single string that ends with \0\0.
 * Each extension must be in this format: "extensions\0Description\0"
 *
 * The list is used to accept extensions by default when IsOurFile returns 0, to register file
 * types, and in the open dialog's type combo. Format actually can be:
 * - "ext1;ext2;...\0EXTS Audio Files (*.ext1; *.ext2; *...\0", //single line with all
 * - "ext1\0EXT1 Audio File (*.ext1)\0ext2\0EXT2 Audio File (*.ext2)\0...", //multiple lines
 * Open dialog's text (including all plugin's "Description") seems limited to old MAX_PATH 260
 * (max size for "extensions" checks seems ~0x40000 though). Given vgmstream's huge number
 * of exts, use single line to (probably) work properly with dialogs (used to be multi line).
 */
void build_extension_list(char* winamp_list, int winamp_list_size) {
    const char** ext_list;
    int ext_list_len;
    int i, written;
    int description_size = 0x100; /* reserved max at the end */

    winamp_list[0] = '\0';
    winamp_list[1] = '\0';

    ext_list = libvgmstream_get_extensions(&ext_list_len);

    for (i = 0; i < ext_list_len; i++) {
        int used = add_extension(winamp_list, winamp_list_size - description_size, ext_list[i]);
        if (used <= 0) {
            //vgm_logi("build_extension_list: not enough buf for all exts\n");
            break;
        }
        winamp_list += used;
        winamp_list_size -= used;
    }
    if (i > 0) {
        winamp_list[-1] = '\0'; /* last "ext;" to "ext\0" */
    }

    /* generic description for the info dialog since we can't really show everything */
    written = snprintf(winamp_list, winamp_list_size - 2, "vgmstream Audio Files%c", '\0');

    /* should end with double \0 */
    if (written < 0) {
        winamp_list[0] = '\0';
        winamp_list[1] = '\0';
    }
    else {
        winamp_list[written + 0] = '\0';
        winamp_list[written + 1] = '\0';
    }
}


/* makes a modified filename, suitable to pass parameters around */
static void make_fn_subsong(in_char* dst, int dst_size, const in_char* filename, int subsong_index) {
    /* Follows "(file)(config)(ext)". Winamp needs to "see" (ext) to validate, and file goes first so relative
     * files work in M3Us (path is added). Protocols a la "vgmstream://(config)(file)" work but don't get full paths. */
    wa_snprintf(dst,dst_size, wa_L("%s|$s=%i|.vgmstream"), filename, subsong_index);
}

/* unpacks the subsongs by adding entries to the playlist */
bool split_subsongs(const in_char* filename, int subsong_index, libvgmstream_t* vgmstream) {
    int playlist_index;
    HWND hPlaylistWindow;


    if (settings.disable_subsongs || vgmstream->format->subsong_count <= 1)
        return 0; /* don't split if no subsongs */
    if (subsong_index > 0 || vgmstream->format->subsong_index > 0)
        return 0; /* no split if already playing subsong */

    hPlaylistWindow = (HWND)SendMessage(input_module.hMainWindow, WM_WA_IPC, IPC_GETWND_PE, IPC_GETWND);
    playlist_index = SendMessage(input_module.hMainWindow,WM_WA_IPC,0,IPC_GETLISTPOS);

    /* The only way to pass info around in Winamp is encoding it into the filename, so a fake name
     * is created with the index. Then, winamp_Play (and related) intercepts and reads the index. */
    for (int i = 0; i < vgmstream->format->subsong_count; i++) {
        in_char stream_fn[WINAMP_PATH_LIMIT];

        make_fn_subsong(stream_fn, WINAMP_PATH_LIMIT, filename, (i+1)); /* encode index in filename */

        /* insert at index */
        {
            COPYDATASTRUCT cds = {0};
            wa_fileinfo f;

            wa_strncpy(f.file, stream_fn,MAX_PATH-1);
            f.file[MAX_PATH-1] = '\0';
            f.index = playlist_index + (i+1);
            cds.dwData = wa_IPC_PE_INSERTFILENAME;
            cds.lpData = (void*)&f;
            cds.cbData = sizeof(wa_fileinfo);
            SendMessage(hPlaylistWindow,WM_COPYDATA,0,(LPARAM)&cds);
        }
        /* IPC_ENQUEUEFILE can pre-set the title without needing the Playlist handle, but can't insert at index */
    }

    /* remove current file from the playlist */
    SendMessage(hPlaylistWindow, WM_WA_IPC, IPC_PE_DELETEINDEX, playlist_index);

    /* autoplay doesn't always advance to the first unpacked track, but manually fails somehow */
    //SendMessage(input_module.hMainWindow,WM_WA_IPC,playlist_index,IPC_SETPLAYLISTPOS);
    //SendMessage(input_module.hMainWindow,WM_WA_IPC,0,IPC_STARTPLAY);

    return 1;
}

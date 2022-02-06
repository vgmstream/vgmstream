/**
 * vgmstream for XMPlay
 */

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <string.h>
#include <ctype.h>

#include "xmpin.h"
#include "../src/vgmstream.h"
#include "../src/plugins.h"


#include "../version.h"
#ifndef VGMSTREAM_VERSION
#define VGMSTREAM_VERSION "unknown version " __DATE__
#endif
#define PLUGIN_NAME  "vgmstream plugin " VGMSTREAM_VERSION
#define PLUGIN_INFO  PLUGIN_NAME " (" __DATE__ ")"


/* ************************************* */

#define SAMPLE_BUFFER_SIZE  1024

/* XMPlay extension list, only needed to associate extensions in Windows */
/*  todo: as of v3.8.2.17, any more than ~1000 will crash XMplay's file list screen (but not using the non-native Winamp plugin...) */
#define EXTENSION_LIST_SIZE   1000 /* (0x2000 * 2) */
#define XMPLAY_MAX_PATH  32768

/* XMPlay function library */
static XMPFUNC_IN *xmpfin;
static XMPFUNC_MISC *xmpfmisc;
static XMPFUNC_FILE *xmpffile;

char working_extension_list[EXTENSION_LIST_SIZE] = {0};
char filepath[MAX_PATH];

/* plugin config */
double fade_seconds = 10.0;
double fade_delay = 0.0;
double loop_count = 2.0;
int ignore_loop = 0;
int disable_subsongs = 1;
BOOL xmplay_doneloop = 0;

/* plugin state */
VGMSTREAM* vgmstream = NULL;
int decode_pos_samples;
int length_samples = 0;
int output_channels;
int current_subsong = 0;
INT16 sample_buffer[SAMPLE_BUFFER_SIZE * VGMSTREAM_MAX_CHANNELS];

//XMPFILE current_file = NULL;
//char current_fn[XMPLAY_MAX_PATH] = {0};

static int shownerror = 0; /* init error */

/* ************************************* */

/* a STREAMFILE that operates via XMPlay's XMPFUNC_FILE+XMPFILE */
typedef struct _XMPLAY_STREAMFILE {
    STREAMFILE sf;          /* callbacks */
    XMPFILE infile;         /* actual FILE */
    char name[PATH_LIMIT];
    off_t offset;           /* current offset */
    int internal_xmpfile;   /* infile was not supplied externally and can be closed */
} XMPLAY_STREAMFILE;

static STREAMFILE* open_xmplay_streamfile_by_xmpfile(XMPFILE file, const char* path, int internal);

static size_t xmpsf_read(XMPLAY_STREAMFILE* sf, uint8_t* dst, offv_t offset, size_t length) {
    size_t read;

    if (sf->offset != offset) {
        if (xmpffile->Seek(sf->infile, offset))
            sf->offset = offset;
        else
            sf->offset = xmpffile->Tell(sf->infile);
    }

    read = xmpffile->Read(sf->infile, dst, length);
    if (read > 0)
        sf->offset += read;

    return read;
}

static off_t xmpsf_get_size(XMPLAY_STREAMFILE* sf) {
    return xmpffile->GetSize(sf->infile);
}

static off_t xmpsf_get_offset(XMPLAY_STREAMFILE* sf) {
    return xmpffile->Tell(sf->infile);
}

static void xmpsf_get_name(XMPLAY_STREAMFILE* sf, char* buffer, size_t length) {
    strncpy(buffer, sf->name, length);
    buffer[length-1] = '\0';
}

static STREAMFILE* xmpsf_open(XMPLAY_STREAMFILE* sf, const char* const filename, size_t buffersize) {
    XMPFILE newfile;

    if (!filename)
        return NULL;

    newfile = xmpffile->Open(filename);
    if (!newfile) return NULL;

    strncpy(filepath, filename, MAX_PATH);
    filepath[MAX_PATH - 1] = 0x00;

    return open_xmplay_streamfile_by_xmpfile(newfile, filename, 1); /* internal XMPFILE */
}

static void xmpsf_close(XMPLAY_STREAMFILE* sf) {
    /* Close XMPFILE, but only if we opened it (ex. for subfiles inside metas).
     * Otherwise must be left open as other parts of XMPlay need it and would crash. */
    if (sf->internal_xmpfile) {
        xmpffile->Close(sf->infile);
    }

    free(sf);
}

static STREAMFILE* open_xmplay_streamfile_by_xmpfile(XMPFILE infile, const char* path, int internal) {
    XMPLAY_STREAMFILE* this_sf = calloc(1, sizeof(XMPLAY_STREAMFILE));
    if (!this_sf) return NULL;

    this_sf->sf.read = (void*)xmpsf_read;
    this_sf->sf.get_size = (void*)xmpsf_get_size;
    this_sf->sf.get_offset = (void*)xmpsf_get_offset;
    this_sf->sf.get_name = (void*)xmpsf_get_name;
    this_sf->sf.open = (void*)xmpsf_open;
    this_sf->sf.close = (void*)xmpsf_close;
    this_sf->infile = infile;
    this_sf->offset = 0;
    strncpy(this_sf->name, path, sizeof(this_sf->name));

    this_sf->internal_xmpfile = internal;

    return &this_sf->sf; /* pointer to STREAMFILE start = rest of the custom data follows */
}

VGMSTREAM* init_vgmstream_xmplay(XMPFILE infile, const char* path, int subsong) {
    STREAMFILE* sf = NULL;
    VGMSTREAM* vgmstream = NULL;

    sf = open_xmplay_streamfile_by_xmpfile(infile, path, 0); /* external XMPFILE */
    if (!sf) return NULL;

    sf->stream_index = subsong;
    vgmstream = init_vgmstream_from_STREAMFILE(sf);
    if (!vgmstream) goto fail;

    return vgmstream;

fail:
    close_streamfile(sf);
    return NULL;
}

/* ************************************* */

static void apply_config(VGMSTREAM* vgmstream) {
    vgmstream_cfg_t vcfg = {0};

    vcfg.allow_play_forever = 0;
  //vcfg.play_forever = loop_forever;
    vcfg.loop_count = loop_count;
    vcfg.fade_time = fade_seconds;
    vcfg.fade_delay = fade_delay;
    vcfg.ignore_loop = ignore_loop;

    vgmstream_apply_config(vgmstream, &vcfg);
}


/* get the tags as an array of "key\0value\0", NULL-terminated */
static char *get_tags(VGMSTREAM * infostream) {
    char* tags;
	int pos = 0;

	tags = xmpfmisc->Alloc(1024);
	memset(tags, 0x00, 1024);

	if (infostream->stream_name != NULL && strlen(infostream->stream_name) > 0)
	{
		memcpy(tags + pos, "title", 5);
		pos += 6;
		memcpy(tags + pos, infostream->stream_name, strlen(infostream->stream_name));
		pos += strlen(infostream->stream_name) + 1;
	}
	
	char* end;
	char* start = NULL;
	int j = 2;
	for (char* i = filepath+strlen(filepath); i > filepath; i--)
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
	for (char* i = filepath; *i != 0; i++)
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

/* Adds ext to XMPlay's extension list. */
static int add_extension(int length, char * dst, const char * ext) {
    int ext_len;
    int i;

    if (length <= 1)
        return 0;

    ext_len = strlen(ext);

    /* check if end reached or not enough room to add */
    if (ext_len+2 > length-2) {
        dst[0]='\0';
        return 0;
    }

    /* copy new extension + null terminate */
    for (i=0; i < ext_len; i++)
        dst[i] = ext[i];
    dst[i]='/';
    dst[i+1]='\0';
    return i+1;
}

/* Creates XMPlay's extension list, a single string with 2 nulls.
 * Extensions must be in this format: "Description\0extension1/.../extensionN" */
static void build_extension_list() {
    const char ** ext_list;
    size_t ext_list_len;
    int i, written;

    written = sprintf(working_extension_list, "%s%c", "vgmstream files",'\0');

    ext_list = vgmstream_get_formats(&ext_list_len);

    for (i=0; i < ext_list_len; i++) {
        written += add_extension(EXTENSION_LIST_SIZE-written, working_extension_list + written, ext_list[i]);
    }
    working_extension_list[written-1] = '\0'; /* remove last "/" */
}

/* ************************************* */

/* info for the "about" button in plugin options */
void WINAPI xmplay_About(HWND win) {
    MessageBox(win,
            PLUGIN_INFO "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm, Nicknine, Thealexbarney, CyberBotX, and many others\n"
            "\n"
            "XMPlay plugin by unknownfile, PSXGamerPro1, kode54, others\n"
            "\n"
            "https://github.com/vgmstream/vgmstream/\n"
            "https://sourceforge.net/projects/vgmstream/ (original)"
            ,"about xmp-vgmstream",MB_OK);
}

#if 0
/* present config options to user (OPTIONAL) */
void WINAPI xmplay_Config(HWND win) {
    /* defined in resource.rc */
    DialogBox(input_module.hDllInstance, (const char *)IDD_CONFIG, win, configDlgProc);
}
#endif

/* quick check if a file is playable by this plugin */
BOOL WINAPI xmplay_CheckFile(const char *filename, XMPFILE file) {
    VGMSTREAM* infostream = NULL;
    if (file)
        infostream = init_vgmstream_xmplay(file, filename, 0);
    else
        infostream = init_vgmstream(filename); //TODO: unicode problems?
    if (!infostream)
        return FALSE;

    close_vgmstream(infostream);

    return TRUE;
}

/* update info from a file, returning the number of subsongs */
DWORD WINAPI xmplay_GetFileInfo(const char *filename, XMPFILE file, float **length, char **tags) {
    VGMSTREAM* infostream;
    int subsong_count;

    if (file)
        infostream = init_vgmstream_xmplay(file, filename, 0);
    else
        infostream = init_vgmstream(filename); //TODO: unicode problems?
    if (!infostream)
        return 0;

    apply_config(infostream);

    //vgmstream_mixing_autodownmix(infostream, downmix_channels);
    vgmstream_mixing_enable(infostream, 0, NULL, NULL);

    if (length && infostream->sample_rate) {
        int length_samples = vgmstream_get_samples(infostream);
        float *lens = (float*)xmpfmisc->Alloc(sizeof(float));
        lens[0] = (float)length_samples / (float)infostream->sample_rate;
        *length = lens;
    }

    subsong_count = infostream->num_streams;
    if (disable_subsongs || subsong_count == 0)
        subsong_count = 1;

    *tags = get_tags(infostream);
    close_vgmstream(infostream);

    return subsong_count;
}

/* open a file for playback, returning:  0=failed, 1=success, 2=success and XMPlay can close the file */
DWORD WINAPI xmplay_Open(const char *filename, XMPFILE file) {
    if (file)
        vgmstream = init_vgmstream_xmplay(file, filename, current_subsong+1);
    else
        vgmstream = init_vgmstream(filename);
    if (!vgmstream)
        return 0;

    apply_config(vgmstream);

    //vgmstream_mixing_autodownmix(vgmstream, downmix_channels);
    vgmstream_mixing_enable(vgmstream, SAMPLE_BUFFER_SIZE, NULL /*&input_channels*/, &output_channels);

    decode_pos_samples = 0;
    length_samples = vgmstream_get_samples(vgmstream);

    //strncpy(current_fn,filename,XMPLAY_MAX_PATH);
    //current_file = file;
    //current_subsong = 0;


    if (length_samples) {
        float length = (float)length_samples / (float)vgmstream->sample_rate;
        xmpfin->SetLength(length, TRUE);
    }

    return 1;
}

/* close the playback file */
void WINAPI xmplay_Close() {
    close_vgmstream(vgmstream);
    vgmstream = NULL;
}

/* set the sample format */
void WINAPI xmplay_SetFormat(XMPFORMAT *form) {
    form->res = 16 / 8; /* PCM 16 */
    form->chan = output_channels;
    form->rate = vgmstream->sample_rate;
}

/* get tags, return NULL to delay title update (OPTIONAL) */
char * WINAPI xmplay_GetTags() {
	return get_tags(vgmstream);
}

/* main panel info text (short file info) */
void WINAPI xmplay_GetInfoText(char* format, char* length) {
    int rate, samples, bps;
    char fmt[128];
    int t, tmin, tsec;

    if (!format)
        return;
    if (!vgmstream)
        return;

    rate = vgmstream->sample_rate;
    samples = vgmstream->num_samples;
    bps = get_vgmstream_average_bitrate(vgmstream) / 1000;
    get_vgmstream_coding_description(vgmstream, fmt, sizeof(fmt));
	if (strcmp(fmt, "FFmpeg") == 0)
	{
		char buffer[1024];
		buffer[0] = '\0';
		describe_vgmstream(vgmstream, buffer, sizeof(buffer));
		char* enc_tmp = strstr(buffer, "encoding: ") + 10;
		char* enc_end = strstr(enc_tmp, "\n");
		int len = (int)enc_end - (int)enc_tmp;

		memset(fmt, 0x00, sizeof(fmt));
		strncpy(fmt, enc_tmp, len);
	}

    t = samples / rate;
    tmin = t / 60;
    tsec = t % 60;

    sprintf(format, "%s", fmt);
    sprintf(length, "%d:%02d - %dKb/s - %dHz", tmin, tsec, bps, rate);
}

/* info for the "General" window/tab (buf is ~40K) */
void WINAPI xmplay_GetGeneralInfo(char* buf) {
    int i;
    char description[1024];

    if (!buf)
        return;
    if (!vgmstream)
        return;

    description[0] = '\0';
    describe_vgmstream(vgmstream,description,sizeof(description));

    /* tags are divided with a tab and lines with carriage return so we'll do some guetto fixin' */
	int tag_done = 0;
	description[0] -= 32; //Replace small letter with capital for consistency with XMPlay's output
    for (i = 0; i < 1024; i++) {
        if (description[i] == '\0')
            break;

        if (description[i] == ':' && !tag_done) { /* to ignore multiple ':' in a line*/
            description[i] = ' ';
            description[i+1] = '\t';
            tag_done = 1;
        }

        if (description[i] == '\n') {
            description[i] = '\r';
			if (description[i+1])
				description[i+1] -= 32; //Replace small letter with capital for consistency with XMPlay's output
            tag_done = 0;
        }
    }

    sprintf(buf,"%s", description);
}

/* get the seeking granularity in seconds */
double WINAPI xmplay_GetGranularity() {
    return 0.001; /* can seek in milliseconds */
}

/* seek to a position (in granularity units), return new position or -1 = failed */
double WINAPI xmplay_SetPosition(DWORD pos) {
    double cpos;
    int32_t seek_sample = (int32_t)(pos * xmplay_GetGranularity() * vgmstream->sample_rate);

    if (pos == XMPIN_POS_AUTOLOOP || pos == XMPIN_POS_LOOP)
        xmplay_doneloop = 1;

#if 0
    /* set a subsong */
    if (!disable_subsongs && (pos & XMPIN_POS_SUBSONG)) {
        int new_subsong = LOWORD(pos);

        /* "single subsong mode (don't show info on other subsongs)" */
        if (pos & XMPIN_POS_SUBSONG1) {
            // ???
        }

        if (new_subsong && new_subsong != current_subsong) { /* todo implicit? */
            if (current_file)
                return -1;

            vgmstream = init_vgmstream_xmplay(current_file, current_fn, current_subsong+1);
            if (!vgmstream) return -1;
            
            current_subsong = new_subsong;
            return 0.0;
        }
    }
#endif

    seek_vgmstream(vgmstream, seek_sample);

    decode_pos_samples = seek_sample;

    cpos = (double)decode_pos_samples / vgmstream->sample_rate;
    return cpos;
}

/* decode some sample data */
DWORD WINAPI xmplay_Process(float* buf, DWORD bufsize) {
    int32_t i, done, samples_to_do;
    BOOL do_loop = xmpfin->GetLooping();
    float *sbuf = buf;


    bufsize /= output_channels;

    samples_to_do = do_loop ? bufsize : length_samples - decode_pos_samples;
    if (samples_to_do > bufsize)
        samples_to_do = bufsize;

    /* decode */
    done = 0;
    while (done < samples_to_do) {
        int to_do = SAMPLE_BUFFER_SIZE;
        if (to_do > samples_to_do - done)
            to_do = samples_to_do - done;

        render_vgmstream(sample_buffer, to_do, vgmstream);

        for (i = 0; i < to_do * output_channels; i++) {
            *sbuf++ = sample_buffer[i] * 1.0f / 32768.0f;
        }

        done += to_do;
    }

    sbuf = buf;

    decode_pos_samples += done;

    return done * output_channels;
}

static DWORD WINAPI xmplay_GetSubSongs(float *length) {
    int subsong_count;

    if (!vgmstream)
        return 0;

    subsong_count = vgmstream->num_streams;
    if (disable_subsongs || subsong_count == 0)
        subsong_count = 1;

    /* get times for all subsongs */
    //todo request updating playlist update every subsong change instead?
    {
        /* not good for vgmstream as would mean re-parsing many times */
        //int i;
        //*length = 0;
        //for (i = 0; i < subsong_count; i++) {
        //    float subsong_length = ...
        //    *length += subsong_length;
        //}

        /* simply use the current length */ //todo just use 0?
        length_samples = vgmstream_get_samples(vgmstream);
        *length = (float)length_samples / (float)vgmstream->sample_rate;
    }

    return subsong_count;
}

/* *********************************** */

/* main plugin def, see xmpin.h */
XMPIN vgmstream_xmpin = {
    XMPIN_FLAG_CANSTREAM,
    "vgmstream for XMPlay",
    working_extension_list,
    xmplay_About,
    NULL,//XMP_Config
    xmplay_CheckFile,
    xmplay_GetFileInfo,
    xmplay_Open,
    xmplay_Close,
    NULL,
    xmplay_SetFormat,
    xmplay_GetTags, //(OPTIONAL) --actually mandatory
    xmplay_GetInfoText,
    xmplay_GetGeneralInfo,
    NULL,//GetMessage - text for the "Message" tab window/tab (OPTIONAL)
    xmplay_SetPosition,
    xmplay_GetGranularity,
    NULL,
    xmplay_Process,
    NULL,
    NULL,
    xmplay_GetSubSongs,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

/* get the plugin's XMPIN interface */
__declspec(dllexport) XMPIN* WINAPI  XMPIN_GetInterface(UINT32 face, InterfaceProc faceproc) {
    if (face != XMPIN_FACE) {
        // unsupported version
        if (face < XMPIN_FACE && !shownerror) {
            MessageBox(0,
                    "The xmp-vgmstream plugin requires XMPlay 3.8 or above.\n\n"
                    "Please update at:\n"
                    "http://www.un4seen.com/xmplay.html\n"
                    "http://www.un4seen.com/stuff/xmplay.exe", 0, MB_ICONEXCLAMATION);
            shownerror = 1;
        }
        return NULL;
    }

    xmpfin = (XMPFUNC_IN*)faceproc(XMPFUNC_IN_FACE);
    xmpfmisc = (XMPFUNC_MISC*)faceproc(XMPFUNC_MISC_FACE);
    xmpffile = (XMPFUNC_FILE*)faceproc(XMPFUNC_FILE_FACE);

    build_extension_list();

    return &vgmstream_xmpin;
}

#if 0
// needed?
BOOL WINAPI DllMain(HINSTANCE hDLL, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hDLL);
            break;
    }
    return TRUE;
}
#endif

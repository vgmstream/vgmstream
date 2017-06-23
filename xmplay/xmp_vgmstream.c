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

#include "../src/formats.h"
#include "../src/vgmstream.h"
#include "xmpin.h"


#ifndef VERSION
#include "../version.h"
#endif
#ifndef VERSION
#define VERSION "(unknown version)"
#endif

static XMPFUNC_IN *xmpfin;
static XMPFUNC_MISC *xmpfmisc;
static XMPFUNC_FILE *xmpffile;

/* ************************************* */

typedef struct _XMPSTREAMFILE {
	STREAMFILE sf;
	XMPFILE file;
	off_t offset;
	char name[PATH_LIMIT];
} XMPSTREAMFILE;

static void xmpsf_seek(XMPSTREAMFILE *this, off_t offset) {
	if (xmpffile->Seek(this->file, offset))
		this->offset = offset;
	else
		this->offset = xmpffile->Tell(this->file);
}

static off_t xmpsf_get_size(XMPSTREAMFILE *this)
{
	return xmpffile->GetSize(this->file);
}

static off_t xmpsf_get_offset(XMPSTREAMFILE *this)
{
	return xmpffile->Tell(this->file);
}

static void xmpsf_get_name(XMPSTREAMFILE *this, char *buffer, size_t length)
{
	strncpy(buffer, this->name, length);
	buffer[length - 1] = '\0';
}

static size_t xmpsf_read(XMPSTREAMFILE *this, uint8_t *dest, off_t offset, size_t length)
{
	size_t read;
	if (this->offset != offset)
		xmpsf_seek(this, offset);
	read = xmpffile->Read(this->file, dest, length);
	if (read > 0)
		this->offset += read;
	return read;
}

static void xmpsf_close(XMPSTREAMFILE *this)
{
    // The line below is what Causes this Plugin to Crash. Credits to Ian Luck to Finding this issue.
    // This closes the internal XMPFILE, which must be done by XMPlay instead.
    // However vgmtream sometimes opens its own files, so it may be leaking handles.
    //xmpffile->Close(this->file);

	free(this);
}

static STREAMFILE *xmpsf_create_from_path(const char *path);
static STREAMFILE *xmpsf_open(XMPSTREAMFILE *this, const char *const filename, size_t buffersize)
{
	if (!filename) return NULL;
	return xmpsf_create_from_path(filename);
}

static STREAMFILE *xmpsf_create(XMPFILE file, const char *path)
{
	XMPSTREAMFILE *streamfile = malloc(sizeof(XMPSTREAMFILE));

	if (!streamfile) return NULL;

	memset(streamfile, 0, sizeof(XMPSTREAMFILE));
	streamfile->sf.read = (void*)xmpsf_read;
	streamfile->sf.get_size = (void*)xmpsf_get_size;
	streamfile->sf.get_offset = (void*)xmpsf_get_offset;
	streamfile->sf.get_name = (void*)xmpsf_get_name;
	streamfile->sf.get_realname = (void*)xmpsf_get_name;
	streamfile->sf.open = (void*)xmpsf_open;
	streamfile->sf.close = (void*)xmpsf_close;
	streamfile->file = file;
	streamfile->offset = 0;
	strncpy(streamfile->name, path, sizeof(streamfile->name));

	return &streamfile->sf;
}

STREAMFILE *xmpsf_create_from_path(const char *path)
{
	XMPFILE file = xmpffile->Open(path);
	if (!file)
		return NULL;

	return xmpsf_create(file, path);
}

// Probably not needed at all.
//VGMSTREAM *init_vgmstream_from_path(const char *path)
//{
//	STREAMFILE *sf;
//	VGMSTREAM *vgm;
//
//	sf = xmpsf_create_from_path(path);
//	if (!sf) return NULL;
//
//	vgm = init_vgmstream_from_STREAMFILE(sf);
//	if (!vgm) goto err1;
//
//	return vgm;
//
//err1:
//	xmpsf_close((XMPSTREAMFILE *)sf);
//	return NULL;
//}

VGMSTREAM *init_vgmstream_from_xmpfile(XMPFILE file, const char *path)
{
	STREAMFILE *sf;
	VGMSTREAM *vgm;

	sf = xmpsf_create(file, path);
	if (!sf) return NULL;

	vgm = init_vgmstream_from_STREAMFILE(sf);
	if (!vgm) goto err1;

	return vgm;

err1:
	xmpsf_close((XMPSTREAMFILE *)sf);
	return NULL;
}

/* ************************************* */

/* internal state */
VGMSTREAM * vgmstream = NULL;
int32_t totalFrames, framesDone, framesLength, framesFade;

#define APP_NAME "vgmstream plugin" //unused?

void __stdcall XMP_About(HWND hwParent) {
    MessageBox(hwParent,
            "vgmstream plugin " VERSION " " __DATE__ "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, snakemeat, soneek, kode54, bnnm and many others\n"
            "\n"
            "XMPlay plugin by unknownfile, PSXGamerPro1, kode54\n"
            "\n"
            "https://github.com/kode54/vgmstream/\n"
            "https://sourceforge.net/projects/vgmstream/ (original)"
            ,"about xmp-vgmstream",MB_OK);
}

void __stdcall XMP_Close() {
	close_vgmstream(vgmstream);
	vgmstream = NULL;
}

BOOL __stdcall XMP_CheckFile(const char *filename, XMPFILE file) {
	VGMSTREAM* thisvgmstream;
	int ret;
	if (file) thisvgmstream = init_vgmstream_from_xmpfile(file, filename);
	else thisvgmstream = init_vgmstream(filename);

	if (!thisvgmstream) ret = FALSE;
	else { ret = TRUE; close_vgmstream(thisvgmstream); }

	return ret;
}

DWORD __stdcall XMP_GetFileInfo(const char *filename, XMPFILE file, float **length, char **tags)
{
	VGMSTREAM* thisvgmstream;
	if (file) thisvgmstream = init_vgmstream_from_xmpfile(file, filename);
	else thisvgmstream = init_vgmstream(filename);
	if (!thisvgmstream) return 0;

	if (length && thisvgmstream->sample_rate)
	{
		int totalFrames = get_vgmstream_play_samples(2.0, 10.0, 10.0, thisvgmstream);
		float *lens = (float*)xmpfmisc->Alloc(sizeof(float));
		lens[0] = (float)totalFrames / (float)thisvgmstream->sample_rate;
		*length = lens;
	}

	close_vgmstream(thisvgmstream);

	return 1;
}

DWORD __stdcall XMP_Open(const char *filename, XMPFILE file) {
	if (file) vgmstream = init_vgmstream_from_xmpfile(file, filename);
	else vgmstream = init_vgmstream(filename);
 
	if (!vgmstream) return 0;

	totalFrames = get_vgmstream_play_samples(2.0, 10.0, 10.0, vgmstream);
	framesDone = 0;
	framesFade = vgmstream->sample_rate * 10;
	framesLength = totalFrames - framesFade;

	if (totalFrames)
	{
		float length = (float)totalFrames / (float)vgmstream->sample_rate;
		xmpfin->SetLength(length, TRUE);
	}

	return 1;
}

DWORD __stdcall XMP_Process(float* buffer, DWORD bufsize) {
	INT16 buf[1024];
	UINT32 i, j, todo, done;

	BOOL doLoop = xmpfin->GetLooping();

	float *sbuf = buffer;

	UINT32 samplesTodo;

	bufsize /= vgmstream->channels;

	samplesTodo = doLoop ? bufsize : totalFrames - framesDone;
	if (samplesTodo > bufsize)
		samplesTodo = bufsize;

	done = 0;
	while (done < samplesTodo) {
		todo = 1024 / vgmstream->channels;
		if (todo > samplesTodo - done)
			todo = samplesTodo - done;
		render_vgmstream(buf, todo, vgmstream);
		for (i = 0, j = todo * vgmstream->channels; i < j; ++i)
		{
			*sbuf++ = buf[i] * 1.0f / 32768.0f;
		}
		done += todo;
	}

	sbuf = buffer;

	if (!doLoop && framesDone + done > framesLength)
	{
		long fadeStart = (framesLength > framesDone) ? framesLength : framesDone;
		long fadeEnd = (framesDone + done) > totalFrames ? totalFrames : (framesDone + done);
		long fadePos;

		float fadeScale = (float)(totalFrames - fadeStart) / framesFade;
		float fadeStep = 1.0f / framesFade;
		sbuf += (fadeStart - framesDone) * vgmstream->channels;
		j = vgmstream->channels;
		for (fadePos = fadeStart; fadePos < fadeEnd; ++fadePos)
		{
			for (i = 0; i < j; ++i)
			{
				sbuf[i] = sbuf[i] * fadeScale;
			}
			sbuf += j;
			fadeScale -= fadeStep;
			if (fadeScale <= 0.0f) break;
		}
		done = (int)(fadePos - framesDone);
	}

	framesDone += done;

	return done * vgmstream->channels;
}

void __stdcall XMP_SetFormat(XMPFORMAT *form) {
	form->res = 16 / 8;
	form->chan = vgmstream->channels;
	form->rate = vgmstream->sample_rate;
}

char * __stdcall XMP_GetTags()
{
	return NULL;
}

double __stdcall XMP_GetGranularity()
{
	return 0.001;
}

double __stdcall XMP_SetPosition(DWORD pos) {
	double cpos = (double)framesDone / (double)vgmstream->sample_rate;
	double time = pos * XMP_GetGranularity();

	if (time < cpos)
	{
		reset_vgmstream(vgmstream);
		cpos = 0.0;
	}

	while (cpos < time)
	{
		INT16 buffer[1024];
		long max_sample_count = 1024 / vgmstream->channels;
		long samples_to_skip = (long)((time - cpos) * vgmstream->sample_rate);
		if (samples_to_skip > max_sample_count)
			samples_to_skip = max_sample_count;
		if (!samples_to_skip)
			break;
		render_vgmstream(buffer, (int)samples_to_skip, vgmstream);
		cpos += (double)samples_to_skip / (double)vgmstream->sample_rate;
	}

	framesDone = (int32_t)(cpos * vgmstream->sample_rate);

	return cpos;
}

/* main panel info text (short file info) */
void __stdcall XMP_GetInfoText(char* format, char* length) {
    if (!format)
	    return;

	sprintf(format,"vgmstream");
	/* length is the file time */
}

/* info for the "General" window/tab (buf is ~40K) */
void __stdcall XMP_GetGeneralInfo(char* buf) {
    int i;
    char description[1024];

    if (!buf)
        return;
    if (!vgmstream)
        return;

    description[0] = '\0';
    describe_vgmstream(vgmstream,description,sizeof(description));

    /* tags are divided with a tab and lines with carriage return so we'll do some guetto fixin' */
    for (i = 0; i < 1024; i++) {
        int tag_done = 0;

        if (description[i] == '\0')
            break;

        if (description[i] == ':' && !tag_done) { /* to ignore multiple ':' in a line*/
            description[i] = ' ';
            description[i+1] = '\t';
            tag_done = 1;
        }

        if (description[i] == '\n') {
            description[i] = '\r';
            tag_done = 0;
        }
    }

    sprintf(buf,"vgmstream\t\r%s\r", description);
}


static int add_extension(int length, char * dst, const char * src);
static void build_extension_list();

/* XMPlay extension list, only needed to associate extensions in Windows */
/*  todo: as of v3.8.2.17, any more than ~1000 will crash XMplay's file list screen (but not using the non-native Winamp plugin...) */
#define EXTENSION_LIST_SIZE   1000 /*VGM_EXTENSION_LIST_CHAR_SIZE * 2*/
char working_extension_list[EXTENSION_LIST_SIZE] = {0};

/* plugin defs, see xmpin.h */
XMPIN vgmstream_intf = {
	XMPIN_FLAG_CANSTREAM,
	"vgmstream for XMPlay",
	working_extension_list,
	XMP_About,
	NULL,//XMP_Config
	XMP_CheckFile,
	XMP_GetFileInfo,
	XMP_Open,
	XMP_Close,
	NULL,
	XMP_SetFormat,
	XMP_GetTags, //(OPTIONAL) --actually mandatory
	XMP_GetInfoText,
	XMP_GetGeneralInfo,
	NULL,//GetMessage - text for the "Message" tab window/tab (OPTIONAL)
	XMP_SetPosition,
	XMP_GetGranularity,
	NULL,
	XMP_Process,
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
	NULL,
	NULL,
	NULL,
};

static int shownerror = 0;

__declspec(dllexport) XMPIN* __stdcall XMPIN_GetInterface(UINT32 face, InterfaceProc faceproc)
{
	if (face != XMPIN_FACE)
	{ // unsupported version
		if (face<XMPIN_FACE && !shownerror)
		{
			//Replaced the message box below with a better one.
			//MessageBox(0, "The XMP-vgmstream plugin requires XMPlay 3.8 or above", 0, MB_ICONEXCLAMATION);
			MessageBox(0, "The xmp-vgmstream plugin requires XMPlay 3.8 or above. Please update at.\n\n http://www.un4seen.com/xmplay.html \n or at\n http://www.un4seen.com/stuff/xmplay.exe.", 0, MB_ICONEXCLAMATION);
			shownerror = 1;
		}
		return NULL;
	}

	xmpfin = (XMPFUNC_IN*)faceproc(XMPFUNC_IN_FACE);
	xmpfmisc = (XMPFUNC_MISC*)faceproc(XMPFUNC_MISC_FACE);
	xmpffile = (XMPFUNC_FILE*)faceproc(XMPFUNC_FILE_FACE);

    build_extension_list();

    return &vgmstream_intf;
}


/**
 * Creates XMPlay's extension list, a single string with 2 nulls.
 * Extensions must be in this format: "Description\0extension1/.../extensionN"
 */
static void build_extension_list() {
    const char ** ext_list;
    int ext_list_len;
    int i, written;

    written = sprintf(working_extension_list, "%s%c", "vgmstream files",'\0');

    ext_list = vgmstream_get_formats();
    ext_list_len = vgmstream_get_formats_length();

    for (i=0; i < ext_list_len; i++) {
        written += add_extension(EXTENSION_LIST_SIZE-written, working_extension_list + written, ext_list[i]);
    }
    working_extension_list[written-1] = '\0'; /* remove last "/" */
}

/**
 * Adds ext to XMPlay's extension list.
 */
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

/*
** vgmstream for XMPlay
**
** 11/11/2009 - started. this is hilariously buggy and doesnt support much yet. [unknownfile]
*/

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <io.h>
#include <conio.h>

#include ".\src\vgmstream.h"
#include ".\src\util.h"
#include "xmpin.h"
#include "version.h"


#ifndef VERSION
#define VERSION
#endif

static XMPFUNC_IN *xmpfin;
static XMPFUNC_MISC *xmpfmisc;
static XMPFUNC_FILE *xmpffile;

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
//	xmpffile->Close(this->file); 
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

VGMSTREAM * vgmstream = NULL;
int32_t totalFrames, framesDone, framesLength, framesFade;

#define APP_NAME "vgmstream plugin"
#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__

void __stdcall XMPAbout(HWND hwParent) {
    MessageBox(hwParent,
            PLUGIN_DESCRIPTION "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, kode54, and PSXGamerPro1\n\n"
			"https://gitlab.kode54.net/kode54/vgmstream"
            ,"about xmp-vgmstream",MB_OK);
}

void __stdcall Stop() {
	close_vgmstream(vgmstream);
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

DWORD __stdcall LoadVgmStream(const char *filename, XMPFILE file) {
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

DWORD __stdcall XMP_Buffer(float* buffer, DWORD bufsize) {
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

void __stdcall XMP_GetInfoText(char* txt, char* length) {
	if (txt)
		sprintf(txt,"vgmstream!");
}

void __stdcall GetAdditionalFields(char* blerp) {
	sprintf(blerp,"oh god how did this get here I am not good with computers\n");
}

XMPIN vgmstream_intf = {
	XMPIN_FLAG_CANSTREAM,
	"vgmstream for XMPlay",
	"vgmstream files\0""2dx9/aaap/aax/acm/adp/adpcm/ads/adx/afc/agsc/ahx/aifc/aiff/aix/amts/as4/asd/asf/asr/ass/ast/aud/aus/baf/baka/bar/bcstm/bcwav/bfstm/bfwav/bfwavnsmbu/bg00/bgw/bh2pcm/bmdx/bns/bnsf/bo2/brstm/caf/capdsp/ccc/cfn/cnk/dcs/dcsw/ddsp/de2/dmsg/dsp/dvi/dxh/eam/emff/enth/fag/filp/fsb/fwav/gca/gcm/gcsw/gcw/genh/gms/gsp/hca/hgc1/his/hps/hwas/idsp/idvi/ikm/ild/int/isd/ish/ivaud/ivb/joe/kces/kcey/khv/kraw/leg/logg/lps/lsf/lwav/matx/mcg/mi4/mib/mic/mihb/mpdsp/msa/mss/msvp/mus/musc/musx/mwv/myspd/ndp/npsf/nus3bank/nwa/omu/otm/p3d/pcm/pdt/pnb/pos/psh/psw/raw/rkv/rnd/rrds/rsd/rsf/rstm/rwar/rwav/rws/rwsd/rwx/rxw/s14/sab/sad/sap/sc/scd/sd9/sdt/seg/sfl/sfs/sl3/sli/smp/smpl/snd/sng/sns/spd/sps/spsd/spt/spw/ss2/ss7/ssm/sss/ster/sth/stm/stma/str/strm/sts/stx/svag/svs/swav/swd/tec/thp/tk5/tydsp/um3/vag/vas/vgs/vig/vjdsp/voi/vpk/vs/vsf/waa/wac/wad/wam/was/wavm/wb/wii/wp2/wsd/wsi/wvs/xa/xa2/xa30/xmu/xss/xvas/xwav/xwb/ydsp/ymf/zsd/zwdsp/vgmstream/vgms",
	XMPAbout,
	NULL,
	XMP_CheckFile,
	XMP_GetFileInfo,
	LoadVgmStream,
	Stop,
	NULL,
	XMP_SetFormat,
	XMP_GetTags, // Actually mandatory
	XMP_GetInfoText,
	GetAdditionalFields,
	NULL,
	XMP_SetPosition,
	XMP_GetGranularity,
	NULL,
	XMP_Buffer,
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

	return &vgmstream_intf;
}

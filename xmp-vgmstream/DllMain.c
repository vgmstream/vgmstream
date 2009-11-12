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

#include "../src/vgmstream.h"
#include "../src/util.h"
#include "xmp_in.h"


#ifndef VERSION
#define VERSION
#endif

VGMSTREAM * vgmstream = NULL;
int Decoder_Is_Using_Lame_Hack = 0; 
double timestamp=0;

#define DECODE_SIZE		1024
#define APP_NAME "vgmstream plugin"
#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__

void __stdcall XMPAbout() {
    MessageBox(NULL,
            PLUGIN_DESCRIPTION "\n"
            "by hcs, FastElbja, manakoAT, and bxaimc\n\n"
            "http://sourceforge.net/projects/vgmstream"
            ,"about in_vgmstream",MB_OK);
}

void __stdcall Stop() {
	close_vgmstream(vgmstream);
}

int __stdcall XMP_CheckFile(char *filename, BYTE *buf, DWORD length) {
    VGMSTREAM* fakevgmstream = init_vgmstream(filename);
    int ret;

	if (!fakevgmstream) ret = 0;
	else { ret = 1; close_vgmstream(fakevgmstream); }

	return ret;
}

// part of our hugeass hack to stop things from crashing
static INT32 valid_freqs[5] = {
	11025,
	22050,
	32000,
	44100,
	48000,
};

int __stdcall LoadVgmStream(char *filename, XMPFILE file) {
	int i;
	vgmstream = init_vgmstream(filename);
 
	if (!vgmstream) return 0;
	 // just loop forever till we have configuration done

	Decoder_Is_Using_Lame_Hack = 0;

	for (i = 0; i < 5; i ++) {
		if (vgmstream->sample_rate == valid_freqs[i]) {
			Decoder_Is_Using_Lame_Hack = 1;
			break;
		}
	}

	return 1;
}

int __stdcall XMP_Buffer(float* buffer, UINT32 bufsize) {
	int i,x,y;
	int adder = vgmstream->channels * (Decoder_Is_Using_Lame_Hack ? 2 : 1);

	/*
	** Algorithm for correct playback.
	** This is in fact a huge-ass hack which is terrible and crashes tracks with sample rates
	** it doesn't like. We'll need to fix this later
	*/


	for (i=0;i<bufsize;i+=adder) {
		INT16 buf[16];
		memset(buf,0,16 * 2);

		render_vgmstream(buf,vgmstream->channels,vgmstream);
		for (x=0;x<adder;x++) {	
			for (y=0;y<(Decoder_Is_Using_Lame_Hack ? 2 : 1);y++)
				*(buffer + i + x + y) = (float)(buf[x]) / 22050; // This divide is required, audio is REALLY LOUD otherwise
		}
	}

	return bufsize;
}

DWORD __stdcall XMP_GetFormat(DWORD *chan, DWORD *res) {
	*(chan) = vgmstream->channels;

	if (Decoder_Is_Using_Lame_Hack) return vgmstream->sample_rate;
	else return vgmstream->sample_rate / 2;
}

void __stdcall XMP_GetInfoText(char* txt, char* length) {
	sprintf(txt,"vgmstream!");
}

void __stdcall GetAdditionalFields(char* blerp) {
	sprintf(blerp,"oh god how did this get here i am not good with computers\n");
}


double __stdcall GetDecodePosition() {
	return timestamp;
}

// Trap functions to catch stuff we haven't implemented.
#define TRAP_FCN(id) void* __stdcall trap_##id() {\
		cprintf("trap %d hit\n",id);\
		return NULL;\
}
TRAP_FCN(1);
TRAP_FCN(2);
TRAP_FCN(3);
TRAP_FCN(4);
TRAP_FCN(5);
TRAP_FCN(6);
TRAP_FCN(7);
TRAP_FCN(8);
TRAP_FCN(9);
TRAP_FCN(10);
TRAP_FCN(11);
TRAP_FCN(12);
TRAP_FCN(13);
TRAP_FCN(14);
TRAP_FCN(15);
TRAP_FCN(16);
TRAP_FCN(17);
TRAP_FCN(18);
TRAP_FCN(19);
TRAP_FCN(20);
TRAP_FCN(21);
TRAP_FCN(22);

int __stdcall XMP_GetSubSongs() {
	return 1; //
}

XMPIN vgmstream_intf = {
	0,
	"vgmstream for XMplay",
	"vgmstream files\0brstm",
	XMPAbout,
	NULL,
	XMP_CheckFile,
	NULL,
	LoadVgmStream,
	Stop,
	XMP_GetFormat,
	NULL,
	NULL,
	XMP_GetInfoText,
	GetAdditionalFields,
	NULL,
	NULL,
	GetDecodePosition,
	NULL,//trap_18,
	XMP_Buffer,
	NULL,
	NULL,
	XMP_GetSubSongs,
};


__declspec(dllexport) XMPIN* XMPIN_GetInterface(UINT32 version, void* ifproc) {
	AllocConsole();
	if (version != 1) return NULL;
	return &vgmstream_intf;
}
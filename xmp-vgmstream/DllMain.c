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

int __stdcall LoadVgmStream(char *filename, XMPFILE file) {
	vgmstream = init_vgmstream(filename);
 
	if (!vgmstream) return 0;
	 // just loop forever till we have configuration done

	return 1;
}

int __stdcall XMP_Buffer(float* buffer, UINT32 bufsize) {
	int i;

	// Quick way of converting to float
	for (i=0;i<bufsize;i+=vgmstream->channels) {
		INT16 buf[16];
		render_vgmstream(buf,vgmstream->channels,vgmstream);
		*(buffer + i) = buf[0];
		if (vgmstream->channels == 2) *(buffer + i + 1) = buf[1];
	}

	return bufsize;
}

DWORD __stdcall XMP_GetFormat(DWORD *chan, DWORD *res) {
	*(chan) = vgmstream->channels;
	return vgmstream->sample_rate / 2;
}

void __stdcall FormatPlayWindowText(char* txt) {
	strcpy(txt,"vgmstream!");
}

void __stdcall GetAdditionalFields(char* blerp) {
	strcpy(blerp,"oh god how did this get here i am not good with computers\n");
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

int __stdcall Call22() {
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
	FormatPlayWindowText,
	GetAdditionalFields,
	NULL,
	NULL,
	GetDecodePosition,
	NULL,//trap_18,
	XMP_Buffer,
	NULL,
	NULL,
	Call22,
};


__declspec(dllexport) XMPIN* XMPIN_GetInterface(UINT32 version, void* ifproc) {
	AllocConsole();
	if (version != 1) return NULL;
	return &vgmstream_intf;
}
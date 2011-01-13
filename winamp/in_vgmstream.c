/* Winamp plugin interface for vgmstream */
/* Based on: */
/*
** Example Winamp .RAW input plug-in
** Copyright (c) 1998, Justin Frankel/Nullsoft Inc.
*/

#ifdef _MSC_VER
#define _CRT_SECURE_NO_DEPRECATE
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <io.h>

#include "../src/vgmstream.h"
#include "../src/util.h"
#include "in2.h"
#include "wa_ipc.h"
#include "resource.h"

#ifndef VERSION
#define VERSION
#endif

#define APP_NAME "vgmstream plugin"
#define PLUGIN_DESCRIPTION "vgmstream plugin " VERSION " " __DATE__
#define INI_NAME "plugin.ini"

/* post when playback stops */
#define WM_WA_MPEG_EOF WM_USER+2

In_Module input_module; /* the input module, declared at the bottom of this file */
DWORD WINAPI __stdcall decode(void *arg);

char lastfn[MAX_PATH+1] = {0}; /* name of the currently playing file */
short sample_buffer[576*2*2]; /* 576 16-bit samples, stereo, possibly doubled in size for DSP */

#define DEFAULT_FADE_SECONDS "10.00"
#define DEFAULT_FADE_DELAY_SECONDS "0.00"
#define DEFAULT_LOOP_COUNT "2.00"
#define DEFAULT_THREAD_PRIORITY 3
#define DEFAULT_LOOP_FOREVER 0
#define DEFAULT_IGNORE_LOOP 0

#define FADE_SECONDS_INI_ENTRY "fade_seconds"
#define FADE_DELAY_SECONDS_INI_ENTRY "fade_delay"
#define LOOP_COUNT_INI_ENTRY "loop_count"
#define THREAD_PRIORITY_INI_ENTRY "thread_priority"
#define LOOP_FOREVER_INI_ENTRY "loop_forever"
#define IGNORE_LOOP_INI_ENTRY "ignore_loop"

double fade_seconds;
double fade_delay_seconds;
double loop_count;
int thread_priority;
int loop_forever;
int ignore_loop;

char *priority_strings[] = {"Idle","Lowest","Below Normal","Normal","Above Normal","Highest (not recommended)","Time Critical (not recommended)"};
int priority_values[] = {THREAD_PRIORITY_IDLE,THREAD_PRIORITY_LOWEST,THREAD_PRIORITY_BELOW_NORMAL,THREAD_PRIORITY_NORMAL,THREAD_PRIORITY_ABOVE_NORMAL,THREAD_PRIORITY_HIGHEST,THREAD_PRIORITY_TIME_CRITICAL};

VGMSTREAM * vgmstream = NULL;
HANDLE decode_thread_handle = INVALID_HANDLE_VALUE;
int paused = 0;
int decode_abort = 0;
int seek_needed_samples = -1;
int decode_pos_ms = 0;
int decode_pos_samples = 0;
int stream_length_samples = 0;
int fade_samples = 0;

#define EXTENSION_LIST_SIZE 10240
char working_extension_list[EXTENSION_LIST_SIZE] = {0};
char * extension_list[] = {

    "2dx9\0""2DX9 Audio File (*.2DX9)\0",

    "aax\0AAX Audio File (*.AAX)\0",
    "aaap\0AAAP Audio File (*.AAAP)\0",
    "aax\0AAX Audio File (*.AAX)\0",
    "acm\0ACM Audio File (*.ACM)\0",
    "adm\0ADM Audio File (*.ADM)\0",
    "adpcm\0ADPCM Audio File (*.ADPCM)\0",
    "adp\0ADP Audio File (*.ADP)\0",
    "ads\0PS2 ADS Audio File (*.ADS)\0",
    "adx\0ADX Audio File (*.ADX)\0",
    "afc\0AFC Audio File (*.AFC)\0",
    "agsc\0AGSC Audio File (*.AGSC)\0",
    "ahx\0AHX Audio File (*.AHX)\0",
    "aifc\0AIFC Audio File (*.AIFC)\0",
    "aix\0AIX Audio File (*.AIX)\0",
    "amts\0AMTS Audio File (*.AMTS)\0",
    "as4\0AS4 Audio File (*.AS4)\0",
    "asd\0ASD Audio File (*.ASD)\0",
    "asf\0ASF Audio File (*.ASF)\0",
    "ast\0AST Audio File (*.AST)\0",
    "asr\0ASR Audio File (*.ASR)\0",
    "ass\0ASS Audio File (*.ASS)\0",
    "aud\0AUD Audio File (*.AUD)\0",    
    "aus\0AUS Audio File (*.AUS)\0",    

    "b1s\0B1S Audio File (*.B1S)\0",
    "baka\0BAKA Audio File (*.BAKA)\0",
    "baf\0BAF Audio File (*.BAF)\0",
    "bar\0BAR Audio File (*.BAR)\0",
    "bdsp\0BDSP Audio File (*.BDSP)\0",
    "bg00\0BG00 Audio File (*.BG00)\0",
    "bgw\0BGW Audio File (*.BGW)\0",
    "bh2pcm\0BH2PCM Audio File (*.BH2PCM)\0",
    "bmdx\0BMDX Audio File (*.BMDX)\0",
    "bns\0BNS Audio File (*.BNS)\0",
    "bnsf\0BNSF Audio File (*.BNSF)\0",
    "bo2\0BO2 Audio File (*.BO2)\0",
    "brstm;brstmspm\0BRSTM Audio File (*.BRSTM)\0",

    "caf\0CAF Audio File (*.CAF)\0",
    "capdsp\0CAPDSP Audio File (*.CAPDSP)\0", /* Capcom custom coefs */
    "cbd2\0CBD2 Audio File (*.CBD2)\0",
    "ccc\0CCC Audio File (*.CCC)\0",
    "cfn\0CFN Audio File (*.CFN)\0",
    "cnk\0CNK Audio File (*.CNK)\0",
	"cps\0CPS Audio File (*.CPS)\0",

    "dcs\0DCS Audio File (*.DCS)\0",
    "de2\0DE2 Audio File (*.DE2)\0",
    "ddsp\0DDSP Audio File (*.DDSP)\0",
    "dmsg\0DMSG Audio File (*.DMSG)\0",
    "dsp\0DSP Audio File (*.DSP)\0",
    "dspw\0DSPW Audio File (*.DSPW)\0",
    "dtk\0DTK Audio File (*.DTK)\0",
    "dvi\0DVI Audio File (*.DVI)\0",
    "dxh\0DXH Audio File (*.DXH)\0",

    "eam\0EAM Audio File (*.EAM)\0",
    "emff\0EMFF Audio File (*.EMFF)\0",
    "enth\0ENTH Audio File (*.ENTH)\0",

    "fag\0FAG Audio File (*.FAG)\0",
    "ffw\0FFW Audio File (*.FFW)\0",
    "filp\0FILP Audio File (*.FILP)\0",
    "fsb\0FSB Audio File (*.FSB)\0",

    "gbts\0GBTS Audio File (*.GBTS)\0",
    "gca\0GCA Audio File (*.GCA)\0",
    "gcm\0GCM Audio File (*.GCM)\0",
    "gcub\0GCUB Audio File (*.GCUB)\0",
    "gcw\0GCW Audio File (*.GCW)\0",
    "genh\0GENH Audio File (*.GENH)\0",
    "gms\0GMS Audio File (*.GMS)\0",
    "gsb\0GSB Audio File (*.GSB)\0",

    "hgc1\0HGC1 Audio File (*.HGC1)\0",
    "his\0HIS Audio File (*.HIS)\0",
    "hlwav\0HLWAV Audio File (*.HLWAV)\0",
    "hps\0HALPST Audio File (*.HPS)\0",
    "hwas\0HWAS Audio File (*.HWAS)\0",

    "iab\0IAB Audio File (*.IAB)\0",
	"iadp\0IADP Audio File (*.IADP)\0",
    "idsp\0IDSP Audio File (*.IDSP)\0",
    "idvi\0IDVI Audio File (*.IDVI)\0",
    "ikm\0IKM Audio File (*.IKM)\0",
    "ild\0ILD Audio File (*.ILD)\0",
    "int\0PS2 RAW Interleaved PCM (*.INT)\0",
    "isd\0ISD Audio File (*.ISD)\0",
    "isws\0ISWS Audio File (*.ISWS)\0",
    "ivaud\0IVAUD Audio File (*.IVAUD)\0",
    "ivb\0IVB Audio File (*.IVB)\0",

    "joe\0JOE Audio File (*.JOE)\0",
    "jstm\0JSTM Audio File (*.JSTM)\0",

    "kces\0KCES Audio File (*.KCES)\0",
    "kcey\0KCEY Audio File (*.KCEY)\0",
    "khv\0KHV Audio File (*.KHV)\0",
    "kovs\0KOVS Audio File (*.KOVS)\0",
    "kraw\0KRAW Audio File (*.KRAW)\0",

    "leg\0LEG Audio File (*.LEG)\0",	
    "logg\0LOGG Audio File (*.LOGG)\0",
    "lpcm\0LPCM Audio File (*.LPCM)\0",
    "lps\0LPS Audio File (*.LPS)\0",
    "lsf\0LSF Audio File (*.LSF)\0",
    "lwav\0LWAV Audio File (*.LWAV)\0",

    "matx\0MATX Audio File (*.MATX)\0",
    "mcg\0MCG Audio File (*.MCG)\0",
    "mi2\0PS2 MI2 Audio File (*.MI2)\0",
    "mi4\0PS2 MI4 Audio File (*.MI4)\0",
    "mib\0PS2 MIB Audio File (*.MIB)\0",
    "mic\0PS2 MIC Audio File (*.MIC)\0",
    "mihb\0MIHB Audio File (*.MIHB)\0",
    "mpdsp\0MPDSP Audio File (*.MPDSP)\0",
    "mpds\0MPDS Audio File (*.MPDS)\0",
    "msa\0MSA Audio File (*.MSA)\0",
    "msf\0MSF Audio File (*.MSF)\0",
    "mss\0MSS Audio File (*.MSS)\0",
    "msvp\0MSVP Audio File (*.MSVP)\0",
	"mtaf\0MTAF Audio File (*.MTAF)\0",
    "mus\0MUS Playlist File (*.MUS)\0",
    "musc\0MUSC Audio File (*.MUSC)\0",
    "musx\0MUSX Audio File (*.MUSX)\0",
    "mwv\0MWV Audio File (*.MWV)\0",
    "mxst\0MxSt Audio File (*.MxSt)\0",
    "myspd\0MYSPD Audio File (*.MYSPD)\0",

    "ndp\0NDP Audio File (*.NDP)\0",
    "ngca\0NGCA Audio File (*.NGCA)\0",
    "npsf\0PS2 NPSF Audio File (*.NPSF)\0",
    "nwa\0NWA Audio File (*.NWA)\0",

    "omu\0OMU Audio File (*.OMU)\0",

    "p2bt\0P2BT Audio File (*.P2BT)\0",
    "p3d\0P3D Audio File (*.P3D)\0",
	"past\0PAST Audio File (*.PAST)\0",
    "pcm\0PCM Audio File (*.PCM)\0",
    "pdt\0PDT Audio File (*.PDT)\0",
    "pnb\0PNB Audio File (*.PNB)\0",
    "pona\0PONA Audio File (*.PONA)\0",
    "pos\0POS Audio File (*.POS)\0",
    "ps2stm\0PS2STM Audio File (*.PS2STM)\0",
    "psh\0PSH Audio File (*.PSH)\0",
    "psw\0PSW Audio File (*.PSW)\0",

    "ras\0RAS Audio File (*.RAS)\0",
	"raw\0RAW Audio File (*.RAW)\0",
    "rkv\0RKV Audio File (*.RKV)\0",
    "rnd\0RND Audio File (*.RND)\0",
    "rrds\0RRDS Audio File (*.RRDS)\0",
    "rsd\0RSD Audio File (*.RSD)\0",
    "rsf\0RSF Audio File (*.RSF)\0",
    "rstm\0RSTM Audio File (*.RSTM)\0",
    "rvws\0RVWS Audio File (*.RVWS)\0",
    "rwar\0RWAR Audio File (*.RWSD)\0",
    "rwav\0RWAV Audio File (*.RWAV)\0",
    "rws\0RWS Audio File (*.RWS)\0",
    "rwsd\0RWSD Audio File (*.RWSD)\0",
    "rwx\0RWX Audio File (*.RWX)\0",
    "rxw\0PS2 RXWS File (*.RXW)\0",

    "s14\0S14 Audio File (*.S14)\0",
    "sab\0SAB Audio File (*.SAB)\0",
    "sad\0SAD Audio File (*.SAD)\0",
    "sap\0SAP Audio File (*.SAP)\0",
    "sc\0SC Audio File (*.SC)\0",
    "scd\0SCD Audio File (*.SCD)\0",
    "sck\0SCK Audio File (*.SCK)\0",
    "sd9\0SD9 Audio File (*.SD9)\0",
    "sdt\0SDT Audio File (*.SDT)\0",
    "seg\0SEG Audio File (*.SEG)\0",
    "sfl\0SFL Audio File (*.SFL)\0",
	"sgb\0SGB Audio File (*.SGB)\0",
    "sfs\0SFS Audio File (*.SFS)\0",
	"sgb\0SGB Audio File (*.SGB)\0",
    "sl3\0SL3 Audio File (*.SL3)\0",
    "sli\0SLI Audio File (*.SLI)\0",
    "smp\0SMP Audio File (*.SMP)\0",
    "smpl\0SMPL Audio File (*.SMPL)\0",
    "snd\0SND Audio File (*.SND)\0",
    "sng\0SNG Audio File (*.SNG)\0",
    "sns\0SNS Audio File (*.SNS)\0",
    "spd\0SPD Audio File (*.SPD)\0",
    "spm\0SPM Audio File (*.SPM)\0",
    "sps\0SPS Audio File (*.SPS)\0",
    "spsd\0SPSD Audio File (*.SPSD)\0",
    "spw\0SPW Audio File (*.SPW)\0",
    "ss2\0PS2 SS2 Audio File (*.SS2)\0",
    "ss3\0SS3 Audio File (*.SS3)\0",
    "ss7\0SS7 Audio File (*.SS7)\0",
    "ssm\0SSM Audio File (*.SSM)\0",
    "sss\0SSS Audio File (*.SSS)\0",
    "ster\0STER Audio File (*.STER)\0",
    "sth\0STH Audio File (*.STH)\0",
    "stma\0STMA Audio File (*.STMA)\0",
    "str\0STR Audio File (*.STR)\0",
    "strm\0STRM Audio File (*.STRM)\0",
    "sts\0PS2 EXST Audio File (*.STS)\0",
    "stx\0STX Audio File (*.STX)\0",
    "svag\0PS2 SVAG Audio File (*.SVAG)\0",
    "svs\0SVS Audio File (*.SVS)\0",
    "swav\0SWAV Audio File (*.SWAV)\0",
    "swd\0SWD Audio File (*.SWD)\0",

	"tec\0TEC Audio File (*.TEC)\0",
    "thp\0THP Audio File (*.THP)\0",
    "tk1\0TK1 Audio File (*.TK1)\0",
    "tk5\0TK5 Audio File (*.TK5)\0",
	"tra\0TRA Audio File (*.TRA)\0",
    "tydsp\0TYDSP Audio File (*.TYDSP)\0",

    "um3\0UM3 Audio File (*.UM3)\0",

    "vag\0VAG Audio File (*.VAG)\0",
    "vas\0VAS Audio File (*.VAS)\0",
    "vb\0VB Audio File (*.VB)\0",
    "vgs\0VGS Audio File (*.VGS)\0",
    "vig\0VIG Audio File (*.VIG)\0",
	"vms\0VMS Audio File (*.VMS)\0",
    "vpk\0VPK Audio File (*.VPK)\0",
    "vs\0VS Audio File (*.VS)\0",
    "vsf\0VSF Audio File (*.VSF)\0",
    "vgv\0VGV Audio File (*.VGV)\0",
    "voi\0VOI Audio File (*.VOI)\0",

    "waa\0WAA Audio File (*.WAA)\0",
    "wac\0WAC Audio File (*.WAC)\0",
    "wad\0WAD Audio File (*.WAD)\0",
    "wam\0WAM Audio File (*.WAM)\0",
    "wavm\0WAVM Audio File (*.WAVM)\0",
    "was\0WAS Audio File (*.WAS)\0",
    "wb\0WB Audio File (*.WB)\0",
    "wii\0WII Audio File (*.WII)\0",
    "wp2\0WP2 Audio File (*.WP2)\0",
    "wsd\0WSD Audio File (*.WSD)\0",
    "wsi\0WSI Audio File (*.WSI)\0",
    "wvs\0WVS Audio File (*.WVS)\0",

    "xa\0PSX CD-XA File (*.XA)\0",
    "xa2\0XA2 Audio File (*.XA2)\0",
    "xa30\0XA30 Audio File (*.XA30)\0",
	"xau\0XAU Audio File (*.XAU)\0",
    "xmu\0XMU Audio File (*.XMU)\0",
    "xsf\0XSF Audio File (*.XSF)\0",
    "xss\0XSS Audio File (*.XSS)\0",
    "xvag\0XVAG Audio File (*.XVAG)\0",
    "xvas\0XVAS Audio File (*.XVAS)\0",
    "xwav\0XWAV Audio File (*.XWAV)\0",
    "xwb\0XWB Audio File (*.XWB)\0",
	"xag\0XAG Audio File (*.XAG)\0",

    "ydsp\0YDSP Audio File (*.YDSP)\0",
    "ymf\0YMF Audio File (*.YMF)\0",

    "zsd\0ZSD Audio File (*.ZSD)\0",
    "zwdsp\0ZWDSP Audio File (*.ZWDSP)\0",
};

void about(HWND hwndParent) {
    MessageBox(hwndParent,
            PLUGIN_DESCRIPTION "\n"
            "by hcs, FastElbja, manakoAT, bxaimc, and snakemeat\n\n"
            "http://sourceforge.net/projects/vgmstream"
            ,"about in_vgmstream",MB_OK);
}
void quit() {}

void build_extension_list() {
    int i;
    working_extension_list[0]='\0';
    working_extension_list[1]='\0';

    for (i=0;i<sizeof(extension_list)/sizeof(extension_list[0]);i++) {
        concatn_doublenull(EXTENSION_LIST_SIZE,working_extension_list,
                extension_list[i]);
    }
}

void GetINIFileName(char * iniFile) {
    /* if we're running on a newer winamp version that better supports
     * saving of settings to a per-user directory, use that directory - if not
     * then just revert to the old behaviour */

    if(IsWindow(input_module.hMainWindow) && SendMessage(input_module.hMainWindow, WM_WA_IPC,0,IPC_GETVERSION) >= 0x5000) {
        char * iniDir = (char *)SendMessage(input_module.hMainWindow, WM_WA_IPC, 0, IPC_GETINIDIRECTORY);
        strncpy(iniFile, iniDir, MAX_PATH);

        strncat(iniFile, "\\Plugins\\", MAX_PATH);
        /* can't be certain that \Plugins already exists in the user dir */
        CreateDirectory(iniFile,NULL);
        strncat(iniFile, INI_NAME, MAX_PATH);

    }
    else {
        char * lastSlash;

        GetModuleFileName(NULL, iniFile, MAX_PATH);
        lastSlash = strrchr(iniFile, '\\');

        *(lastSlash + 1) = 0;
        strncat(iniFile, "Plugins\\" INI_NAME,MAX_PATH);
    }
}

void init() {
    char iniFile[MAX_PATH+1];
    char buf[256];
    int consumed;

    GetINIFileName(iniFile);

    thread_priority=GetPrivateProfileInt(APP_NAME,THREAD_PRIORITY_INI_ENTRY,DEFAULT_THREAD_PRIORITY,iniFile);
    if (thread_priority < 0 || thread_priority > 6) {
        sprintf(buf,"%d",DEFAULT_THREAD_PRIORITY);
        WritePrivateProfileString(APP_NAME,THREAD_PRIORITY_INI_ENTRY,buf,iniFile);
        thread_priority = DEFAULT_THREAD_PRIORITY;
    }

    GetPrivateProfileString(APP_NAME,FADE_SECONDS_INI_ENTRY,DEFAULT_FADE_SECONDS,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&fade_seconds,&consumed)<1 || consumed!=strlen(buf) || fade_seconds < 0) {
        WritePrivateProfileString(APP_NAME,FADE_SECONDS_INI_ENTRY,DEFAULT_FADE_SECONDS,iniFile);
        sscanf(DEFAULT_FADE_SECONDS,"%lf",&fade_seconds);
    }

    GetPrivateProfileString(APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,DEFAULT_FADE_DELAY_SECONDS,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&fade_delay_seconds,&consumed)<1 || consumed!=strlen(buf)) {
        WritePrivateProfileString(APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,DEFAULT_FADE_DELAY_SECONDS,iniFile);
        sscanf(DEFAULT_FADE_DELAY_SECONDS,"%lf",&fade_delay_seconds);
    }

    GetPrivateProfileString(APP_NAME,LOOP_COUNT_INI_ENTRY,DEFAULT_LOOP_COUNT,buf,sizeof(buf),iniFile);
    if (sscanf(buf,"%lf%n",&loop_count,&consumed)!=1 || consumed!=strlen(buf) || loop_count < 0) {
        WritePrivateProfileString(APP_NAME,LOOP_COUNT_INI_ENTRY,DEFAULT_LOOP_COUNT,iniFile);
        sscanf(DEFAULT_LOOP_COUNT,"%lf",&loop_count);
    }

    loop_forever=GetPrivateProfileInt(APP_NAME,LOOP_FOREVER_INI_ENTRY,DEFAULT_LOOP_FOREVER,iniFile);
    ignore_loop=GetPrivateProfileInt(APP_NAME,IGNORE_LOOP_INI_ENTRY,DEFAULT_IGNORE_LOOP,iniFile);

    if (loop_forever && ignore_loop) {
        sprintf(buf,"%d",DEFAULT_LOOP_FOREVER);
        WritePrivateProfileString(APP_NAME,LOOP_FOREVER_INI_ENTRY,buf,iniFile);
        loop_forever = DEFAULT_LOOP_FOREVER;
        sprintf(buf,"%d",DEFAULT_IGNORE_LOOP);
        WritePrivateProfileString(APP_NAME,IGNORE_LOOP_INI_ENTRY,buf,iniFile);
        ignore_loop = DEFAULT_IGNORE_LOOP;
    }

    build_extension_list();
}

/* we don't recognize protocols */
int isourfile(char *fn) { return 0; }

/* request to start playing a file */
int play(char *fn)
{
    int max_latency;

    /* don't lose a pointer! */
    if (vgmstream) {
        /* TODO: this should either pop up an error box or close the file */
        return 1;
    }
    /* open the stream, set up */
    vgmstream = init_vgmstream(fn);
    /* were we able to open it? */
    if (!vgmstream) {
        return 1;
    }
    if (ignore_loop) vgmstream->loop_flag = 0;
    /* will we be able to play it? */
    if (vgmstream->channels <= 0) {
        close_vgmstream(vgmstream);
        vgmstream=NULL;
        return 1;
    }

    /* Remember that name, friends! */
    strncpy(lastfn,fn,MAX_PATH);

    /* open the output plugin */
    max_latency = input_module.outMod->Open(vgmstream->sample_rate,vgmstream->channels,
            16, 0, 0);
    /* were we able to open it? */
    if (max_latency < 0) {
        close_vgmstream(vgmstream);
        vgmstream=NULL;
        return 1;
    }

    /* Set info display */
    /* TODO: actual bitrate */
    input_module.SetInfo(100,vgmstream->sample_rate/1000,vgmstream->channels,1);

    /* setup visualization */
    input_module.SAVSAInit(max_latency,vgmstream->sample_rate);
    input_module.VSASetInfo(vgmstream->sample_rate,vgmstream->channels);

    decode_abort = 0;
    seek_needed_samples = -1;
    decode_pos_ms = 0;
    decode_pos_samples = 0;
    paused = 0;
    stream_length_samples = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,vgmstream);

    fade_samples = (int)(fade_seconds * vgmstream->sample_rate);

    decode_thread_handle = CreateThread(
            NULL,   /* handle cannot be inherited */
            0,      /* stack size, 0=default */
            decode, /* thread start routine */
            NULL,   /* no parameter to start routine */
            0,      /* run thread immediately */
            NULL);  /* don't keep track of the thread id */

    SetThreadPriority(decode_thread_handle,priority_values[thread_priority]);

    return 0;
}

/* pausing... */
void pause() { paused=1; input_module.outMod->Pause(1); }
void unpause() {paused=0; input_module.outMod->Pause(0); }
int ispaused() { return paused; }

/* stop playback */
void stop() {
    if (decode_thread_handle != INVALID_HANDLE_VALUE) {
        decode_abort=1;

        /* arbitrary wait length */
        if (WaitForSingleObject(decode_thread_handle,1000) == WAIT_TIMEOUT) {
            /* TODO: error? */
            TerminateThread(decode_thread_handle,0);
        }
        CloseHandle(decode_thread_handle);
        decode_thread_handle = INVALID_HANDLE_VALUE;
    }

    if (vgmstream) {
        close_vgmstream(vgmstream);
        vgmstream=NULL;
    }

    input_module.outMod->Close();
    input_module.SAVSADeInit();
}

/* get current stream length */
int getlength() {
    return stream_length_samples*1000LL/vgmstream->sample_rate;
}

/* get current output time */
int getoutputtime() {
    return decode_pos_ms+(input_module.outMod->GetOutputTime()-input_module.outMod->GetWrittenTime());
}

/* seek */
void setoutputtime(int t) {
    if (vgmstream)
        seek_needed_samples = (long long)t * vgmstream->sample_rate / 1000LL;
}

/* pass these commands through */
void setvolume(int volume) { input_module.outMod->SetVolume(volume); }
void setpan(int pan) { input_module.outMod->SetPan(pan); }

/* display information */
int infoDlg(char *fn, HWND hwnd) {
    VGMSTREAM * infostream = NULL;
    char description[1024];
    description[0]='\0';

    concatn(sizeof(description),description,PLUGIN_DESCRIPTION "\n\n");

    if (!fn || !*fn) {
        if (!vgmstream) return 0;
        describe_vgmstream(vgmstream,description,sizeof(description));
    } else {
        infostream = init_vgmstream(fn);
        if (!infostream) return 0;
        describe_vgmstream(infostream,description,sizeof(description));
        close_vgmstream(infostream);
        infostream=NULL;
    }

    MessageBox(hwnd,description,"Stream info",MB_OK);
    return 0;
}

/* retrieve information on this or possibly another file */
void getfileinfo(char *filename, char *title, int *length_in_ms) {
    if (!filename || !*filename)  /* currently playing file*/
    {
        if (!vgmstream) return;
        if (length_in_ms) *length_in_ms=getlength();
        if (title) 
        {
            char *p=lastfn+strlen(lastfn);
            while (*p != '\\' && p >= lastfn) p--;
            strcpy(title,++p);
        }
    }
    else /* some other file */
    {
        VGMSTREAM * infostream;
        if (length_in_ms) 
        {
            *length_in_ms=-1000;
            if ((infostream=init_vgmstream(filename)))
            {
                *length_in_ms = get_vgmstream_play_samples(loop_count,fade_seconds,fade_delay_seconds,infostream)*1000LL/infostream->sample_rate;

                close_vgmstream(infostream);
                infostream=NULL;
            }
        }
        if (title) 
        {
            char *p=filename+strlen(filename);
            while (*p != '\\' && p >= filename) p--;
            strcpy(title,++p);
        }
    }
}

/* nothin' */
void eq_set(int on, char data[10], int preamp) {}

/* the decode thread */
DWORD WINAPI __stdcall decode(void *arg) {
    /* channel count shouldn't change during decode */
    int max_buffer_samples = sizeof(sample_buffer)/sizeof(sample_buffer[0])/2/vgmstream->channels;

    while (!decode_abort) {

        int samples_to_do;
        int l;

        if (decode_pos_samples+max_buffer_samples>stream_length_samples && (!loop_forever || !vgmstream->loop_flag))
            samples_to_do=stream_length_samples-decode_pos_samples;
        else
            samples_to_do=max_buffer_samples;

        /* play 'till the end of this seek, or note if we're done seeking */
        if (seek_needed_samples != -1) {
            /* reset if we need to seek backwards */
            if (seek_needed_samples < decode_pos_samples) {
                reset_vgmstream(vgmstream);

                if (ignore_loop) vgmstream->loop_flag = 0;

                decode_pos_samples = 0;
                decode_pos_ms = 0;
            }

            if (decode_pos_samples < seek_needed_samples) {
                samples_to_do=seek_needed_samples-decode_pos_samples;
                if (samples_to_do>max_buffer_samples) samples_to_do=max_buffer_samples;
            } else
                seek_needed_samples = -1;

            input_module.outMod->Flush((int)decode_pos_ms);
        }

        l = (samples_to_do*vgmstream->channels*2)<<(input_module.dsp_isactive()?1:0);

        if (samples_to_do == 0) {
            input_module.outMod->CanWrite();    /* ? */
            if (!input_module.outMod->IsPlaying()) {
                PostMessage(input_module.hMainWindow,   /* message dest */
                        WM_WA_MPEG_EOF,     /* message id */
                        0,0);   /* no parameters */
                return 0;
            }
            Sleep(10);
        }
        else if (seek_needed_samples != -1) {
            render_vgmstream(sample_buffer,samples_to_do,vgmstream);

            decode_pos_samples+=samples_to_do;
            decode_pos_ms=decode_pos_samples*1000LL/vgmstream->sample_rate;
        }
        else if (input_module.outMod->CanWrite() >= l) {
            /* let vgmstream do its thing */
            render_vgmstream(sample_buffer,samples_to_do,vgmstream);

            /* fade! */
            if (vgmstream->loop_flag && fade_samples > 0 && !loop_forever) {
                int samples_into_fade = decode_pos_samples - (stream_length_samples - fade_samples);
                if (samples_into_fade + samples_to_do > 0) {
                    int j,k;
                    for (j=0;j<samples_to_do;j++,samples_into_fade++) {
                        if (samples_into_fade > 0) {
                            double fadedness = (double)(fade_samples-samples_into_fade)/fade_samples;
                            for (k=0;k<vgmstream->channels;k++) {
                                sample_buffer[j*vgmstream->channels+k] =
                                    (short)(sample_buffer[j*vgmstream->channels+k]*fadedness);
                            }
                        }
                    }
                }
            }

            input_module.SAAddPCMData((char*)sample_buffer,vgmstream->channels,16,decode_pos_ms);
            input_module.VSAAddPCMData((char*)sample_buffer,vgmstream->channels,16,decode_pos_ms);
            decode_pos_samples+=samples_to_do;
            decode_pos_ms=decode_pos_samples*1000LL/vgmstream->sample_rate;
            if (input_module.dsp_isactive())
                l =input_module.dsp_dosamples(sample_buffer,samples_to_do,16,vgmstream->channels,vgmstream->sample_rate) * 
                    2 * vgmstream->channels;

            input_module.outMod->Write((char*)sample_buffer,l);
        }   /* if we can write enough */
        else Sleep(20);
    }   /* main loop */
    return 0;
}

INT_PTR CALLBACK configDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    char buf[256];
    char iniFile[MAX_PATH+1];
    static int mypri;
    HANDLE hSlider;

    switch (uMsg) { 
        case WM_CLOSE:
            EndDialog(hDlg,TRUE);
            return TRUE;
        case WM_INITDIALOG:
            GetINIFileName(iniFile);

            /* set CPU Priority slider */
            hSlider=GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER);
            SendMessage(hSlider, TBM_SETRANGE,
                    (WPARAM) TRUE,                  /* redraw flag */
                    (LPARAM) MAKELONG(1, 7));       /* min. & max. positions */
            SendMessage(hSlider, TBM_SETPOS, 
                    (WPARAM) TRUE,                  /* redraw flag */
                    (LPARAM) thread_priority+1);
            mypri=thread_priority;
            SetDlgItemText(hDlg,IDC_THREAD_PRIORITY_TEXT,priority_strings[thread_priority]);

            sprintf(buf,"%.2lf",fade_seconds);
            SetDlgItemText(hDlg,IDC_FADE_SECONDS,buf);
            sprintf(buf,"%.2lf",fade_delay_seconds);
            SetDlgItemText(hDlg,IDC_FADE_DELAY_SECONDS,buf);
            sprintf(buf,"%.2lf",loop_count);
            SetDlgItemText(hDlg,IDC_LOOP_COUNT,buf);

            if (loop_forever)
                CheckDlgButton(hDlg,IDC_LOOP_FOREVER,BST_CHECKED);
            else if (ignore_loop)
                CheckDlgButton(hDlg,IDC_IGNORE_LOOP,BST_CHECKED);
            else
                CheckDlgButton(hDlg,IDC_LOOP_NORMALLY,BST_CHECKED);

            break;
        case WM_COMMAND:
            switch (GET_WM_COMMAND_ID(wParam, lParam)) {
                case IDOK:
                    {
                        double temp_fade_seconds;
                        double temp_fade_delay_seconds;
                        double temp_loop_count;
                        int consumed;

                        /* read and verify */
                        GetDlgItemText(hDlg,IDC_FADE_SECONDS,buf,sizeof(buf));
                        if (sscanf(buf,"%lf%n",&temp_fade_seconds,&consumed)<1
                            || consumed!=strlen(buf) ||
                            temp_fade_seconds<0) {
                            MessageBox(hDlg,
                                    "Invalid value for Fade Length\n"
                                    "Must be a number greater than or equal to zero",
                                    "Error",MB_OK|MB_ICONERROR);
                            break;
                        }

                        GetDlgItemText(hDlg,IDC_FADE_DELAY_SECONDS,buf,sizeof(buf));
                        if (sscanf(buf,"%lf%n",&temp_fade_delay_seconds,
                            &consumed)<1 || consumed!=strlen(buf)) {
                            MessageBox(hDlg,
                                    "Invalid valid for Fade Delay\n"
                                    "Must be a number",
                                    "Error",MB_OK|MB_ICONERROR);
                            break;
                        }

                        GetDlgItemText(hDlg,IDC_LOOP_COUNT,buf,sizeof(buf));
                        if (sscanf(buf,"%lf%n",&temp_loop_count,&consumed)<1 ||
                                consumed!=strlen(buf) ||
                                temp_loop_count<0) {
                            MessageBox(hDlg,
                                    "Invalid value for Loop Count\n"
                                    "Must be a number greater than or equal to zero",
                                    "Error",MB_OK|MB_ICONERROR);
                            break;
                        }

                        GetINIFileName(iniFile);

                        thread_priority=mypri;
                        sprintf(buf,"%d",thread_priority);
                        WritePrivateProfileString(APP_NAME,THREAD_PRIORITY_INI_ENTRY,buf,iniFile);

                        fade_seconds = temp_fade_seconds;
                        sprintf(buf,"%.2lf",fade_seconds);
                        WritePrivateProfileString(APP_NAME,FADE_SECONDS_INI_ENTRY,buf,iniFile);

                        fade_delay_seconds = temp_fade_delay_seconds;
                        sprintf(buf,"%.2lf",fade_delay_seconds);
                        WritePrivateProfileString(APP_NAME,FADE_DELAY_SECONDS_INI_ENTRY,buf,iniFile);

                        loop_count = temp_loop_count;
                        sprintf(buf,"%.2lf",loop_count);
                        WritePrivateProfileString(APP_NAME,LOOP_COUNT_INI_ENTRY,buf,iniFile);

                        loop_forever = (IsDlgButtonChecked(hDlg,IDC_LOOP_FOREVER) == BST_CHECKED);
                        sprintf(buf,"%d",loop_forever);
                        WritePrivateProfileString(APP_NAME,LOOP_FOREVER_INI_ENTRY,buf,iniFile);

                        ignore_loop = (IsDlgButtonChecked(hDlg,IDC_IGNORE_LOOP) == BST_CHECKED);
                        sprintf(buf,"%d",ignore_loop);
                        WritePrivateProfileString(APP_NAME,IGNORE_LOOP_INI_ENTRY,buf,iniFile);
                    }

                    EndDialog(hDlg,TRUE);
                    break;
                case IDCANCEL:
                    EndDialog(hDlg,TRUE);
                    break;
                case IDC_DEFAULT_BUTTON:
                    /* set CPU Priority slider */
                    hSlider=GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER);
                    SendMessage(hSlider, TBM_SETRANGE,
                            (WPARAM) TRUE,                  /* redraw flag */
                            (LPARAM) MAKELONG(1, 7));       /* min. & max. positions */
                    SendMessage(hSlider, TBM_SETPOS, 
                            (WPARAM) TRUE,                  /* redraw flag */
                            (LPARAM) DEFAULT_THREAD_PRIORITY+1);
                    mypri=DEFAULT_THREAD_PRIORITY;
                    SetDlgItemText(hDlg,IDC_THREAD_PRIORITY_TEXT,priority_strings[mypri]);

                    SetDlgItemText(hDlg,IDC_FADE_SECONDS,DEFAULT_FADE_SECONDS);
                    SetDlgItemText(hDlg,IDC_FADE_DELAY_SECONDS,DEFAULT_FADE_DELAY_SECONDS);
                    SetDlgItemText(hDlg,IDC_LOOP_COUNT,DEFAULT_LOOP_COUNT);

                    CheckDlgButton(hDlg,IDC_LOOP_FOREVER,BST_UNCHECKED);
                    CheckDlgButton(hDlg,IDC_IGNORE_LOOP,BST_UNCHECKED);
                    CheckDlgButton(hDlg,IDC_LOOP_NORMALLY,BST_CHECKED);
                    break;
                default:
                    return FALSE;
            }
        case WM_HSCROLL:
            if ((struct HWND__ *)lParam==GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER)) {
                if (LOWORD(wParam)==TB_THUMBPOSITION || LOWORD(wParam)==TB_THUMBTRACK) mypri=HIWORD(wParam)-1;
                else mypri=SendMessage(GetDlgItem(hDlg,IDC_THREAD_PRIORITY_SLIDER),TBM_GETPOS,0,0)-1;
                SetDlgItemText(hDlg,IDC_THREAD_PRIORITY_TEXT,priority_strings[mypri]);
            }
            break;
        default:
            return FALSE;
    }

    return TRUE;
}

void config(HWND hwndParent) {
    DialogBox(input_module.hDllInstance, (const char *)IDD_CONFIG, hwndParent, configDlgProc);
}

In_Module input_module = 
{
    IN_VER,
    PLUGIN_DESCRIPTION,
    0,  /* hMainWindow */
    0,  /* hDllInstance */
    working_extension_list,
    1, /* is_seekable  */
    1, /* uses output */
    config,
    about,
    init,
    quit,
    getfileinfo,
    infoDlg,
    isourfile,
    play,
    pause,
    unpause,
    ispaused,
    stop,
    getlength,
    getoutputtime,
    setoutputtime,
    setvolume,
    setpan,
    0,0,0,0,0,0,0,0,0, // vis stuff
    0,0, // dsp
    eq_set,
    NULL,       // setinfo
    0 // out_mod
};

__declspec( dllexport ) In_Module * winampGetInModule2()
{
    return &input_module;
}


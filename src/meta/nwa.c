#include "meta.h"
#include "../util.h"
#include <string.h>

#ifdef WIN32
#define DIRSEP '\\'
#else
#define DIRSEP '/'
#endif

/* NWA - Visual Arts streams
 *
 * This can apparently get a lot more complicated, I'm only handling the
 * raw PCM case at the moment (until I see something else).
 *
 * Kazunori "jagarl" Ueno's nwatowav was helpful, and will probably be used
 * to write coding support if it comes to that.
 */

VGMSTREAM * init_vgmstream_nwa(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
	int i;
    int channel_count;
    int loop_flag = 0;
    int32_t loop_start_sample;
    int ini_found = 0;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("nwa",filename_extension(filename))) goto fail;

    /* check that we're using raw pcm */
    if (
            read_32bitLE(0x08,streamFile)!=-1 || /* compression level */
            read_32bitLE(0x10,streamFile)!=0  || /* block count */
            read_32bitLE(0x18,streamFile)!=0  || /* compressed data size */
            read_32bitLE(0x20,streamFile)!=0  || /* block size */
            read_32bitLE(0x24,streamFile)!=0     /* restsize */
       ) goto fail;

    /* try to locate NWAINFO.INI in the same directory */
    {
        char ininame[260];
        char * ini_lastslash;
        char * namebase;
        STREAMFILE *inistreamfile;

        strncpy(ininame,filename,sizeof(ininame));
        ininame[sizeof(ininame)-1]='\0';    /* a pox on the stdlib! */

        ini_lastslash = strrchr(ininame,DIRSEP);
        if (!ini_lastslash) {
            strncpy(ininame,"NWAINFO.INI",sizeof(ininame));
            namebase = filename;
        } else {
            strncpy(ini_lastslash+1,"NWAINFO.INI",
                    sizeof(ininame)-(ini_lastslash+1-ininame));
            namebase = ini_lastslash+1-ininame+filename;
        }
        ininame[sizeof(ininame)-1]='\0';    /* curse you, strncpy! */

        inistreamfile = streamFile->open(streamFile,ininame,4096);

        if (inistreamfile) {
            /* ini found, try to find our name */
            const char * ext;
            int length;
            int found;
            off_t offset;
            off_t file_size;
            off_t found_off;

            ini_found = 1;

            ext = filename_extension(namebase);
            length = ext-1-namebase;
            file_size = get_streamfile_size(inistreamfile);

            for (found = 0, offset = 0; !found && offset<file_size; offset++) {
                off_t suboffset;
                /* Go for an n*m search 'cause it's easier than building an
                 * FSA for the search string. Just wanted to make the point that
                 * I'm not ignorant, just lazy. */
                for (suboffset = offset;
                        suboffset<file_size &&
                        suboffset-offset<length &&
                        read_8bit(suboffset,inistreamfile)==
                            namebase[suboffset-offset];
                        suboffset++) {}

                if (suboffset-offset==length &&
                        read_8bit(suboffset,inistreamfile)==0x09) { /* tab */
                    found=1;
                    found_off = suboffset+1;
                }
            }

            if (found) {
                char loopstring[9]={0};

                if (read_streamfile((uint8_t*)loopstring,found_off,8,
                            inistreamfile)==8)
                {
                    loop_start_sample = atol(loopstring);
                    if (loop_start_sample > 0) loop_flag = 1;
                }
            }   /* if found file name in INI */

            close_streamfile(inistreamfile);
        } /* if opened INI ok */
    } /* INI block */

    channel_count = read_16bitLE(0x00,streamFile);

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    /* fill in the vital statistics */
    vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bitLE(0x04,streamFile);
    switch (read_16bitLE(0x02,streamFile)) {
        case 16:
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->interleave_block_size = 2;
            break;
        case 8:
            vgmstream->coding_type = coding_PCM8;
            vgmstream->interleave_block_size = 1;
            break;
        default:
            goto fail;
    }
    vgmstream->num_samples = read_32bitLE(0x1c,streamFile)/channel_count;
    if (channel_count > 1) {
        vgmstream->layout_type = layout_interleave;
    } else {
        vgmstream->layout_type = layout_none;
    }

    if (ini_found) {
        vgmstream->meta_type = meta_NWA_INI;
    } else {
        vgmstream->meta_type = meta_NWA;
    }

    if (loop_flag) {
        vgmstream->loop_start_sample = loop_start_sample;
        vgmstream->loop_end_sample = vgmstream->num_samples;
    }

    /* open the file for reading by each channel */
    {
        STREAMFILE *chstreamfile;

        /* have both channels use the same buffer, as interleave is so small */
        chstreamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);

        if (!chstreamfile) goto fail;

        for (i=0;i<2;i++) {
            vgmstream->ch[i].streamfile = chstreamfile;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=0x2c+(off_t)(i*vgmstream->interleave_block_size);
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

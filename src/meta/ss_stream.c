#include "meta.h"
#include "../util.h"

/* SS_STREAM

   SS_STREAM is format used by various UBI Soft Games

   2008-11-15 - Fastelbja : First version ...

   
   Some infos, sorry for messing up the meta (regards, manako)

Splinter Cell - *.SS0; PC (except Music_*.SS0)
Splinter Cell: Pandora Tomorrow - *.SS0, *.LS0; PC
Splinter Cell: Chaos Theory - *.SS0, *.LS0; PC
Splinter Cell: Double Agent - *.SS0, *.LS0; PC

UbiSoft Old Simple Stream (version 3)
UbiSoft Simple Stream (version 5)
UbiSoft Old Interleaved Stream (version 2)
UbiSoft Interleaved Stream (version 8 )

Note: if the version number is 3, then all values in this file are big-endian. If the version is 5, then all values are little-endian.

Header:
byte {1} - Version number (3 or 5)
byte {3} - Unknown
byte {4} - Unknown
uint32 {4} - Unknown
uint16 {2} - Unknown
uint16 {2} - Number of extra uncompressed samples before the data (always 10)
int16 {2} - First left sample for decompression
byte {1} - First left index for decompression
byte {1} - Unknown
int16 {2} - First right sample for decompression
byte {1} - First right index for decompression
byte {1} - Unknown
byte {4} - Unknown

Extra Uncompressed Samples:
if the sound is mono:
int16 {Number of extra uncompressed samples * 2} - Uncompressed samples

if the sound is stereo:
int16 {Number of extra uncompressed samples * 4} - Uncompressed samples

Data:
byte {?} - Compressed data



And here is the format of the old interleaved streams:

Code:
Little-endian

uint32 {4} - Signature (2)
uint32 {4} - Number of Layers (always 3)
uint32 {4} - Total File Size
uint32 {4} - Unknown (always 20)
uint32 {4} - Unknown (always 1104)
uint32 {4} - Average Block Size (always 361)

For Each Block: {
   uint32 {4} - Block Index (begins at 1)
   uint32 {4} - Unknown (always 20)
   
   For Each Layer (Chunk): {
      uint32 {4} - Layer Chunk Size
   }
   
   For Each Layer (Chunk): {
      uint32 {Layer Chunk Size} - Chunk of an Encapsulated UbiSoft Old Simple Stream
   }
}


And the new interleaved streams:

Code:
Little-endian

uint16 {2} - Signature (8)
uint16 {2} - Unknown
uint32 {4} - Unknown
uint32 {4} - Number of Layers
uint32 {4} - Number of Blocks
uint32 {4} - Number of Bytes after This to the Headers
uint32 {4} - The Sum of (Number of Layers * 4) Plus the Header Lengths
uint32 {4} - Average Sum of Chunk Data Lengths

For Each Layer: {
   uint32 {4} - Layer Header Size
}

For Each Layer: {
   uint32 {Layer Header Size} - Header of an Encapsulated Stream (PCM, UbiSoft Simple Stream, Ogg Vorbis)
}

For Each Block: {
   uint32 {4} - Signature (3)
   uint32 {4} - Number of bytes from the start of this block to the next block, 0 if no more blocks
   
   For Each Layer (Chunk): {
      uint32 {4} - Layer Chunk Size
   }
   
   For Each Layer (Chunk): {
      uint32 {Layer Chunk Size} - Chunk of an Encapsulated Stream (PCM, UbiSoft Simple Stream, Ogg Vorbis)
   }
}


*/

VGMSTREAM * init_vgmstream_ss_stream(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    int loop_flag=0;
    int channels;
    int channel_count;
    int freq_flag;
    off_t start_offset;
    int i;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("ss3",filename_extension(filename)) && 
		strcasecmp("ss7",filename_extension(filename))) goto fail;

	loop_flag = 0;
  freq_flag = read_8bit(0x08,streamFile);

    if (read_8bit(0x0C,streamFile) == 0) {
        channels = 1;
    } else {
        channels = read_8bit(0x0C,streamFile)*2;
    }

    channel_count = channels;

    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
  vgmstream->channels = channel_count;
  vgmstream->sample_rate = 48000;


#if 0
    if (!strcasecmp("ss3",filename_extension(filename))) {
        vgmstream->sample_rate = 32000;
    } else if (!strcasecmp("ss7",filename_extension(filename))) {
        vgmstream->sample_rate = 48000;
    }
#endif

  start_offset = (read_8bit(0x07,streamFile)+5);

#if 0
            if (channel_count == 1){
                start_offset = 0x3C;
            } else if (channel_count == 2) {
                start_offset = 0x44;
            }
#endif

	if(channel_count==1)
		vgmstream->coding_type = coding_IMA;
	else
		vgmstream->coding_type = coding_EACS_IMA;

    vgmstream->num_samples = (int32_t)((get_streamfile_size(streamFile)-start_offset)* 2 / vgmstream->channels);
    vgmstream->layout_type = layout_none;
	
    vgmstream->meta_type = meta_XBOX_WAVM;
	vgmstream->get_high_nibble=0;

    /* open the file for reading by each channel */
    {
        for (i=0;i<channel_count;i++) {
           
            if (channel_count == 1){
                vgmstream->ch[i].offset = start_offset;
            } else if (channel_count == 2) {
                vgmstream->ch[i].offset = start_offset;
            }
            
                vgmstream->ch[i].streamfile = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
			    vgmstream->ch[i].adpcm_history1_32=(int32_t)read_16bitLE(0x10+i*4,streamFile);
    			vgmstream->ch[i].adpcm_step_index =(int)read_8bit(0x12+i*4,streamFile);
            if (!vgmstream->ch[i].streamfile) goto fail;
        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

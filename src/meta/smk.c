#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

static int smacker_get_info(STREAMFILE *streamFile, int target_subsong, int * out_total_streams, size_t *out_stream_size, int * out_channel_count, int * out_sample_rate, int * out_num_samples);

/* SMK - RAD Game Tools Smacker movies (audio/video format) */
VGMSTREAM * init_vgmstream_smk(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    int channel_count = 0, loop_flag = 0, sample_rate = 0, num_samples = 0;
    int total_subsongs = 0, target_subsong = streamFile->stream_index;
    size_t stream_size;


    /* checks */
    if (!check_extensions(streamFile,"smk"))
        goto fail;

    if (read_32bitBE(0x00,streamFile) != 0x534D4B32 &&  /* "SMK2" */
        read_32bitBE(0x00,streamFile) != 0x534D4B34)    /* "SMK4" */
        goto fail;

    /* find target stream info */
    if (!smacker_get_info(streamFile, target_subsong, &total_subsongs, &stream_size, &channel_count, &sample_rate, &num_samples))
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count, loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->layout_type = layout_none;
    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = meta_SMACKER;

    {
#ifdef VGM_USE_FFMPEG
        /* target_subsong should be passed manually */
        vgmstream->codec_data = init_ffmpeg_header_offset_subsong(streamFile, NULL,0, 0x0,get_streamfile_size(streamFile), target_subsong);
        if (!vgmstream->codec_data) goto fail;
        vgmstream->coding_type = coding_FFmpeg;
#else
        goto fail;
#endif
    }

    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

typedef enum {
    SMK_AUDIO_PACKED    = (1<<7),
    SMK_AUDIO_PRESENT   = (1<<6),
    SMK_AUDIO_16BITS    = (1<<5),
    SMK_AUDIO_STEREO    = (1<<4),
    SMK_AUDIO_BINK_RDFT = (1<<3),
    SMK_AUDIO_BINK_DCT  = (1<<2),
  //SMK_AUD_UNUSED1     = (1<<1),
  //SMK_AUD_UNUSED0     = (1<<0),
} smk_audio_flag;

//todo test multilang streams and codecs other than SMACKAUD
/* Gets stream info, and number of samples in a file by reading frames
 * info: https://wiki.multimedia.cx/index.php/Smacker */
static int smacker_get_info(STREAMFILE *sf, int target_subsong, int * out_total_subsongs, size_t * out_stream_size, int * out_channel_count, int * out_sample_rate, int * out_num_samples) {
    STREAMFILE *sf_index = NULL;
    uint32_t flags, total_frames, trees_sizes;
    off_t size_offset, type_offset, data_offset;
    int i, j, sample_rate = 0, channel_count = 0, num_samples = 0;
    int total_subsongs, target_stream = 0;
    size_t stream_size = 0;
    uint8_t stream_flags = 0;
    
    
    /* rough format:
     * - header (id, frames, video info/config, audio info)
     * - frame sizes table
     * - frame info table
     * - huffman trees
     * - frame data
     */

    /* read header */
    total_frames = read_u32le(0x0c,sf);
    if (total_frames <= 0 || total_frames > 0x100000) goto fail; /* something must be off */

    flags = read_u32le(0x14,sf);
    if (flags & 1) /* extra "ring frame" */
        total_frames += 1;

    trees_sizes = read_u32le(0x34,sf);

    if (target_subsong == 0) target_subsong = 1;
    total_subsongs = 0;
    for (i = 0; i < 7; i++) { /* up to 7 audio (multilang?) streams */
        uint32_t audio_info = read_u32le(0x48 + 0x04*i,sf);
        uint8_t audio_flags = (audio_info >> 24) & 0xFF;
        int audio_rate = audio_info & 0x00FFFFFF;

        if (audio_flags & SMK_AUDIO_PRESENT) {
            total_subsongs++;
            
            if (target_subsong == total_subsongs) {
                target_stream = i;
                sample_rate = audio_rate & 0x00FFFFFF;
                channel_count = (audio_flags & SMK_AUDIO_STEREO) ? 2 : 1;
                stream_flags = audio_flags;
            }
        }
    }
    if (target_subsong < 0 || target_subsong > total_subsongs || total_subsongs < 1) goto fail;
    if (sample_rate == 0 || channel_count == 0) goto fail;

    /* read size and type tables into buffer */
    sf_index = reopen_streamfile(sf, total_frames*0x04 + total_frames*0x01);
    if (!sf_index) goto fail;

    /* read frames and sum all samples, since some codecs are VBR */
    size_offset = 0x68;
    type_offset = size_offset + total_frames*0x04;
    data_offset = type_offset + total_frames*0x01 + trees_sizes;
    for (i=0; i < total_frames; i++) {
        uint32_t frame_size = read_u32le(size_offset,sf_index) & 0xFFFFFFFC; /* last 2 bits are keyframe flags */
        uint8_t  frame_type = read_u8   (type_offset,sf_index); /* 0: has palette, 1..7: has stream N) */
        off_t offset = data_offset;
        
        /* skip palette */
        if (frame_type & (1<<0)) {
            uint8_t palette_size = read_u8(offset,sf);
            offset += palette_size * 4;
        }

        /* read target audio packet and ignore rest (though probably all streams are the same) */
        for (j = 0; j < 7; j++) {
            uint32_t audio_size;
            
            /* check if stream N exists in this frame (supposedly streams can be go in separate frames) */
            if ( !(frame_type & (1<<(j+1))) )
                continue;

            audio_size = read_u32le(offset,sf);
            
            if (j == target_stream) {
                
                /* resulting PCM bytes to samples */
                if (stream_flags & SMK_AUDIO_PACKED) { /* Smacker and maybe Bink codecs */
                    uint32_t unpacked_size = read_u32le(offset+0x04,sf);
                    num_samples += unpacked_size / (0x02 * channel_count);
                }
                else if (stream_flags & SMK_AUDIO_16BITS) { /* PCM16 */
                    num_samples += (audio_size - 0x04) / (0x02 * channel_count);
                }
                else { /* PCM8 */
                    num_samples += (audio_size - 0x04) / (0x01 * channel_count);
                }
            }

            stream_size += audio_size;
            offset += audio_size;
        }
        
        /* rest is video packet (size = offset - data_offset) */

        size_offset += 0x04;
        type_offset += 0x01;
        data_offset += frame_size;
    }

    if (out_total_subsongs) *out_total_subsongs = total_subsongs;
    if (out_stream_size)    *out_stream_size = stream_size;
    if (out_sample_rate)    *out_sample_rate = sample_rate;
    if (out_channel_count)  *out_channel_count = channel_count;
    if (out_num_samples)    *out_num_samples = num_samples;

    close_streamfile(sf_index);
    return 1;

fail:
    close_streamfile(sf_index);
    return 0;
}

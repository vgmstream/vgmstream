#include "meta.h"
#include "../coding/coding.h"

/* XNB - Microsoft XNA Game Studio 4.0 format */
VGMSTREAM * init_vgmstream_xnb(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;
    int flags, codec, sample_rate, block_size, bps;
    size_t xnb_size, data_size;


    /* check extension, case insensitive */
    if ( !check_extensions(streamFile,"xnb"))
        goto fail;

    /* check header */
    if ((read_32bitBE(0,streamFile) & 0xFFFFFF00) != 0x584E4200) /* "XNB" */
        goto fail;
    /* 0x04: platform: ‘w’ = Microsoft Windows, ‘m’ = Windows Phone 7, ‘x’ = Xbox 360, 'a' = Android */

    if (read_8bit(0x04,streamFile) != 0x05)  /* XNA 4.0 version only */
        goto fail;

    flags = read_8bit(0x05,streamFile);
    if (flags & 0x80) goto fail; /* compressed with XMemCompress */
    //if (flags & 0x01) goto fail; /* XMA/big endian flag? */

    /* "check for truncated XNB" (???) */
    xnb_size = read_32bitLE(0x06,streamFile);
    if (get_streamfile_size(streamFile) < xnb_size) goto fail;

    /* XNB contains "type reader" class references to parse "shared resource" data (can be any implemented filetype) */
    {
        char reader_name[255+1];
        off_t current_chunk = 0xa;
        int reader_string_len;
        uint32_t fmt_chunk_size;
        const char * type_sound =  "Microsoft.Xna.Framework.Content.SoundEffectReader"; /* partial "fmt" chunk or XMA */
        //const char * type_song =  "Microsoft.Xna.Framework.Content.SongReader"; /* just references a companion .wma */

        /* type reader count, accept only one for now */
        if (read_8bit(current_chunk++, streamFile) != 1)
            goto fail;

        reader_string_len = read_8bit(current_chunk++, streamFile); /* doesn't count null */
        if (reader_string_len > 255) goto fail;

        /* check SoundEffect type string */
        if (read_string(reader_name,reader_string_len+1,current_chunk,streamFile) != reader_string_len)
            goto fail;
        if ( strcmp(reader_name, type_sound) != 0 )
            goto fail;
        current_chunk += reader_string_len + 1;
        current_chunk += 4; /* reader version */

        /* shared resource count */
        if (read_8bit(current_chunk++, streamFile) != 1)
            goto fail;

        /* shared resource: partial "fmt" chunk */
        fmt_chunk_size = read_32bitLE(current_chunk, streamFile);
        current_chunk += 4;

        {
            codec         = read_16bitLE(current_chunk+0x00, streamFile);
            channel_count = read_16bitLE(current_chunk+0x02, streamFile);
            sample_rate   = read_32bitLE(current_chunk+0x04, streamFile);
            block_size    = read_16bitLE(current_chunk+0x0c, streamFile);
            bps           = read_16bitLE(current_chunk+0x0e, streamFile);
        }

        current_chunk += fmt_chunk_size;

        data_size = read_32bitLE(current_chunk, streamFile);
        current_chunk += 4;

        start_offset = current_chunk;
    }


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->meta_type = meta_XNB;

    switch (codec) {
        case 0x01:
            vgmstream->coding_type = bps == 8 ? coding_PCM8_U_int : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = block_size / channel_count;
            vgmstream->num_samples = pcm_bytes_to_samples(data_size, channel_count, bps);
            break;

        case 0x02:
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = block_size;
            vgmstream->num_samples = msadpcm_bytes_to_samples(data_size, block_size, channel_count);
            break;

        case 0x11:
            vgmstream->coding_type = coding_MS_IMA;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = block_size;
            vgmstream->num_samples = ms_ima_bytes_to_samples(data_size, block_size, channel_count);
            break;

        default:
            VGM_LOG("XNB: unknown codec 0x%x\n", codec);
            goto fail;
    }

    if ( !vgmstream_open_stream(vgmstream,streamFile,start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}

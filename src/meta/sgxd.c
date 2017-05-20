#include "meta.h"
#include "../coding/coding.h"


/* SGXD - Sony/SCEI's format (SGB+SGH / SGD / SGX), found in:
 *  PS3: Genji, Folklore, Afrika (Short VAG), Tokyo Jungle
 *  PSP: Brave Story, Sarugetchu Sarusaru Daisakusen, Kurohyo 1/2, Pathwork Heroes */
VGMSTREAM * init_vgmstream_sgxd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;
    off_t start_offset, data_offset, chunk_offset;
    size_t data_size;

    int is_sgx, is_sgb;
    int loop_flag, channels, type;
    int sample_rate, num_samples, loop_start_sample, loop_end_sample;
    int target_stream = 0, total_streams;


    /* check extension, case insensitive */
    /* .sgx: header+data (Genji), .sgd: header+data, .sgh/sgd: header/data */
    if (!check_extensions(streamFile,"sgx,sgd,sgb"))
        goto fail;
    is_sgx = check_extensions(streamFile,"sgx");
    is_sgb = check_extensions(streamFile,"sgb");

    /* SGB+SGH: use SGH as header; otherwise use the current file as header */
    if (is_sgb) {
        streamHeader = open_stream_ext(streamFile, "sgh");
        if (!streamHeader) goto fail;
    } else {
        streamHeader = streamFile;
    }


    /* SGXD base (size 0x10) */
    if (read_32bitBE(0x00,streamHeader) != 0x53475844) /* "SGXD" */
        goto fail;
    /* 0x04  SGX: full header_size; SGD/SGH: unknown header_size (counting from 0x0/0x8/0x10, varies) */
    /* 0x08  SGX: first chunk offset? (0x10); SGD/SGH: full header_size */
    /* 0x0c  SGX/SGH: full data size with padding; SGD: full data size + 0x80000000 with padding */
    if (is_sgb) {
        data_offset = 0x00;
    } else if ( is_sgx ) {
        data_offset = read_32bitLE(0x04,streamHeader);
    } else {
        data_offset = read_32bitLE(0x08,streamHeader);
    }


    /* typical chunks: WAVE, NAME (strings), RGND, SEQD (related to SFX), WSUR, WMKR, BUSS */
    /* WAVE chunk (size 0x10 + files * 0x38 + optional padding) */
    if (is_sgx) { /* position after chunk+size */
        if (read_32bitBE(0x10,streamHeader) != 0x57415645) goto fail;  /* "WAVE" */
        chunk_offset = 0x18;
    } else {
        if (!find_chunk_le(streamHeader, 0x57415645,0x10,0, &chunk_offset,NULL)) goto fail; /* "WAVE" */
    }
    /* 0x04  SGX: unknown; SGD/SGH: chunk length,  0x08  null */

    /* check multi-streams (usually only SE containers; Puppeteer) */
    total_streams = read_32bitLE(chunk_offset+0x04,streamHeader);
    if (target_stream == 0) target_stream = 1;
    if (target_stream < 0 || target_stream > total_streams || total_streams < 1) goto fail;

    /* read stream header */
    {
        off_t stream_offset;
        chunk_offset += 0x08 + 0x38 * (target_stream-1); /* position in target header*/

        /* 0x00  ? (00/01/02) */
        /* 0x04  sometimes global offset to wave_name */
        type = read_8bit(chunk_offset+0x08,streamHeader);
        channels = read_8bit(chunk_offset+0x09,streamHeader);
        /* 0x0a  null */
        sample_rate = read_32bitLE(chunk_offset+0x0c,streamHeader);

        /* 0x10  info_type: meaning of the next value
         *  (00=null, 30/40=data size without padding (ADPCM, ATRAC3plus), 80/A0=block size (AC3) */
        /* 0x14  info_value (see above) */
        /* 0x18  unknown (ex. 0x0008/0010/3307/CC02/etc)x2 */
        /* 0x1c  null */

        num_samples = read_32bitLE(chunk_offset+0x20,streamHeader);
        loop_start_sample = read_32bitLE(chunk_offset+0x24,streamHeader);
        loop_end_sample = read_32bitLE(chunk_offset+0x28,streamHeader);
        data_size = read_32bitLE(chunk_offset+0x2c,streamHeader); /* stream size (without padding) / interleave (for type3) */

        if (is_sgx) {
            stream_offset = 0x0;
        } else{
            stream_offset = read_32bitLE(chunk_offset+0x30,streamHeader);
        }
        /* 0x34 SGX: unknown; SGD/SGH: stream size (with padding) / interleave */

        loop_flag = loop_start_sample!=0xffffffff && loop_end_sample!=0xffffffff;
        start_offset = data_offset + stream_offset;
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channels,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start_sample;
    vgmstream->loop_end_sample = loop_end_sample;
    vgmstream->num_streams = total_streams;
    vgmstream->meta_type = meta_SGXD;

    /* needs -1 to match RIFF AT3's loop chunk
     * (maybe SGXD = "loop before this sample" rather than "loop after this sample" as expected by vgmstream) */
    if (vgmstream->loop_end_sample > 0)
        vgmstream->loop_end_sample -= 1;

    switch (type) {
        case 0x03:      /* PS-ADPCM */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            if (is_sgx || is_sgb) {
                vgmstream->interleave_block_size = 0x10;
            } else { /* this only seems to happen with SFX */
                vgmstream->interleave_block_size = data_size;
            }

            break;

#ifdef VGM_USE_FFMPEG
        case 0x04: {    /* ATRAC3plus */
            ffmpeg_codec_data *ffmpeg_data;

            /* internally has a RIFF header; but the SGXD  header / sample rate has priority over it (may not match) */
            ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset, data_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* manually read skip_samples if FFmpeg didn't do it */
            if (ffmpeg_data->skipSamples <= 0) {
                off_t chunk_offset;
                size_t chunk_size, fact_skip_samples = 0;
                if (!find_chunk_le(streamFile, 0x66616374,start_offset+0xc,0, &chunk_offset,&chunk_size)) /* find "fact" */
                    goto fail;
                if (chunk_size == 0x8) {
                    fact_skip_samples  = read_32bitLE(chunk_offset+0x4, streamFile);
                } else if (chunk_size == 0xc) {
                    fact_skip_samples  = read_32bitLE(chunk_offset+0x8, streamFile);
                }
                ffmpeg_set_skip_samples(ffmpeg_data, fact_skip_samples);
            }
            /* SGXD loop/sample values are relative (without skip samples) vs RIFF (with skip samples), no need to adjust */

            break;
        }
#endif
        case 0x05:      /* Short PS-ADPCM */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x4;

            break;

#ifdef VGM_USE_FFMPEG
        case 0x06: {    /* AC3 */
            ffmpeg_codec_data *ffmpeg_data;

            ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset, data_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* manually set skip_samples if FFmpeg didn't do it */
            if (ffmpeg_data->skipSamples <= 0) {
                /* PS3 AC3 consistently has 256 encoder delay samples, and there are ~1000-2000 samples after num_samples.
                 * Skipping them marginally improves full loops in some Tokyo Jungle tracks (ex. a_1.sgd). */
                ffmpeg_set_skip_samples(ffmpeg_data, 256);
            }
            /* SGXD loop/sample values are relative (without skip samples), no need to adjust */

            break;
        }
#endif

        default:
            goto fail;
    }

    /* open the file for reading */
    if (!vgmstream_open_stream(vgmstream,streamFile,start_offset))
        goto fail;

    if (is_sgb && streamHeader) close_streamfile(streamHeader);
    return vgmstream;

fail:
    if (is_sgb && streamHeader) close_streamfile(streamHeader);
    close_vgmstream(vgmstream);
    return NULL;
}

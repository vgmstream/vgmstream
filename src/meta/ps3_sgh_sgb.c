#include "meta.h"
#include "../util.h"


/* utils to fix AT3 looping */
typedef struct {
    int32_t fact_samples;
    int32_t loop_start_sample;
    int32_t loop_end_sample;
    int32_t skip_samples;
} at3_riff_info;
static int get_at3_riff_info(at3_riff_info* info, STREAMFILE *streamFile, int32_t offset);


/* Sony's SGB+SGH / SGD / SGX (variations of the same format)
 *  PS3: Genji (SGX only), Folklore, Afrika, Tokyo Jungle
 *  PSP: Brave Story, Sarugetchu Sarusaru Daisakusen, Kurohyo 1/2
 *
 * Contains header + chunks, usually:
 *  WAVE: stream(s) header of ADPCM, AC3, ATRAC3plus, etc
 *  NAME: stream name(s)
 *  WSUR, WMRK, BUSS: unknown
 *  RGND, SEQD: unknown (related to SE)
 * Then data, containing the original header if applicable (ex. AT3 RIFF).
 * The SGDX header has priority over it (ex. some ATRAC3plus files have 48000 while the data RIFF 44100)
 */
VGMSTREAM * init_vgmstream_ps3_sgdx(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    STREAMFILE * streamHeader = NULL;

    off_t start_offset, data_offset, chunk_offset;
    size_t data_size;

    int is_sgx, is_sgb;
    int loop_flag, channels, type;
    int sample_rate, num_samples, loop_start_sample, loop_end_sample;

    int target_stream = 0, total_streams;


    /* check extension, case insensitive */
    if (!check_extensions(streamFile,"sgx,sgd,sgb"))
        goto fail;
    is_sgx = check_extensions(streamFile,"sgx");
    is_sgb = check_extensions(streamFile,"sgb");
    //is_sgd = check_extensions(streamFile,"sgd");

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


    /* WAVE chunk (size 0x10 + files * 0x38 + optional padding) */
    /* 0x04  SGX: unknown; SGD/SGH: chunk length,  0x08  null */
    if (is_sgx) { /* position after chunk+size */
        if (read_32bitBE(0x10,streamHeader) != 0x57415645) goto fail;  /* "WAVE" */
        chunk_offset = 0x18;
    } else {
        if (!find_chunk_le(streamHeader, 0x57415645,0x10,0, &chunk_offset,NULL)) goto fail; /* "WAVE" */
    }

    /* check multi-streams (usually only SE containers; Puppeteer) */
    total_streams = read_32bitLE(chunk_offset+0x04,streamHeader);
    if (target_stream == 0) target_stream = 1;
    if (target_stream > total_streams) goto fail;

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
            stream_offset = 0x0; /* TODO unknown (not seen multi SGX) */
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
    vgmstream->meta_type = meta_PS3_SGDX;

    switch (type) {
        case 0x03: /* PSX ADPCM */
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            if (is_sgx || is_sgb) {
                vgmstream->interleave_block_size = 0x10;
            } else { //todo this only seems to happen with SFX
                vgmstream->interleave_block_size = data_size;
            }

            break;

#ifdef VGM_USE_FFMPEG
        case 0x04: { /* ATRAC3plus */
            at3_riff_info info;
            ffmpeg_codec_data *ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset, data_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            /* manually fix looping due to FFmpeg bugs */
            if (loop_flag && get_at3_riff_info(&info, streamFile, start_offset)) {
                if (vgmstream->num_samples == info.fact_samples) { /* use if looks normal */
                    /* todo use "skip samples"; for now we just use absolute loop values */
                    vgmstream->loop_start_sample = info.loop_start_sample;
                    vgmstream->loop_end_sample = info.loop_end_sample;
                    vgmstream->num_samples += info.skip_samples; /* to ensure it always reaches loop_end */
                }
            }

            break;
        }
#endif
        case 0x05: /* Short VAG ADPCM */
            vgmstream->coding_type = coding_PSX_cfg;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x4;

            break;

#ifdef VGM_USE_FFMPEG
        case 0x06: { /* AC3 */
            ffmpeg_codec_data *ffmpeg_data = init_ffmpeg_offset(streamFile, start_offset, data_size);
            if ( !ffmpeg_data ) goto fail;
            vgmstream->codec_data = ffmpeg_data;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

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



/**
 * AT3 RIFF headers have a "skip samples at the beginning" value that the decoder should use,
 * and absolute loop values. However the SGDX header loop values assume those samples are skipped.
 *
 * FFmpeg doesn't support/export this, so we have to manually get the absolute values to fix looping.
 */
static int get_at3_riff_info(at3_riff_info* info, STREAMFILE *streamFile, int32_t offset) {
    off_t chunk_offset;
    size_t chunk_size;

    memset(info, 0, sizeof(at3_riff_info));

    if (read_32bitBE(offset+0x0,streamFile)!=0x52494646 /* "RIFF" */
            && read_32bitBE(offset+0x8,streamFile)!=0x57415645 ) /* "WAVE" */
        goto fail;

    /*"smpl"*/
    if (!find_chunk_le(streamFile, 0x736D706C,offset+0xc,0, &chunk_offset,&chunk_size)) goto fail;
    if (read_32bitLE(chunk_offset+0x1C, streamFile)==0
            || read_32bitLE(chunk_offset+0x24+0x4, streamFile)!=0 )
        goto fail;
    info->loop_start_sample = read_32bitLE(chunk_offset+0x1C+0x8+0x8, streamFile);
    info->loop_end_sample = read_32bitLE(chunk_offset+0x1C+0x8+0xc,streamFile);

    /*"fact"*/
    if (!find_chunk_le(streamFile, 0x66616374,offset+0xc,0, &chunk_offset,&chunk_size)) goto fail;
    if (chunk_size == 0x8) {
        info->fact_samples = read_32bitLE(chunk_offset+0x0, streamFile);
        info->skip_samples = read_32bitLE(chunk_offset+0x4, streamFile);
    } else if (chunk_size == 0xc) {
        info->fact_samples = read_32bitLE(chunk_offset+0x0, streamFile);
        info->skip_samples = read_32bitLE(chunk_offset+0x8, streamFile);
    } else {
        goto fail;
    }

    /* found */
    return 1;

fail:
    /* not found */
    return 0;

}

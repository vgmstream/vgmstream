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
    char filename[PATH_LIMIT];

    off_t start_offset, data_offset;

    int i;
    int is_sgx, is_sgd, is_sgb;

    int chunk_offset;
    int total_streams;
    int target_stream = 0; /* usually only SE use substreams */

#ifdef VGM_USE_FFMPEG
    ffmpeg_codec_data *ffmpeg_data = NULL;
#endif


    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    is_sgx = strcasecmp("sgx",filename_extension(filename))==0;
    is_sgd = strcasecmp("sgd",filename_extension(filename))==0;
    is_sgb = strcasecmp("sgb",filename_extension(filename))==0;
    if ( !(is_sgx || is_sgd || is_sgb) )
        goto fail;

    /* SGB+SGH: use SGH as header; otherwise use the current file as header */
    if (is_sgb) {
        char fileheader[PATH_LIMIT];

        strcpy(fileheader,filename);
        strcpy(fileheader+strlen(fileheader)-3,"sgh");

        streamHeader = streamFile->open(streamFile,fileheader,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!streamHeader) goto fail;
    } else {
        streamHeader = streamFile;
    }


    /* SGXD chunk (size 0x10) */
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


    chunk_offset = 0x10;
    /* WAVE chunk (size 0x10 + files * 0x38 + optional padding) */
    /*  the format reads chunks until header_size, but we only want WAVE in the first position meaning BGM */
    if (read_32bitBE(chunk_offset+0x00,streamHeader) != 0x57415645)  /* "WAVE" */
        goto fail;
    /* 0x04  SGX: unknown; SGD/SGH: chunk length */
    /* 0x08  null */

    /* usually only SE containers have multiple streams but just in case... */
    total_streams = read_32bitLE(chunk_offset+0x0c,streamHeader);
    if (target_stream+1 > total_streams)
        goto fail;

    /* read stream (skip until target_stream) */
    chunk_offset += 0x10;
    {
        int stream_loop_flag;
        int stream_type;
        int stream_channels;
        int32_t stream_sample_rate;
        int32_t stream_num_samples, stream_loop_start_sample, stream_loop_end_sample;
        off_t stream_start_offset;
        int32_t stream_size;

        for (i=0; i < total_streams; i++) {
            if (i != target_stream) {
                chunk_offset += 0x38; /* next file */
                continue;
            }

            /* 0x00  ? (00/01/02) */
            /* 0x04  sometimes global offset to wave_name */
            stream_type = read_8bit(chunk_offset+0x08,streamHeader);
            stream_channels = read_8bit(chunk_offset+0x09,streamHeader);
            /* 0x0a  null */
            stream_sample_rate = read_32bitLE(chunk_offset+0x0c,streamHeader);

            /* 0x10  info_type: meaning of the next value
             *  (00=null, 30/40=data size without padding (ADPCM, ATRAC3plus), 80/A0=block size (AC3) */
            /* 0x14  info_value (see above) */
            /* 0x18  unknown (ex. 0x0008/0010/3307/CC02/etc)x2 */
            /* 0x1c  null */

            stream_num_samples = read_32bitLE(chunk_offset+0x20,streamHeader);
            stream_loop_start_sample = read_32bitLE(chunk_offset+0x24,streamHeader);
            stream_loop_end_sample = read_32bitLE(chunk_offset+0x28,streamHeader);
            stream_size = read_32bitLE(chunk_offset+0x2c,streamHeader); /* stream size (without padding) / interleave (for type3) */

            if (is_sgx) {
                stream_start_offset = 0x0; /* TODO unknown (not seen multi SGX) */
            } else{
                stream_start_offset = read_32bitLE(chunk_offset+0x30,streamHeader);
            }
            /* 0x34 SGX: unknown; SGD/SGH: stream size (with padding) / interleave */

            stream_loop_flag = stream_loop_start_sample!=0xffffffff && stream_loop_end_sample!=0xffffffff;
            chunk_offset += 0x38; /* next file */

            break;
        }

        /* build the VGMSTREAM */
        vgmstream = allocate_vgmstream(stream_channels,stream_loop_flag);
        if (!vgmstream) goto fail;

        /* fill in the vital statistics */
        vgmstream->num_samples = stream_num_samples;
        vgmstream->sample_rate = stream_sample_rate;
        vgmstream->channels = stream_channels;
        if (stream_loop_flag) {
            vgmstream->loop_start_sample = stream_loop_start_sample;
            vgmstream->loop_end_sample = stream_loop_end_sample;
        }

        vgmstream->meta_type = meta_PS3_SGDX;

        switch (stream_type) {
            case 0x03: /* PSX ADPCM */
                vgmstream->coding_type = coding_PSX;
                vgmstream->layout_type = layout_interleave;
                if (is_sgx) {
                    vgmstream->interleave_block_size = 0x10;
                } else {
                    vgmstream->interleave_block_size = stream_size;
                }

                break;

#ifdef VGM_USE_FFMPEG
            case 0x04: /* ATRAC3plus */
            {
                at3_riff_info info;

                ffmpeg_data = init_ffmpeg_offset(streamFile, data_offset, streamFile->get_size(streamFile));
                if ( !ffmpeg_data ) goto fail;

                vgmstream->coding_type = coding_FFmpeg;
                vgmstream->layout_type = layout_none;
                /*vgmstream->meta_type = meta_FFmpeg;*/
                vgmstream->codec_data = ffmpeg_data;

                /* manually fix looping due to FFmpeg bugs */
                if (stream_loop_flag && get_at3_riff_info(&info, streamFile, data_offset)) {
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
                vgmstream->coding_type = coding_SHORT_VAG_ADPCM;
                vgmstream->layout_type = layout_interleave;
                vgmstream->interleave_block_size = 0x4;

                break;

#ifdef VGM_USE_FFMPEG
            case 0x06: /* AC3 */
                ffmpeg_data = init_ffmpeg_offset(streamFile, data_offset, streamFile->get_size(streamFile));
                if ( !ffmpeg_data ) goto fail;

                vgmstream->coding_type = coding_FFmpeg;
                vgmstream->layout_type = layout_none;
                /*vgmstream->meta_type = meta_FFmpeg;*/
                vgmstream->codec_data = ffmpeg_data;

                break;
#endif

            default:
                goto fail;
        }


        start_offset = data_offset + stream_start_offset;
        /* open the file for reading */
        {
            int i;
            STREAMFILE * file;
            file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
            if (!file) goto fail;

            for (i=0; i < vgmstream->channels; i++) {
                vgmstream->ch[i].streamfile = file;
                vgmstream->ch[i].channel_start_offset =
                        vgmstream->ch[i].offset =
                                start_offset + vgmstream->interleave_block_size * i;
            }
        }
    }


    return vgmstream;

fail:
#ifdef VGM_USE_FFMPEG
    if (ffmpeg_data) {
        free_ffmpeg(ffmpeg_data);
        vgmstream->codec_data = NULL;
    }
#endif
    if (is_sgb && streamHeader) close_streamfile(streamHeader);
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}



/**
 * AT3 RIFF headers have a "skip samples at the beginning" value that the decoder should use,
 * and absolute loop values. However the SGDX header loop values assume those samples are skipped.
 *
 * FFmpeg doesn't support/export this, so we have to manually get the absolute values to fix looping.
 */
static int get_at3_riff_info(at3_riff_info* info, STREAMFILE *streamFile, int32_t offset) {
    off_t current_chunk, riff_size;
    int data_found = 0;

    memset(info, 0, sizeof(at3_riff_info));

    if (read_32bitBE(offset+0x0,streamFile)!=0x52494646 /* "RIFF" */
            && read_32bitBE(offset+0x8,streamFile)!=0x57415645 ) /* "WAVE" */
        goto fail;


    /* read chunks */
    riff_size = read_32bitLE(offset+0x4,streamFile);
    current_chunk = offset + 0xc;

    while (!data_found && current_chunk < riff_size) {
        uint32_t chunk_type = read_32bitBE(current_chunk,streamFile);
        off_t chunk_size = read_32bitLE(current_chunk+4,streamFile);

        switch(chunk_type) {
            case 0x736D706C:    /* smpl */
                if (read_32bitLE(current_chunk+0x24, streamFile)==0
                        || read_32bitLE(current_chunk+0x2c+0x4, streamFile)!=0 )
                    goto fail;

                info->loop_start_sample = read_32bitLE(current_chunk+0x2c+0x8, streamFile);
                info->loop_end_sample = read_32bitLE(current_chunk+0x2c+0xc,streamFile);
                break;

            case 0x66616374:    /* fact */
                if (chunk_size == 0x8) {
                    info->fact_samples = read_32bitLE(current_chunk+0x8, streamFile);
                    info->skip_samples = read_32bitLE(current_chunk+0xc, streamFile);
                } else if (chunk_size == 0xc) {
                    info->fact_samples = read_32bitLE(current_chunk+0x8, streamFile);
                    info->skip_samples = read_32bitLE(current_chunk+0x10, streamFile);
                } else {
                    goto fail;
                }
                break;

            case 0x64617461:    /* data */
                data_found = 1;
                break;

            default:
                break;
        }

        current_chunk += 8+chunk_size;
    }

    if (!data_found)
        goto fail;


    /* found */
    return 1;

fail:
    /* not found */
    return 0;

}

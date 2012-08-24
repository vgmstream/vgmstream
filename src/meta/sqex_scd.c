#include "meta.h"
#include "../coding/coding.h"
#include "../util.h"

/* Square-Enix SCD (FF XIII, XIV) */

/* special streamfile type to handle deinterleaving of complete files,
  (based heavily on AIXSTREAMFILE */
typedef struct _SCDINTSTREAMFILE
{
  STREAMFILE sf;
  STREAMFILE *real_file;
  const char * filename;
  off_t start_physical_offset;
  off_t current_logical_offset;
  off_t interleave_block_size;
  off_t stride_size;
  size_t total_size;
} SCDINTSTREAMFILE;

static STREAMFILE *open_scdint_with_STREAMFILE(STREAMFILE *file, const char * filename, off_t start_offset, off_t interleave_block_size, off_t stride_size, size_t total_size);

VGMSTREAM * init_vgmstream_sqex_scd(STREAMFILE *streamFile) {
    VGMSTREAM * vgmstream = NULL;
    char filename[260];
    off_t start_offset, meta_offset_offset, meta_offset, post_meta_offset, size_offset;
    int32_t loop_start, loop_end;

    int loop_flag = 0;
	int channel_count;
    int codec_id;
    int aux_chunk_count;

    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;

    /* check extension, case insensitive */
    streamFile->get_name(streamFile,filename,sizeof(filename));
    if (strcasecmp("scd",filename_extension(filename))) goto fail;

    /* SEDB */
    if (read_32bitBE(0,streamFile) != 0x53454442) goto fail;
    /* SSCF */
    if (read_32bitBE(4,streamFile) != 0x53534346) goto fail;
    if (read_32bitBE(8,streamFile) == 2 ||
        read_32bitBE(8,streamFile) == 3) {
        /* version 2 BE, as seen in FFXIII demo for PS3 */
        /* version 3 BE, as seen in FFXIII for PS3 */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
        size_offset = 0x14;
        meta_offset_offset = 0x40 + read_16bit(0xe,streamFile);
    } else if (read_32bitLE(8,streamFile) == 3 ||
               read_32bitLE(8,streamFile) == 2) {
        /* version 2/3 LE, as seen in FFXIV for ?? */
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
        size_offset = 0x10;
        meta_offset_offset = 0x40 + read_16bit(0xe,streamFile);
    } else goto fail;

    /* never mind, FFXIII music_68tak.ps3.scd is 0x80 shorter */
#if 0
    /* check file size with header value */
    if (read_32bit(size_offset,streamFile) != get_streamfile_size(streamFile))
        goto fail;
#endif

    meta_offset = read_32bit(meta_offset_offset,streamFile);

    /* check that chunk size equals stream size (?) */
    loop_start = read_32bit(meta_offset+0x10,streamFile);
    loop_end = read_32bit(meta_offset+0x14,streamFile);
    loop_flag = (loop_end > 0);

    channel_count = read_32bit(meta_offset+4,streamFile);
    codec_id = read_32bit(meta_offset+0xc,streamFile);

    post_meta_offset = meta_offset + 0x20;

    /* data at meta_offset is only 0x20 bytes, but there may be auxiliary chunks
       before anything else */

    aux_chunk_count = read_32bit(meta_offset+0x1c,streamFile);
    for (; aux_chunk_count > 0; aux_chunk_count --)
    {
        /* skip aux chunks */
        /*printf("skipping %08x\n", read_32bitBE(post_meta_offset, streamFile));*/
        post_meta_offset += read_32bit(post_meta_offset+4,streamFile);
    }

    start_offset = post_meta_offset + read_32bit(meta_offset+0x18,streamFile);

#ifdef VGM_USE_VORBIS
    if (codec_id == 0x6)
    {
        vgm_vorbis_info_t inf;
        uint32_t seek_table_size = read_32bit(post_meta_offset+0x10, streamFile);
        uint32_t vorb_header_size = read_32bit(post_meta_offset+0x14, streamFile);
        VGMSTREAM * result = NULL;

        memset(&inf, 0, sizeof(inf));
        inf.loop_start = loop_start;
        inf.loop_end = loop_end;
        inf.loop_flag = loop_flag;
        inf.loop_end_found = loop_flag;
        inf.loop_length_found = 0;
        inf.layout_type = layout_ogg_vorbis;
        inf.meta_type = meta_SQEX_SCD;

        result = init_vgmstream_ogg_vorbis_callbacks(streamFile, filename, NULL, start_offset, &inf);

        if (result != NULL) {
            return result;
        }

        // try skipping seek table
        {
            if ((post_meta_offset-meta_offset) + seek_table_size + vorb_header_size != read_32bit(meta_offset+0x18, streamFile)) {
                return NULL;
            }

            start_offset = post_meta_offset + 0x20 + seek_table_size;
            result = init_vgmstream_ogg_vorbis_callbacks(streamFile, filename, NULL, start_offset, &inf);
            if (result != NULL) {
                return result;
            }
        }

        // failed with Ogg, try deobfuscating header
        {
            // skip chunks before xor_byte
            unsigned char xor_byte;

            xor_byte = read_8bit(post_meta_offset+2, streamFile);

            if (xor_byte == 0) {
                return NULL;
            }

            inf.scd_xor = xor_byte;
            inf.scd_xor_len = vorb_header_size;

            result = init_vgmstream_ogg_vorbis_callbacks(streamFile, filename, NULL, start_offset, &inf);
            return result;
        }
    }
#endif
    
	/* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

	/* fill in the vital statistics */
	vgmstream->channels = channel_count;
    vgmstream->sample_rate = read_32bit(meta_offset+8,streamFile);

    switch (codec_id) {
        case 0x1:
            /* PCM */
            vgmstream->coding_type = coding_PCM16LE_int;
            vgmstream->layout_type = layout_none;
            vgmstream->num_samples = read_32bit(meta_offset+0,streamFile) / 2 / channel_count;

            if (loop_flag) {
                vgmstream->loop_start_sample = loop_start / 2 / channel_count;
                vgmstream->loop_end_sample = loop_end / 2 / channel_count;
            }
            break;
#ifdef VGM_USE_MPEG
        case 0x7:
            /* MPEG */
            {
                mpeg_codec_data *mpeg_data = NULL;
                struct mpg123_frameinfo mi;
                coding_t ct;

                mpeg_data = init_mpeg_codec_data(streamFile, start_offset, vgmstream->sample_rate, vgmstream->channels, &ct, NULL, NULL);
                if (!mpeg_data) goto fail;
                vgmstream->codec_data = mpeg_data;

                if (MPG123_OK != mpg123_info(mpeg_data->m, &mi)) goto fail;

                vgmstream->coding_type = ct;
                vgmstream->layout_type = layout_mpeg;
                if (mi.vbr != MPG123_CBR) goto fail;
                vgmstream->num_samples = mpeg_bytes_to_samples(read_32bit(meta_offset+0,streamFile), &mi);
                vgmstream->num_samples -= vgmstream->num_samples%576;
                if (loop_flag) {
                    vgmstream->loop_start_sample = mpeg_bytes_to_samples(loop_start, &mi);
                    vgmstream->loop_start_sample -= vgmstream->loop_start_sample%576;
                    vgmstream->loop_end_sample = mpeg_bytes_to_samples(loop_end, &mi);
                    vgmstream->loop_end_sample -= vgmstream->loop_end_sample%576;
                }
                vgmstream->interleave_block_size = 0;
            }
            break;
#endif
        case 0xC:
            /* MS ADPCM */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = read_16bit(post_meta_offset+0xc,streamFile);
            vgmstream->num_samples = msadpcm_bytes_to_samples(read_32bit(meta_offset+0,streamFile), vgmstream->interleave_block_size, vgmstream->channels);

            if (loop_flag) {
                vgmstream->loop_start_sample = msadpcm_bytes_to_samples(loop_start, vgmstream->interleave_block_size, vgmstream->channels);
                vgmstream->loop_end_sample = msadpcm_bytes_to_samples(loop_end, vgmstream->interleave_block_size, vgmstream->channels);
            }
            break;
        case 0xA:
            /* GC/Wii DSP ADPCM */
            {
                STREAMFILE * file;
                int i;
                const off_t interleave_size = 0x800;
                const off_t stride_size = interleave_size * channel_count;

                size_t total_size;

                scd_int_codec_data * data = NULL;

                vgmstream->coding_type = coding_NGC_DSP;
                vgmstream->layout_type = layout_scd_int;

                /* a normal DSP header... */
                vgmstream->num_samples = read_32bitBE(start_offset+0,streamFile);
                total_size = (read_32bitBE(start_offset+4,streamFile)+1)/2;

                if (loop_flag) {
                    vgmstream->loop_start_sample = loop_start;
                    vgmstream->loop_end_sample = loop_end+1;
                }

                /* verify other channel headers */
                for (i = 1; i < channel_count; i++) {
                    if (read_32bitBE(start_offset+interleave_size*i+0,streamFile) != vgmstream->num_samples ||
                        (read_32bitBE(start_offset+4,streamFile)+1)/2 != total_size) {
                        goto fail;
                    }

                }

                /* the primary streamfile we'll be using */
                file = streamFile->open(streamFile,filename,stride_size);
                if (!file)
                    goto fail;

                vgmstream->ch[0].streamfile = file;

                data = malloc(sizeof(scd_int_codec_data));
                data->substream_count = channel_count;
                data->substreams = calloc(channel_count, sizeof(VGMSTREAM *));
                data->intfiles = calloc(channel_count, sizeof(STREAMFILE *));

                vgmstream->codec_data = data;

                for (i=0;i<channel_count;i++) {
                    STREAMFILE * intfile =
                        open_scdint_with_STREAMFILE(file, "ARBITRARY.DSP", start_offset+interleave_size*i, interleave_size, stride_size, total_size);

                    data->substreams[i] = init_vgmstream_ngc_dsp_std(intfile);
                    data->intfiles[i] = intfile;
                    if (!data->substreams[i])
                        goto fail;

                    /* TODO: only handles mono substreams, though that's all we have with DSP */
                    /* save start things so we can restart for seeking/looping */
                    /* copy the channels */
                    memcpy(data->substreams[i]->start_ch,data->substreams[i]->ch,sizeof(VGMSTREAMCHANNEL)*1);
                    /* copy the whole VGMSTREAM */
                    memcpy(data->substreams[i]->start_vgmstream,data->substreams[i],sizeof(VGMSTREAM));

                }

            }
            break;
        default:
            goto fail;
    }

    vgmstream->meta_type = meta_SQEX_SCD;

    /* open the file for reading */
    if (vgmstream->layout_type != layout_scd_int)
    {
        int i;
        STREAMFILE * file;
        file = streamFile->open(streamFile,filename,STREAMFILE_DEFAULT_BUFFER_SIZE);
        if (!file) goto fail;
        for (i=0;i<channel_count;i++) {
            vgmstream->ch[i].streamfile = file;

            vgmstream->ch[i].channel_start_offset=
                vgmstream->ch[i].offset=start_offset;

        }
    }

    return vgmstream;

    /* clean up anything we may have opened */
fail:
    if (vgmstream) close_vgmstream(vgmstream);
    return NULL;
}

static STREAMFILE *open_scdint_impl(SCDINTSTREAMFILE *streamfile,const char * const filename,size_t buffersize) 
{
    SCDINTSTREAMFILE *newfile;

    if (strcmp(filename, streamfile->filename))
        return NULL;

    newfile = malloc(sizeof(SCDINTSTREAMFILE));
    if (!newfile)
        return NULL;

    memcpy(newfile,streamfile,sizeof(SCDINTSTREAMFILE));
    return &newfile->sf;
}

static void close_scdint(SCDINTSTREAMFILE *streamfile)
{
    free(streamfile);
    return;
}

static size_t get_size_scdint(SCDINTSTREAMFILE *streamfile)
{
    return streamfile->total_size;
}

static size_t get_offset_scdint(SCDINTSTREAMFILE *streamfile)
{
    return streamfile->current_logical_offset;
}

static void get_name_scdint(SCDINTSTREAMFILE *streamfile, char *buffer, size_t length)
{
    strncpy(buffer,streamfile->filename,length);
    buffer[length-1]='\0';
}

static size_t read_scdint(SCDINTSTREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length)
{
    size_t sz = 0;

    while (length > 0)
    {
        off_t to_read;
        off_t length_available;
        off_t block_num;
        off_t intrablock_offset;
        off_t physical_offset;


        block_num = offset / streamfile->interleave_block_size;
        intrablock_offset = offset % streamfile->interleave_block_size;
        streamfile->current_logical_offset = offset;
        physical_offset = streamfile->start_physical_offset + block_num * streamfile->stride_size + intrablock_offset;

        length_available =
                streamfile->interleave_block_size - intrablock_offset;

        if (length < length_available)
        {
            to_read = length;
        }
        else
        {
            to_read = length_available;
        }

        if (to_read > 0)
        {
            size_t bytes_read;

            bytes_read = read_streamfile(dest,
                physical_offset,
                to_read, streamfile->real_file);

            sz += bytes_read;

            streamfile->current_logical_offset = offset + bytes_read;

            if (bytes_read != to_read)
            {
                /* an error which we will not attempt to handle here */
                return sz;
            }

            dest += bytes_read;
            offset += bytes_read;
            length -= bytes_read;
        }
    }

    return sz;
}

/* start_offset is for *this* interleaved stream */
static STREAMFILE *open_scdint_with_STREAMFILE(STREAMFILE *file, const char * filename, off_t start_offset, off_t interleave_block_size, off_t stride_size, size_t total_size)
{
    SCDINTSTREAMFILE * scd = malloc(sizeof(SCDINTSTREAMFILE));
    
    if (!scd)
        return NULL;

    scd->sf.read = (void*)read_scdint;
    scd->sf.get_size = (void*)get_size_scdint;
    scd->sf.get_offset = (void*)get_offset_scdint;
    scd->sf.get_name = (void*)get_name_scdint;
    scd->sf.get_realname = (void*)get_name_scdint;
    scd->sf.open = (void*)open_scdint_impl;
    scd->sf.close = (void*)close_scdint;

    scd->real_file = file;
    scd->filename = filename;
    scd->start_physical_offset = start_offset;
    scd->current_logical_offset = 0;
    scd->interleave_block_size = interleave_block_size;
    scd->stride_size = stride_size;
    scd->total_size = total_size;

    return &scd->sf;
}

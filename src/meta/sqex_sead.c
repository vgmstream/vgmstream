#include "meta.h"
#include "../coding/coding.h"


static STREAMFILE* setup_sead_hca_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, int encryption, size_t header_size, size_t key_start);

/* SABF/MABF - Square Enix's "sead" audio games [Dragon Quest Builders (PS3), Dissidia Opera Omnia (mobile), FF XV (PS4)] */
VGMSTREAM * init_vgmstream_sqex_sead(STREAMFILE * streamFile) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset, tables_offset, mtrl_offset, meta_offset, extradata_offset; //, info_offset, name_offset = 0;
    size_t stream_size, descriptor_size, extradata_size, special_size; //, name_size = 0;


    int loop_flag = 0, channel_count, codec, sample_rate, loop_start, loop_end;
    int is_sab = 0, is_mab = 0;
    int total_subsongs, target_subsong = streamFile->stream_index;

    int32_t (*read_32bit)(off_t,STREAMFILE*) = NULL;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = NULL;


    /* check extensions (.sab: sound/bgm, .mab: music, .sbin: Dissidia Opera Omnia .sab) */
    if ( !check_extensions(streamFile,"sab,mab,sbin"))
        goto fail;


    /** main header **/
    if (read_32bitBE(0x00,streamFile) == 0x73616266) { /* "sabf" */
        is_sab = 1;
    } else if (read_32bitBE(0x00,streamFile) == 0x6D616266) { /* "mabf" */
        is_mab = 1;
    } else {
        goto fail;
    }

    //if (read_8bit(0x04,streamFile) != 0x02)  /* version? */
    //    goto fail;
    /* 0x04(1): version? (usually 0x02, rarely 0x01, ex FF XV title) */
    /* 0x05(1): 0x00/01? */
    /* 0x06(2): version? (usually 0x10, rarely 0x20) */
    if (read_16bitBE(0x06,streamFile) < 0x100) { /* use some value as no apparent flag */
        read_32bit = read_32bitBE;
        read_16bit = read_16bitBE;
    } else {
        read_32bit = read_32bitLE;
        read_16bit = read_16bitLE;
    }
    /* 0x08(1): version 0x04?, 0x0a(2): ?  */
    descriptor_size = read_8bit(0x09,streamFile);

    if (read_32bit(0x0c,streamFile) != get_streamfile_size(streamFile))
        goto fail;
    /* 0x10(n): file descriptor ("BGM", "Music", "SE", etc, long names are ok), padded */
    tables_offset = 0x10 + (descriptor_size + 0x01); /* string null seems counted for padding */
    if (tables_offset % 0x10)
        tables_offset += 0x10 - (tables_offset % 0x10);


    /** offset tables **/
    if (is_sab) {
        if (read_32bitBE(tables_offset+0x00,streamFile) != 0x736E6420) goto fail; /* "snd " (info) */
        if (read_32bitBE(tables_offset+0x10,streamFile) != 0x73657120) goto fail; /* "seq " (unknown) */
        if (read_32bitBE(tables_offset+0x20,streamFile) != 0x74726B20) goto fail; /* "trk " (unknown) */
        if (read_32bitBE(tables_offset+0x30,streamFile) != 0x6D74726C) goto fail; /* "mtrl" (headers/streams) */
      //info_offset = read_32bit(tables_offset+0x08,streamFile);
      //seq_offset  = read_32bit(tables_offset+0x18,streamFile);
      //trk_offset  = read_32bit(tables_offset+0x28,streamFile);
        mtrl_offset = read_32bit(tables_offset+0x38,streamFile);
    }
    else if (is_mab) {
        if (read_32bitBE(tables_offset+0x00,streamFile) != 0x6D757363) goto fail; /* "musc" (info) */
        if (read_32bitBE(tables_offset+0x10,streamFile) != 0x696E7374) goto fail; /* "inst" (unknown) */
        if (read_32bitBE(tables_offset+0x20,streamFile) != 0x6D74726C) goto fail; /* "mtrl" (headers/streams) */
      //info_offset = read_32bit(tables_offset+0x08,streamFile);
      //inst_offset = read_32bit(tables_offset+0x18,streamFile);
        mtrl_offset = read_32bit(tables_offset+0x28,streamFile);
    }
    else {
        goto fail;
    }
    /* each section starts with:
     * 0x00(2): 0x00/01?, 0x02: size? (0x10), 0x04(2): entries, 0x06+: padded to 0x10
     * 0x10+0x04*entry: offset from section start, also padded to 0x10 at the end */

    /* find meta_offset in mtrl and total subsongs */
    {
        int i;
        int entries = read_16bit(mtrl_offset+0x04,streamFile);
        off_t entries_offset = mtrl_offset + 0x10;

        if (target_subsong == 0) target_subsong = 1;
        total_subsongs = 0;
        meta_offset = 0;

        /* manually find subsongs as entries can be dummy (ex. sfx banks in Dissidia Opera Omnia) */
        for (i = 0; i < entries; i++) {
            off_t entry_offset = mtrl_offset + read_32bit(entries_offset + i*0x04,streamFile);

            if (read_8bit(entry_offset+0x05,streamFile) == 0)
                continue; /* codec 0 when dummy */

            total_subsongs++;
            if (!meta_offset && total_subsongs == target_subsong)
                meta_offset = entry_offset;
        }
        if (meta_offset == 0) goto fail;
        /* SAB can contain 0 entries too */
    }


    /** stream header **/
    /* 0x00(2): 0x00/01? */
    /* 0x02(2): base entry size? (0x20) */
    channel_count   =  read_8bit(meta_offset+0x04,streamFile);
    codec           =  read_8bit(meta_offset+0x05,streamFile);
  //entry_id        = read_16bit(meta_offset+0x06,streamFile);
    sample_rate     = read_32bit(meta_offset+0x08,streamFile);
    loop_start      = read_32bit(meta_offset+0x0c,streamFile); /* in samples but usually ignored */

    loop_end        = read_32bit(meta_offset+0x10,streamFile);
    extradata_size  = read_32bit(meta_offset+0x14,streamFile); /* including subfile header, can be 0 */
    stream_size     = read_32bit(meta_offset+0x18,streamFile); /* not including subfile header */
    special_size    = read_32bit(meta_offset+0x1c,streamFile);

    loop_flag       = (loop_end > 0);
    extradata_offset = meta_offset + 0x20;


    /** info section (get stream name) **/
    //if (is_sab) { //todo load name based on entry id
        /* "snd " */
        /* 0x08(2): file number within descriptor */
        /* 0x1a(2): base_entry size (-0x10?) */
        //name_size = read_32bit(snd_offset+0x20,streamFile);
        //name_offset = snd_offset+0x70;
        /* 0x24(4): unique id? (referenced in "seq" section) */
    //}
    //else if (is_mab) {
        /* "musc" */
        //looks like a "music cue" section, pointing to one subsection per "material".
        // ex. one cue may point to 3 named subsongs/sections.
        // some common header info from all materials is repeated (ex. sample rate), while other
        // (loops, maybe proper num_samples) are listed per material but don't always match thei header
    //}


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = sample_rate;
    vgmstream->num_streams = total_subsongs;
    vgmstream->stream_size = stream_size;
    vgmstream->meta_type = is_sab ? meta_SQEX_SAB : meta_SQEX_MAB;

    switch(codec) {

        case 0x01: { /* PCM [Chrono Trigger sfx (PC)] */
            start_offset = extradata_offset + extradata_size;

            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(stream_size, vgmstream->channels, 16);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample   = loop_end;
            break;
        }

        case 0x02: { /* MSADPCM [Dragon Quest Builders (Vita) sfx] */
            start_offset = extradata_offset + extradata_size;

            /* 0x00 (2): null?, 0x02(2): entry size? */
            vgmstream->coding_type = coding_MSADPCM;
            vgmstream->layout_type = layout_none;
            vgmstream->interleave_block_size = read_16bit(extradata_offset+0x04,streamFile);

            /* much like AKBs, there are slightly different loop values here, probably more accurate
             * (if no loop, loop_end doubles as num_samples) */
            vgmstream->num_samples = msadpcm_bytes_to_samples(stream_size, vgmstream->interleave_block_size, vgmstream->channels);
            vgmstream->loop_start_sample = read_32bit(extradata_offset+0x08, streamFile); //loop_start
            vgmstream->loop_end_sample   = read_32bit(extradata_offset+0x0c, streamFile); //loop_end
            break;
        }

#ifdef VGM_USE_VORBIS
        case 0x03: { /* OGG [Final Fantasy XV Benchmark sfx (PC)] */
            VGMSTREAM *ogg_vgmstream = NULL;
            ogg_vorbis_meta_info_t ovmi = {0};
            off_t subfile_offset = extradata_offset + extradata_size;

            ovmi.meta_type = vgmstream->meta_type;
            ovmi.total_subsongs = total_subsongs;
            ovmi.stream_size = stream_size;
            /* post header has some kind of repeated values, config/table? */

            ogg_vgmstream = init_vgmstream_ogg_vorbis_callbacks(streamFile, NULL, subfile_offset, &ovmi);
            if (ogg_vgmstream) {
                ogg_vgmstream->num_streams = vgmstream->num_streams;
                ogg_vgmstream->stream_size = vgmstream->stream_size;

                close_vgmstream(vgmstream);
                return ogg_vgmstream;
            }
            else {
                goto fail;
            }

            break;
        }
#endif

#ifdef VGM_USE_ATRAC9
        case 0x04: { /* ATRAC9 [Dragon Quest Builders (Vita), Final Fantaxy XV (PS4)] */
            atrac9_config cfg = {0};

            start_offset = extradata_offset + extradata_size;
            /* post header has various typical ATRAC9 values */
            cfg.channels = vgmstream->channels;
            cfg.config_data = read_32bit(extradata_offset+0x0c,streamFile);
            cfg.encoder_delay = read_32bit(extradata_offset+0x18,streamFile);

            vgmstream->codec_data = init_atrac9(&cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_ATRAC9;
            vgmstream->layout_type = layout_none;

            vgmstream->sample_rate = read_32bit(extradata_offset+0x1c,streamFile); /* SAB's sample rate can be different but it's ignored */
            vgmstream->num_samples = read_32bit(extradata_offset+0x10,streamFile); /* loop values above are also weird and ignored */
            vgmstream->loop_start_sample = read_32bit(extradata_offset+0x20, streamFile) - (loop_flag ? cfg.encoder_delay : 0); //loop_start
            vgmstream->loop_end_sample   = read_32bit(extradata_offset+0x24, streamFile) - (loop_flag ? cfg.encoder_delay : 0); //loop_end
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case 0x06: { /* MSF subfile (MPEG mode) [Dragon Quest Builders (PS3)] */
            mpeg_codec_data *mpeg_data = NULL;
            mpeg_custom_config cfg = {0};

            start_offset = extradata_offset + extradata_size;
            /* post header is a proper MSF, but sample rate/loops are ignored in favor of SAB's */

            mpeg_data = init_mpeg_custom(streamFile, start_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!mpeg_data) goto fail;
            vgmstream->codec_data = mpeg_data;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = mpeg_bytes_to_samples(stream_size, mpeg_data);
            vgmstream->loop_start_sample = loop_start;
            vgmstream->loop_end_sample = loop_end;
            break;
        }
#endif

        case 0x07: { /* HCA subfile [Dissidia Opera Omnia (Mobile), Final Fantaxy XV (PS4)] */
            //todo there is no easy way to use the HCA decoder; try subfile hack for now
            VGMSTREAM *temp_vgmstream = NULL;
            STREAMFILE *temp_streamFile = NULL;
            off_t subfile_offset = extradata_offset + 0x10;
            size_t subfile_size = stream_size + extradata_size - 0x10;

            /* post header: values from the HCA header, in file endianness + HCA header */
            size_t key_start = special_size & 0xff;
            size_t header_size = read_16bit(extradata_offset+0x02, streamFile);
            int encryption = read_16bit(extradata_offset+0x0c, streamFile); //maybe 8bit?
            /* encryption type 0x01 found in Final Fantasy XII TZA (PS4/PC) */

            temp_streamFile = setup_sead_hca_streamfile(streamFile, subfile_offset, subfile_size, encryption, header_size, key_start);
            if (!temp_streamFile) goto fail;

            temp_vgmstream = init_vgmstream_hca(temp_streamFile);
            if (temp_vgmstream) {
                /* loops can be slightly different (~1000 samples) but probably HCA's are more accurate */
                temp_vgmstream->num_streams = vgmstream->num_streams;
                temp_vgmstream->stream_size = vgmstream->stream_size;
                temp_vgmstream->meta_type = vgmstream->meta_type;

                close_streamfile(temp_streamFile);
                close_vgmstream(vgmstream);
                return temp_vgmstream;
            }
            else {
                close_streamfile(temp_streamFile);
                goto fail;
            }
        }

        case 0x00: /* dummy entry */
        default:
            VGM_LOG("SQEX SEAD: unknown codec %x\n", codec);
            goto fail;
    }

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


typedef struct {
    size_t header_size;
    size_t key_start;
} sead_decryption_data;

/* Encrypted HCA */
static size_t sead_decryption_read(STREAMFILE *streamfile, uint8_t *dest, off_t offset, size_t length, sead_decryption_data* data) {
    /* Found in FFXII_TZA.exe (same key in SCD Ogg V3) */
    static const uint8_t encryption_key[0x100] = {
        0x3A,0x32,0x32,0x32,0x03,0x7E,0x12,0xF7,0xB2,0xE2,0xA2,0x67,0x32,0x32,0x22,0x32, // 00-0F
        0x32,0x52,0x16,0x1B,0x3C,0xA1,0x54,0x7B,0x1B,0x97,0xA6,0x93,0x1A,0x4B,0xAA,0xA6, // 10-1F
        0x7A,0x7B,0x1B,0x97,0xA6,0xF7,0x02,0xBB,0xAA,0xA6,0xBB,0xF7,0x2A,0x51,0xBE,0x03, // 20-2F
        0xF4,0x2A,0x51,0xBE,0x03,0xF4,0x2A,0x51,0xBE,0x12,0x06,0x56,0x27,0x32,0x32,0x36, // 30-3F
        0x32,0xB2,0x1A,0x3B,0xBC,0x91,0xD4,0x7B,0x58,0xFC,0x0B,0x55,0x2A,0x15,0xBC,0x40, // 40-4F
        0x92,0x0B,0x5B,0x7C,0x0A,0x95,0x12,0x35,0xB8,0x63,0xD2,0x0B,0x3B,0xF0,0xC7,0x14, // 50-5F
        0x51,0x5C,0x94,0x86,0x94,0x59,0x5C,0xFC,0x1B,0x17,0x3A,0x3F,0x6B,0x37,0x32,0x32, // 60-6F
        0x30,0x32,0x72,0x7A,0x13,0xB7,0x26,0x60,0x7A,0x13,0xB7,0x26,0x50,0xBA,0x13,0xB4, // 70-7F
        0x2A,0x50,0xBA,0x13,0xB5,0x2E,0x40,0xFA,0x13,0x95,0xAE,0x40,0x38,0x18,0x9A,0x92, // 80-8F
        0xB0,0x38,0x00,0xFA,0x12,0xB1,0x7E,0x00,0xDB,0x96,0xA1,0x7C,0x08,0xDB,0x9A,0x91, // 90-9F
        0xBC,0x08,0xD8,0x1A,0x86,0xE2,0x70,0x39,0x1F,0x86,0xE0,0x78,0x7E,0x03,0xE7,0x64, // A0-AF
        0x51,0x9C,0x8F,0x34,0x6F,0x4E,0x41,0xFC,0x0B,0xD5,0xAE,0x41,0xFC,0x0B,0xD5,0xAE, // B0-BF
        0x41,0xFC,0x3B,0x70,0x71,0x64,0x33,0x32,0x12,0x32,0x32,0x36,0x70,0x34,0x2B,0x56, // C0-CF
        0x22,0x70,0x3A,0x13,0xB7,0x26,0x60,0xBA,0x1B,0x94,0xAA,0x40,0x38,0x00,0xFA,0xB2, // D0-DF
        0xE2,0xA2,0x67,0x32,0x32,0x12,0x32,0xB2,0x32,0x32,0x32,0x32,0x75,0xA3,0x26,0x7B, // E0-EF
        0x83,0x26,0xF9,0x83,0x2E,0xFF,0xE3,0x16,0x7D,0xC0,0x1E,0x63,0x21,0x07,0xE3,0x01, // F0-FF
    };
    size_t bytes_read;
    off_t encrypted_offset = data->header_size;
    int i;

    bytes_read = streamfile->read(streamfile, dest, offset, length);

    /* decrypt data (xor) */
    if (offset >= encrypted_offset) {
        for (i = 0; i < bytes_read; i++) {
            dest[i] ^= encryption_key[(data->key_start + (offset - encrypted_offset) + i) % 0x100];
        }
    }

    return bytes_read;
}

static STREAMFILE* setup_sead_hca_streamfile(STREAMFILE *streamFile, off_t subfile_offset, size_t subfile_size, int encryption, size_t header_size, size_t key_start) {
    STREAMFILE *temp_streamFile = NULL, *new_streamFile = NULL;

    /* setup subfile */
    new_streamFile = open_wrap_streamfile(streamFile);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    new_streamFile = open_clamp_streamfile(temp_streamFile, subfile_offset,subfile_size);
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    if (encryption) {
        sead_decryption_data io_data = {0};
        size_t io_data_size = sizeof(sead_decryption_data);

        io_data.header_size = header_size;
        io_data.key_start = key_start;

        new_streamFile = open_io_streamfile(temp_streamFile, &io_data,io_data_size, sead_decryption_read,NULL);
        if (!new_streamFile) goto fail;
        temp_streamFile = new_streamFile;
    }

    new_streamFile = open_fakename_streamfile(temp_streamFile, NULL,"hca");
    if (!new_streamFile) goto fail;
    temp_streamFile = new_streamFile;

    return temp_streamFile;

fail:
    close_streamfile(temp_streamFile);
    return NULL;
}

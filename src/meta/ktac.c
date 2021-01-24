#include "meta.h"
#include "../coding/coding.h"


typedef struct {
    int channels;
    int sample_rate;
    int loop_flag;
    int32_t num_samples;
    int32_t loop_start;
    int32_t loop_end;
    uint32_t file_size;
    uint32_t stream_offset;
    uint32_t stream_size;
    uint32_t table_offset;
    uint32_t table_entries;
    int type;
    int encoder_delay;
    int end_padding;
    int frame_samples;
} ktac_header_t;


static int make_m4a_header(uint8_t* buf, int buf_len, ktac_header_t* ktac, STREAMFILE* sf);

/* KTAC - Koei Tecmo custom AAC [Kin'iro no Corda 3 (Vita), Shingeki no Kyojin: Shichi kara no Dasshutsu (3DS), Dynasty Warriors (PS4)] */
VGMSTREAM* init_vgmstream_ktac(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    ktac_header_t ktac = {0};


    /* checks */
    /* .ktac: header id */
    if (!check_extensions(sf,"ktac"))
        goto fail;
    if (!is_id32be(0x00,sf, "KTAC"))
        goto fail;

    /* 0x04: version? (always 1) */
    ktac.file_size = read_u32le(0x08,sf);
    if (ktac.file_size != get_streamfile_size(sf))
        goto fail;
    ktac.stream_offset  = read_u32le(0x0c,sf);
    ktac.stream_size    = read_u32le(0x10,sf);
    ktac.type           = read_u32le(0x14,sf);
    ktac.sample_rate    = read_u32le(0x18,sf);
    ktac.num_samples    = read_u32le(0x1c,sf); /* full samples */
    ktac.channels       = read_u16le(0x20,sf);
    ktac.frame_samples  = read_u16le(0x22,sf);
    ktac.encoder_delay  = read_u16le(0x24,sf);
    ktac.end_padding    = read_u16le(0x26,sf);
    ktac.loop_start     = read_u32le(0x28,sf);
    ktac.loop_end       = read_u32le(0x2c,sf);
    /* 0x30: ? (big, related to loops) */
    /* 0x34: ? (always null) */
    ktac.table_offset = read_u32le(0x38,sf);
    ktac.table_entries= read_u32le(0x3c,sf);

    ktac.loop_flag = (ktac.loop_end > 0);

    /* type 1 files crash during sample_copy, wrong fake header/esds?
     * (0=AoT, KnC3 bgm, 1=KnC3 1ch voices, 2=DW4, Atelier Ryza) */
    if (ktac.type == 1)
        goto fail;


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(ktac.channels, ktac.loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_KTAC;
    vgmstream->sample_rate = ktac.sample_rate;
    vgmstream->num_samples = ktac.num_samples - ktac.encoder_delay - ktac.end_padding;
    vgmstream->loop_start_sample = ktac.loop_start * ktac.frame_samples - ktac.encoder_delay;
    vgmstream->loop_end_sample = ktac.loop_end * ktac.frame_samples - ktac.encoder_delay;

    /* KTAC uses AAC, but not type found in .aac (that has headered frames, like mp3) but raw
     * packets + frame size table (similar to .mp4/m4a). We make a fake M4A header to feed FFmpeg */
#ifdef VGM_USE_FFMPEG
    {
        ffmpeg_codec_data* ffmpeg_data = NULL;
        int bytes;
        uint8_t* buf = NULL;
        int buf_len = 0x400 + ktac.table_entries * 0x4;

        if (buf_len > 0x100000) /* ??? */
            goto fail;

        buf = malloc(buf_len);
        if (!buf) goto fail;

        bytes = make_m4a_header(buf, buf_len, &ktac, sf);
        ffmpeg_data = init_ffmpeg_header_offset(sf, buf, bytes, ktac.stream_offset, ktac.stream_size);
        free(buf);

        if (!ffmpeg_data) goto fail;
        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        /* not part of fake header since it's kinda complex to add (iTunes string comment) */
        ffmpeg_set_skip_samples(ffmpeg_data, ktac.encoder_delay);
    }
#else
    goto fail;
#endif

    return vgmstream;
fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* *********************************************************** */

/* Helpers for M4A headers, an insane soup of chunks (AKA "atoms").
 * Needs *A LOT* of atoms and fields so this is more elaborate than usual.
 * - https://developer.apple.com/library/archive/documentation/QuickTime/QTFF/QTFFPreface/qtffPreface.html
 */

/* generic additions */
typedef struct {
    uint8_t* out;
    int bytes;
} m4a_state_t;

typedef struct {
    STREAMFILE* sf;
    ktac_header_t* ktac;    /* config */
    uint8_t* out;           /* current position */
    int bytes;              /* written bytes */
    m4a_state_t chunks;     /* chunks offsets are absolute, save position until we know header size */
} m4a_header_t;

static void add_u32b(m4a_header_t* h, uint32_t value) {
    put_u32be(h->out, value);
    h->out += 0x04;
    h->bytes += 0x04;
}

static void add_u24b(m4a_header_t* h, uint32_t value) {
    put_u16be(h->out + 0x00, (value >> 8u) & 0xFFFF);
    put_u8   (h->out + 0x02, (value >> 0u) & 0xFF);
    h->out += 0x03;
    h->bytes += 0x03;
}

static void add_u16b(m4a_header_t* h, uint16_t value) {
    put_u16be(h->out, value);
    h->out += 0x02;
    h->bytes += 0x02;
}

static void add_u8(m4a_header_t* h, uint32_t value) {
    put_u8(h->out, value);
    h->out += 0x01;
    h->bytes += 0x01;
}

static void add_name(m4a_header_t* h, const char* name) {
    memcpy(h->out, name, 0x4);
    h->out += 0x04;
    h->bytes += 0x04;
}

static void add_atom(m4a_header_t* h, const char* name, uint32_t size) {
    add_u32b(h, size);
    add_name(h, name);
}

/* register + write final size for atoms of variable/complex size */
static void save_atom(m4a_header_t* h, m4a_state_t* s) {
    s->out = h->out;
    s->bytes = h->bytes;
}

static void load_atom(m4a_header_t* h, m4a_state_t* s) {
    put_u32be(s->out, h->bytes - s->bytes);
}

/* common atoms */

static void add_ftyp(m4a_header_t* h) {
    add_atom(h, "ftyp", 0x18);
    add_name(h, "M4A "); /* major brand */
    add_u32b(h, 512);    /* minor version */
    add_name(h, "isom"); /* compatible brands */
    add_name(h, "iso2"); /* compatible brands */
}

static void add_free(m4a_header_t* h) {
    add_atom(h, "free", 0x08);
}

static void add_mdat(m4a_header_t* h) {
    add_atom(h, "mdat", 0x08 + h->ktac->stream_size);
}

/* variable atoms */


static void add_stco(m4a_header_t* h) {
    add_atom(h, "stco", 0x10 + 1 * 0x04);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */
    add_u32b(h, 1);                         /* Number of entries */
    /* there may be an entry per frame, but only first seems needed */
    save_atom(h, &h->chunks);
    add_u32b(h, 0);                         /* Absolute offset N */
}
static void add_stsz(m4a_header_t* h) {
    int i;

    add_atom(h, "stsz", 0x14 + h->ktac->table_entries * 0x04);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */
    add_u32b(h, 0);                         /* Sample size (CBR) */
    add_u32b(h, h->ktac->table_entries);    /* Number of entries (VBR) */
    for (i = 0; i < h->ktac->table_entries; i++) { 
        uint32_t size = read_u32le(h->ktac->table_offset + i*0x04, h->sf);
        add_u32b(h, size);                  /* Sample N */
    }
}

static void add_stsc(m4a_header_t* h) {
    add_atom(h, "stsc", 0x1c);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */
    add_u32b(h, 1);                         /* Number of entries */
    add_u32b(h, 1);                         /* First chunk */
    add_u32b(h, h->ktac->table_entries);    /* Samples per chunk */
    add_u32b(h, 1);                         /* Sample description ID */
}

static void add_stts(m4a_header_t* h) {
    add_atom(h, "stts", 0x18);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */
    add_u32b(h, 1);                         /* Number of entries */
    add_u32b(h, h->ktac->table_entries);    /* Sample count */
    add_u32b(h, h->ktac->frame_samples);    /* Sample duration */
}

/* from mpeg4audio.c (also see ff_mp4_read_dec_config_descr) */
static const int m4a_sample_rates[16] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350
};
static const uint8_t m4a_channels[14] = {
    0,
    1, // mono (1/0)
    2, // stereo (2/0)
    3, // 3/0
    4, // 3/1
    5, // 3/2
    6, // 3/2.1
    8, // 5/2.1
    //0,
    //0,
    //0,
    //7, // 3/3.1
    //8, // 3/2/2.1
    //24 // 3/3/3 - 5/2/3 - 3/0/0.2
};

static void add_esds(m4a_header_t* h) {
    uint16_t config = 0;
    
    /* ES_descriptor (TLV format see ISO 14496-1) and DecSpecificInfoTag define actual decoding
     - config (channels/rate/etc), other atoms with the same stuff is just info
     * - http://ecee.colorado.edu/~ecen5653/ecen5653/papers/ISO%2014496-1%202004.PDF */

    {
        uint8_t object_type = 0x02; /* 0x00=none, 0x01=AAC main, 0x02=AAC LC */
        uint8_t sr_index = 0;
        uint8_t ch_index = 0;
        uint8_t unknown = 0;
        int i;
        for (i = 0; i < 16; i++) {
            if (m4a_sample_rates[i] == h->ktac->sample_rate) {
                sr_index = i;
                break;
            }
        }
        for (i = 0; i < 8; i++) {
            if (m4a_channels[i] == h->ktac->channels) {
                ch_index = i;
                break;
            }
        }

        config |= (object_type & 0x1F) << 11; /* 5b */
        config |= (sr_index & 0x0F) << 7; /* 4b */
        config |= (ch_index & 0x0F) << 3; /* 4b */
        config |= (unknown & 0x07) << 0; /* 3b */
    }

    add_atom(h, "esds", 0x33);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */

    add_u8  (h, 0x03);                      /* ES_DescrTag */
    add_u32b(h, 0x80808022);                /* size 0x22 */
    add_u16b(h, 0x0000);                    /* stream Id */
    add_u8  (h, 0x00);                      /* flags */

    add_u8  (h, 0x04);                      /* DecoderConfigDescrTag */
    add_u32b(h, 0x80808014);                /* size 0x14 */
    add_u8  (h, 0x40);                      /* object type (0x40=audio) */
    add_u8  (h, 0x15);                      /* stream type (6b: 0x5=audio) + upstream (1b) + reserved (1b: const 1) */
    add_u24b(h, 0x000000);                  /* buffer size */
    add_u32b(h, 0);                         /* max bitrate (256000?)*/
    add_u32b(h, 0);                         /* average bitrate (256000?) */

    add_u8  (h, 0x05);                      /* DecSpecificInfoTag */
    add_u32b(h, 0x80808002);                /* size 0x02 */
    add_u16b(h, config);                    /* actual decoder info */

    add_u8  (h, 0x06);                      /* SLConfigDescrTag  */
    add_u32b(h, 0x80808001);                /* size 0x01 */
    add_u8  (h, 0x02);                      /* predefined (2=default) */
}

static void add_mp4a(m4a_header_t* h) {
    add_atom(h, "mp4a", 0x57);
    add_u32b(h, 0);                         /* ? */
    add_u32b(h, 1);                         /* Data reference index */
    add_u32b(h, 0);                         /* Reserved */
    add_u32b(h, 0);                         /* Reserved 2 */
    add_u16b(h, h->ktac->channels);         /* Channel count */
    add_u16b(h, 16);                        /* Sample size */
    add_u32b(h, 0);                         /* Pre-defined */
    add_u16b(h, h->ktac->sample_rate);      /* Sample rate */
    add_u16b(h, 0);                         /* ? */
    add_esds(h); /* elementary stream descriptor */
}

static void add_stsd(m4a_header_t* h) {
    add_atom(h, "stsd", 0x67);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */
    add_u32b(h, 1);                         /* Number of entries */
    add_mp4a(h);
}

static void add_stbl(m4a_header_t* h) {
    m4a_state_t s;

    save_atom(h, &s);
    add_atom(h, "stbl", 0x00);
    add_stsd(h); /* Sample description */
    add_stts(h); /* Time-to-sample  */
    add_stsc(h); /* Sample-to-chunk */
    add_stsz(h); /* Sample size */
    add_stco(h); /* Chunk offset */
    load_atom(h, &s);
}

static void add_dinf(m4a_header_t* h) {
    add_atom(h, "dinf", 0x24);
    add_atom(h, "dref", 0x1c);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */
    add_u32b(h, 1);                         /* Number of entries */
    add_atom(h, "url ", 0x0c);
    add_u32b(h, 1);                         /* Version (1 byte) + Flags (3 byte) */
}

static void add_smhd(m4a_header_t* h) {
    add_atom(h, "smhd", 0x10);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */
    add_u16b(h, 0);                         /* Balance */
    add_u16b(h, 0);                         /* Reserved */
}

static void add_minf(m4a_header_t* h) {
    m4a_state_t s;

    save_atom(h, &s);
    add_atom(h, "minf", 0x00);
    add_smhd(h);
    add_dinf(h);
    add_stbl(h);
    load_atom(h, &s);
}

static void add_hdlr(m4a_header_t* h) {
    add_atom(h, "hdlr", 0x22);
    add_u32b(h, 0);                         /* version (1 byte) + flags (3 byte) */
    add_u32b(h, 0);                         /* Component type */
    add_name(h, "soun");                    /* Component subtype */
    add_u32b(h, 0);                         /* Component manufacturer */
    add_u32b(h, 0);                         /* Component flags */
    add_u32b(h, 0);                         /* Component flags mask */
    add_u16b(h, 0);                         /* Component name */
}

static void add_mdhd(m4a_header_t* h) {
    add_atom(h, "mdhd", 0x20);
    add_u32b(h, 0);                         /* Version (1 byte) + Flags (3 byte) */
    add_u32b(h, 0);                         /* Creation time */
    add_u32b(h, 0);                         /* Modification time */
    add_u32b(h, h->ktac->sample_rate);      /* Time scale */
    add_u32b(h, h->ktac->num_samples);      /* Duration */
    add_u16b(h, 0);                         /* Language (0xC455=eng?) */
    add_u16b(h, 0);                         /* Quality */
}

static void add_mdia(m4a_header_t* h) {
    m4a_state_t s;

    save_atom(h, &s);
    add_atom(h, "mdia", 0x00);
    add_mdhd(h);
    add_hdlr(h);
    add_minf(h);
    load_atom(h, &s);
}

static void add_tkhd(m4a_header_t* h) {
    add_atom(h, "tkhd", 0x5C);
    add_u32b(h, 0x00000001);            /* Version (1 byte) + Flags (3 byte), 1=track enabled */
    add_u32b(h, 0);                     /* Creation time */
    add_u32b(h, 0);                     /* Modification time */
    add_u32b(h, 1);                     /* Track ID */
    add_u32b(h, 0);                     /* Reserved 1 */
    add_u32b(h, h->ktac->num_samples);  /* Duration */
    add_u32b(h, 0);                     /* Reserved 1 */
    add_u32b(h, 0);                     /* Reserved 2 */
    add_u16b(h, 0);                     /* Layer */
    add_u16b(h, 0);                     /* Alternate group (1?) */
    add_u16b(h, 0x0100);                /* Volume */
    add_u16b(h, 0);                     /* Reserved */
    add_u32b(h, 0x00010000);            /* matrix_A */
    add_u32b(h, 0);                     /* matrix_B */
    add_u32b(h, 0);                     /* matrix_U */
    add_u32b(h, 0);                     /* matrix_C */
    add_u32b(h, 0x00010000);            /* matrix_D */
    add_u32b(h, 0);                     /* matrix_V */
    add_u32b(h, 0);                     /* matrix_X */
    add_u32b(h, 0);                     /* matrix_Y */
    add_u32b(h, 0x40000000);            /* matrix_W */
    add_u32b(h, 0);                     /* Width */
    add_u32b(h, 0);                     /* Height */
}

static void add_trak(m4a_header_t* h) {
    m4a_state_t s;

    save_atom(h, &s);
    add_atom(h, "trak", 0x00);
    add_tkhd(h);
    add_mdia(h);
    load_atom(h, &s);
}

static void add_mvhd(m4a_header_t* h) {
    add_atom(h, "mvhd", 0x6c);
    add_u32b(h, 0);                     /* Version (1 byte) + Flags (3 byte) */
    add_u32b(h, 0);                     /* Creation time */
    add_u32b(h, 0);                     /* Modification time */
    add_u32b(h, h->ktac->sample_rate);  /* Time scale */
    add_u32b(h, h->ktac->num_samples);  /* Duration */
    add_u32b(h, 0x00010000);            /* Preferred rate */
    add_u16b(h, 0x0100);                /* Preferred volume */
    add_u32b(h, 0);                     /* Reserved 1 */
    add_u32b(h, 0);                     /* Reserved 2 */
    add_u16b(h, 0);                     /* Reserved 3 */
    add_u32b(h, 0x00010000);            /* matrix_A */
    add_u32b(h, 0);                     /* matrix_B */
    add_u32b(h, 0);                     /* matrix_U */
    add_u32b(h, 0);                     /* matrix_C */
    add_u32b(h, 0x00010000);            /* matrix_D */
    add_u32b(h, 0);                     /* matrix_V */
    add_u32b(h, 0);                     /* matrix_X */
    add_u32b(h, 0);                     /* matrix_Y */
    add_u32b(h, 0x40000000);            /* matrix_W */
    add_u32b(h, 0);                     /* Preview time */
    add_u32b(h, 0);                     /* Preview duration */
    add_u32b(h, 0);                     /* Poster time */
    add_u32b(h, 0);                     /* Selection time */
    add_u32b(h, 0);                     /* Selection duration */
    add_u32b(h, 0);                     /* Current time */
    add_u32b(h, 2);                     /* Next track ID */
}

static void add_moov(m4a_header_t* h) {
    m4a_state_t s;

    save_atom(h, &s);
    add_atom(h, "moov", 0x00);
    add_mvhd(h);
    add_trak(h);
  //add_udta(h);
    load_atom(h, &s);
}

/* *** */

static int make_m4a_header(uint8_t* buf, int buf_len, ktac_header_t* ktac, STREAMFILE* sf) {
    m4a_header_t h = {0};

    if (buf_len < 0x300 + ktac->table_entries * 0x4) /* approx */
        goto fail;

    h.sf = sf;
    h.ktac = ktac;
    h.out = buf;

    add_ftyp(&h);
    add_free(&h);
    add_moov(&h);
    add_mdat(&h);


    /* define absolute chunk offset after all calcs */
    put_u32be(h.chunks.out, h.bytes);

    return h.bytes;
fail:
    return 0;
}

#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"
#include "../util/endianness.h"


typedef enum { PCM, UBI, PSX, DSP, XIMA, ATRAC3, XMA2, MP3, SILENCE } ubi_hx_codec;

typedef struct {
    int big_endian;
    int total_subsongs;
    int is_riff;

    int codec_id;
    ubi_hx_codec codec;         /* unified codec */
    int header_index;           /* entry number within section2 */
    uint32_t header_offset;     /* entry offset within internal .HXx */
    uint32_t header_size;       /* entry offset within internal .HXx */
    char class_name[255];
    size_t class_size;
    size_t stream_mode;

    uint32_t stream_offset;     /* data offset within external stream */
    uint32_t stream_size;       /* data size within external stream */
    uint32_t cuuid1;            /* usually "Res" id1: class (1=Event, 3=Wave), id2: group id+sound id, */
    uint32_t cuuid2;            /* others have some complex id (not hash), id1: parent id?, id2: file id? */

    int loop_flag;
    int channels;
    int sample_rate;
    int num_samples;

    int is_external;
    char resource_name[0x28];   /* filename to the external stream */
    char internal_name[255];    /* WavRes's assigned name */
    char readable_name[255];    /* final subsong name */

} ubi_hx_header;


static int parse_hx(ubi_hx_header* hx, STREAMFILE* sf, int target_subsong);
static VGMSTREAM* init_vgmstream_ubi_hx_header(ubi_hx_header* hx, STREAMFILE* sf);

/* .HXx - banks from Ubisoft's HXAudio engine games [Rayman Arena, Rayman 3, XIII] */
VGMSTREAM* init_vgmstream_ubi_hx(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    ubi_hx_header hx = {0};
    int target_subsong = sf->stream_index;


    /* checks */
    {
        uint32_t name_size = read_u32be(0x04, sf); /* BE/LE, should always be < 0xFF */
        if (name_size == 0 || (name_size & 0x00FFFF00) != 0)
            goto fail;
    }

    /* .hxd: Rayman M/Arena (all), PK: Out of Shadows (all)
     * .hxc: Rayman 3 (PC), XIII (PC)
     * .hx2: Rayman 3 (PS2), XIII (PS2)
     * .hxg: Rayman 3 (GC), XIII (GC)
     * .hxx: Rayman 3 (Xbox), Rayman 3 HD (X360)
     * .hx3: Rayman 3 HD (PS3) */
    if (!check_extensions(sf, "hxd,hxc,hx2,hxg,hxx,hx3"))
        goto fail;

    /* .HXx is a slightly less bizarre bank with various resource classes (events, streams, etc, not unlike other Ubi's engines)
     * then an index to those types. Some games leave a companion .bnh with text info, probably leftover from their tools.
     * Game seems to play files by calling linked ids: EventResData (play/stop/etc) > Random/Program/Wav ResData (1..N refs) > FileIdObj */

    /* HX HEADER */
    hx.big_endian = guess_endian32(0x00, sf);
    if (!parse_hx(&hx, sf, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_hx_header(&hx, sf);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static void build_readable_name(char* buf, size_t buf_size, ubi_hx_header* hx) {
    const char *grp_name;

    if (hx->is_external)
        grp_name = hx->resource_name;
    else
        grp_name = "internal";

    if (hx->internal_name[0])
        snprintf(buf,buf_size, "%s/%i/%08x-%08x/%s/%s", "hx", hx->header_index, hx->cuuid1,hx->cuuid2, grp_name, hx->internal_name);
    else
        snprintf(buf,buf_size, "%s/%i/%08x-%08x/%s", "hx", hx->header_index, hx->cuuid1,hx->cuuid2, grp_name);
}

#define TXT_LINE_MAX 0x1000

/* get name */
static int parse_name_bnh(ubi_hx_header* hx, STREAMFILE* sf, uint32_t cuuid1, uint32_t cuuid2) {
    STREAMFILE* sf_t;
    off_t txt_offset = 0;
    char line[TXT_LINE_MAX];
    char cuuid[40];

    sf_t = open_streamfile_by_ext(sf,"bnh");
    if (sf_t == NULL) goto fail;

    snprintf(cuuid,sizeof(cuuid), "cuuid( 0x%08x, 0x%08x )", cuuid1, cuuid2);

    /* each .bnh line has a cuuid, a bunch of repeated fields and name (sometimes name is filename or "bad name") */
    while (txt_offset < get_streamfile_size(sf)) {
        int line_ok, bytes_read;

        bytes_read = read_line(line, sizeof(line), txt_offset, sf_t, &line_ok);
        if (!line_ok) break;
        txt_offset += bytes_read;

        if (strncmp(line,cuuid,31) != 0)
            continue;
        if (bytes_read <= 79)
            goto fail;

        /* cuuid found, copy name (lines are fixed and always starts from the same position) */
        strcpy(hx->internal_name, &line[79]);

        close_streamfile(sf_t);
        return 1;
    }

fail:
    close_streamfile(sf_t);
    return 0;
}


/* get referenced name from WavRes, using the index again (abridged) */
static int parse_name(ubi_hx_header* hx, STREAMFILE* sf) {
    read_u32_t read_u32 = hx->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = hx->big_endian ? read_s32be : read_s32le;
    uint32_t index_type, index_offset, offset;
    int i, index_entries;
    char class_name[255];


    index_offset = read_u32(0x00, sf);
    index_type = read_u32(index_offset + 0x04, sf);
    index_entries = read_s32(index_offset + 0x08, sf);

    /* doesn't seem to have names (no way to link) */
    if (index_type == 0x01)
        return 1;

    offset = index_offset + 0x0c;
    for (i = 0; i < index_entries; i++) {
        off_t header_offset;
        size_t class_size;
        int j, link_count, language_count, is_found = 0;
        uint32_t cuuid1, cuuid2;


        class_size = read_u32(offset + 0x00, sf);
        if (class_size > sizeof(class_name)+1) goto fail;
        read_string(class_name,class_size+1, offset + 0x04, sf); /* not null-terminated */
        offset += 0x04 + class_size;

        cuuid1 = read_u32(offset + 0x00, sf);
        cuuid2 = read_u32(offset + 0x04, sf);

        header_offset = read_u32(offset + 0x08, sf);
        offset += 0x10;

        //unknown_count = read_s32(offset + 0x00, sf);
        offset += 0x04;

        if (index_type == 0x01) {
            goto fail;
        }
        else {
            link_count = read_s32(offset + 0x00, sf);
            offset += 0x04;
            for (j = 0; j < link_count; j++) {
                uint32_t link_id1 = read_u32(offset + 0x00, sf);
                uint32_t link_id2 = read_u32(offset + 0x04, sf);

                if (link_id1 == hx->cuuid1 && link_id2 == hx->cuuid2) {
                    is_found = 1;
                }
                offset += 0x08;
            }

            language_count = read_s32(offset + 0x00, sf);
            offset += 0x04;
            for (j = 0; j < language_count; j++) {
                uint32_t link_id1 = read_u32(offset + 0x08, sf);
                uint32_t link_id2 = read_u32(offset + 0x0c, sf);

                if (link_id1 == hx->cuuid1 && link_id2 == hx->cuuid2) {
                    is_found = 1;
                }

                offset += 0x10;
            }
        }

        /* identify all possible names so unknown platforms fail */
        if (is_found && (
                strcmp(class_name, "CPCWavResData") == 0 ||
                strcmp(class_name, "CPS2WavResData") == 0 ||
                strcmp(class_name, "CGCWavResData") == 0 ||
                strcmp(class_name, "CXBoxWavResData") == 0 ||
                strcmp(class_name, "CPS3WavResData") == 0)) {
            size_t resclass_size, internal_size;
            off_t wavres_offset = header_offset;

            /* parse WavRes header */
            resclass_size = read_u32(wavres_offset, sf);
            wavres_offset += 0x04 + resclass_size + 0x08 + 0x04; /* skip class + cuiid + flags */

            internal_size = read_u32(wavres_offset + 0x00, sf);
            /* Xbox has some kind of big size and "flags" has a value of 2, instead of 3/4 like other platforms */
            if (strcmp(class_name, "CXBoxWavResData") == 0 && internal_size > 0x100)
                return 1;
            if (internal_size > sizeof(hx->internal_name)+1)
                goto fail;

            /* usually 0 in consoles */
            if (internal_size != 0) {
                read_string(hx->internal_name,internal_size+1, wavres_offset + 0x04, sf);
                return 1;
            }
            else {
                parse_name_bnh(hx, sf, cuuid1, cuuid2);
                return 1; /* ignore error */
            }
        }
    }

fail:
    vgm_logi("UBI HX: error parsing name at %x (report)\n", index_offset);
    return 0;
}


/* parse a single known header resource at offset */
static int parse_header(ubi_hx_header* hx, STREAMFILE* sf, uint32_t offset, uint32_t size, int index)  {
    read_u32_t read_u32 = hx->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = hx->big_endian ? read_s32be : read_s32le;
    read_u16_t read_u16 = hx->big_endian ? read_u16be : read_u16le;
    uint32_t riff_offset, riff_size, stream_adjust = 0, resource_size, chunk_size;
    off_t chunk_offset;
    int cue_flag = 0;

    //todo cleanup/unify common readings

    //;VGM_LOG("ubi hx: header o=%x, s=%x\n\n", offset, size);

    hx->header_index    = index;
    hx->header_offset   = offset;
    hx->header_size     = size;

    hx->class_size = read_u32(offset + 0x00, sf);
    if (hx->class_size > sizeof(hx->class_name)+1) goto fail;
    read_string(hx->class_name,hx->class_size+1, offset + 0x04, sf);
    offset += 0x04 + hx->class_size;

    hx->cuuid1  = read_u32(offset + 0x00, sf);
    hx->cuuid2  = read_u32(offset + 0x04, sf);
    offset += 0x08;

    if (strcmp(hx->class_name, "CPCWaveFileIdObj") == 0 ||
        strcmp(hx->class_name, "CPS2WaveFileIdObj") == 0 ||
        strcmp(hx->class_name, "CGCWaveFileIdObj") == 0 ||
        strcmp(hx->class_name, "CXBoxWaveFileIdObj") == 0) {
        uint32_t flag_type = read_u32(offset + 0x00, sf);

        if (flag_type == 0x01 || flag_type == 0x02) { /* Rayman Arena */
            uint32_t unk_value = read_u32(offset + 0x04, sf); /* float? */
            if (unk_value != 0x00 &&            /* common */
                unk_value != 0xbe570a3d &&      /* Largo Winch: Empire Under Threat (PC)-most */
                unk_value != 0xbf8e147b) {      /* Largo Winch: Empire Under Threat (PC)-few */
                VGM_LOG("ubi hx: unknown flag\n");
                goto fail;
            }

            hx->stream_mode = read_u32(offset + 0x08, sf); /* flag: 0=internal, 1=external */
            /* 0x0c: flag: 0=static, 1=stream */
            offset += 0x10;
        }
        else if (flag_type == 0x03) { /* others */
            /* 0x04: some kind of parent id shared by multiple Waves, or 0 */
            offset += 0x08;

            if (strcmp(hx->class_name, "CGCWaveFileIdObj") == 0) {
                if (read_u32(offset + 0x00, sf) != read_u32(offset + 0x04, sf))
                    goto fail; /* meaning? */
                hx->stream_mode = read_u32(offset + 0x04, sf);
                offset += 0x08;
            }
            else {
                hx->stream_mode = read_u8(offset, sf);
                offset += 0x01;
            }
        }
        else {
            VGM_LOG("ubi hx: unknown flag-type\n");
            goto fail;
        }

        /* get bizarro adjust (found in XIII external files) */
        if (hx->stream_mode == 0x0a) {
            stream_adjust = read_u32(offset, sf); /* what */
            offset += 0x04;
        }

        //todo probably a flag: &1=external, &2=stream, &8=has adjust (XIII), &4=??? (XIII PS2, small, mono)
        switch(hx->stream_mode) {
            case 0x00: /* memory (internal file) */
            case 0x02: /* same (no diffs in size/channels/etc?) [Rayman 3 demo (PC)] */
                riff_offset = offset;
                riff_size   = read_u32(riff_offset + 0x04, sf) + 0x08;
                break;

            case 0x01: /* static (smaller external file) */
            case 0x03: /* stream (bigger external file) */
            case 0x07: /* static? */
            case 0x0a: /* static? */
                resource_size = read_u32(offset + 0x00, sf);
                if (resource_size > sizeof(hx->resource_name)+1) goto fail;
                read_string(hx->resource_name,resource_size+1, offset + 0x04, sf);

                riff_offset = offset + 0x04 + resource_size;
                riff_size   = read_u32(riff_offset + 0x04, sf) + 0x08;

                hx->is_external = 1;
                break;

            default:
                VGM_LOG("ubi hx: unknown wave mode %x\n", hx->stream_mode);
                goto fail;
        }

        /* parse pseudo-RIFF "fmt" */
        if (read_u32(riff_offset, sf) != 0x46464952) { /* "RIFF" in machine endianness */
            VGM_LOG("ubi hx: unknown RIFF\n");
            goto fail;
        }

        hx->is_riff = 1;

        hx->codec_id = read_u16(riff_offset + 0x14 , sf);
        switch(hx->codec_id) {
            case 0x01: hx->codec = PCM; break;
            case 0x02: hx->codec = UBI; break;
            case 0x03: hx->codec = PSX; break;
            case 0x04: hx->codec = DSP; break;
            case 0x05: hx->codec = XIMA; break;
            case 0x55: hx->codec = MP3; break;  /* Largo Winch: Empire Under Threat (PC) */
            default: 
                VGM_LOG("ubi hx: unknown codec %x\n", hx->codec_id);
                goto fail;
        }
        hx->channels    = read_u16(riff_offset + 0x16, sf);
        hx->sample_rate = read_u32(riff_offset + 0x18, sf);

        /* find "datx" (external) or "data" (internal) also in machine endianness */
        if (hx->is_external) {

            if (find_chunk_riff_ve(sf, 0x78746164,riff_offset + 0x0c,riff_size - 0x0c, &chunk_offset,NULL, hx->big_endian)) {
                hx->stream_size     = read_u32(chunk_offset + 0x00, sf);
                hx->stream_offset   = read_u32(chunk_offset + 0x04, sf) + stream_adjust;
            }
            else if ((flag_type == 0x01 || flag_type == 0x02) && /* Rayman M (not Arena) uses "data" instead */
                    find_chunk_riff_ve(sf, 0x61746164,riff_offset + 0x0c,riff_size - 0x0c, &chunk_offset,&chunk_size, hx->big_endian)) {
                hx->stream_size     = chunk_size;
                hx->stream_offset   = read_u32(chunk_offset + 0x00, sf) + stream_adjust;
            }
            else {
                VGM_LOG("ubi hx: unknown chunk\n");
                goto fail;
            }
        }
        else {
            if (!find_chunk_riff_ve(sf, 0x61746164,riff_offset + 0x0c,riff_size - 0x0c, &chunk_offset,&chunk_size, hx->big_endian)) {
                VGM_LOG("ubi hx: unknown chunk RIFF\n");
                goto fail;
            }
            hx->stream_offset   = chunk_offset;

            if (chunk_size >  riff_size - (chunk_offset - riff_offset) || !chunk_size)
                chunk_size = riff_size - (chunk_offset - riff_offset); /* just in case */
            hx->stream_size     = chunk_size;
        }

        /* can contain other RIFF stuff like "cue ", "labl" and "ump3"
         * XIII music uses cue/labl to play/loop dynamic sections */
    }
    else if (strcmp(hx->class_name, "CXBoxStaticHWWaveFileIdObj") == 0 ||
             strcmp(hx->class_name, "CXBoxStreamHWWaveFileIdObj") == 0 ||
             strcmp(hx->class_name, "CPS3StaticAC3WaveFileIdObj") == 0 ||
             strcmp(hx->class_name, "CPS3StreamAC3WaveFileIdObj") == 0) {

        hx->stream_offset   = read_u32(offset + 0x00, sf);
        hx->stream_size     = read_u32(offset + 0x04, sf);
        offset += 0x08;

        //todo some dummy files have 0 size

        if (read_u32(offset + 0x00, sf) != 0x01) {
            VGM_LOG("ubi hx: unknown flag non 0x01\n");
            goto fail;
        }

        /* 0x04: some kind of parent id shared by multiple Waves, or 0 */
        offset += 0x08;

        hx->stream_mode = read_u8(offset, sf);
        offset += 0x01;

        if ((strcmp(hx->class_name, "CXBoxStaticHWWaveFileIdObj") == 0 ||
             strcmp(hx->class_name, "CXBoxStreamHWWaveFileIdObj") == 0) && !hx->big_endian) {
            /* micro header: some mix of channels + block size + sample rate + flags, unsure of which bits */
            
            /* 0x00: ? */
            uint8_t flags    = read_u8(offset + 0x01, sf);
            switch(flags) { 
                case 0x05: // b00000101 /* XIII (Xbox)-beta 2002-12 */
                    hx->channels = 1;
                    hx->codec = PCM;
                    break;
                case 0x09: // b00001001 /* XIII (Xbox)-beta 2002-12 */
                    hx->channels = 2;
                    hx->codec = PCM;
                    break;
                case 0x48: // b01001000
                    hx->channels = 1;
                    hx->codec = XIMA;
                    break;
                case 0x90: // b10010000
                    hx->channels = 2;
                    hx->codec = XIMA;
                    break;
                default:
                    VGM_LOG("ubi hx: channel flags %x\n", flags);
                    goto fail;
            }
            hx->sample_rate = (read_u16(offset + 0x02, sf) & 0x7FFFu) << 1u;  /* ??? */
            cue_flag        = read_u8(offset + 0x03, sf) & (1 << 7);
            offset += 0x04;
        }
        else if ((strcmp(hx->class_name, "CXBoxStaticHWWaveFileIdObj") == 0 ||
                  strcmp(hx->class_name, "CXBoxStreamHWWaveFileIdObj") == 0) && hx->big_endian) {
            /* fake fmt chunk */
            hx->codec       = XMA2;
            hx->channels    = read_u16(offset + 0x02, sf);
            hx->sample_rate = read_u32(offset + 0x04, sf);
            hx->num_samples = read_s32(offset + 0x18, sf) / 0x02 / hx->channels;
            cue_flag        = read_u32(offset + 0x34, sf);
            offset += 0x38;
        }
        else {
            /* MSFC header */
            hx->codec       = ATRAC3;
            hx->codec_id    = read_u32(offset + 0x04, sf);
            hx->channels    = read_u32(offset + 0x08, sf);
            hx->sample_rate = read_u32(offset + 0x10, sf);
            cue_flag        = read_u32(offset + 0x40, sf);
            offset += 0x44;
        }

        /* skip cue table that sometimes exists in streams */
        if (cue_flag) {
            int j;

            size_t cue_count = read_s32(offset, sf);
            offset += 0x04;
            for (j = 0; j < cue_count; j++) {
                /* 0x00: id? */
                size_t description_size = read_u32(offset + 0x04, sf); /* for next string */
                offset += 0x08 + description_size;
            }
        }


        switch(hx->stream_mode) {
            case 0x00: /* static (smaller internal file) [XIII (Xbox)] */
            case 0x02: /* static (smaller internal file) [XIII-beta (Xbox)] */
                hx->stream_offset += offset;
                break;

            case 0x01: /* static (smaller external file) */
            case 0x03: /* stream (bigger external file) */
            case 0x07: /* stream? */
                resource_size = read_u32(offset + 0x00, sf);
                if (resource_size > sizeof(hx->resource_name)+1) goto fail;
                read_string(hx->resource_name,resource_size+1, offset + 0x04, sf);

                hx->is_external = 1;
                break;

            default:
                VGM_LOG("ubi hx: unknown stream mode %x\n", hx->stream_mode);
                goto fail;
        }
    }
    else {
        VGM_LOG("ubi hx: unknown type\n");
        goto fail;
    }

    return 1;
fail:
    vgm_logi("UBI HX: error parsing header at %x (report)\n", hx->header_offset);
    return 0;
}


/* parse a bank index and its possible audio headers (some info from Droolie's .bms) */
static int parse_hx(ubi_hx_header* hx, STREAMFILE* sf, int target_subsong) {
    read_u32_t read_u32 = hx->big_endian ? read_u32be : read_u32le;
    read_s32_t read_s32 = hx->big_endian ? read_s32be : read_s32le;
    uint32_t index_offset, offset;
    int i, index_entries;
    char class_name[255];
    uint32_t index_type;


    index_offset = read_u32(0x00, sf);
    if (read_u32(index_offset + 0x00, sf) != get_id32be("XDNI")) { /* (INDX in given endianness) */
        VGM_LOG("ubi hx: unknown index\n");
        goto fail;
    }

    /* usually 0x02, rarely 0x01 [Rayman M demo (PS2)] */
    index_type = read_u32(index_offset + 0x04, sf);
    if (index_type != 0x01 && index_type != 0x02) {
        VGM_LOG("ubi hx: unknown index type\n");
        goto fail;
    }

    if (target_subsong == 0) target_subsong = 1;

    index_entries = read_s32(index_offset + 0x08, sf);
    offset = index_offset + 0x0c;
    for (i = 0; i < index_entries; i++) {
        uint32_t header_offset, class_size, header_size;
        int j, unknown_count, link_count, language_count;

        //;VGM_LOG("ubi hx: index %i at %x\n", i, offset);

        /* parse index entries: offset to actual header plus some extra info also in the header */

        class_size = read_u32(offset + 0x00, sf);
        if (class_size > sizeof(class_name)+1) goto fail;

        read_string(class_name,class_size+1, offset + 0x04, sf); /* not null-terminated */
        offset += 0x04 + class_size;

        /* 0x00: id1+2 */
        header_offset = read_u32(offset + 0x08, sf);
        header_size   = read_u32(offset + 0x0c, sf);
        offset += 0x10;

        /* not seen */
        unknown_count = read_s32(offset + 0x00, sf);
        if (unknown_count != 0) {
            VGM_LOG("ubi hx: found unknown near %x\n", offset);
            goto fail;
        }
        offset += 0x04;

        if (index_type == 0x01) {
            link_count = 0;
            language_count = 0;
        }
        else {
            /* ids that this object directly points to (ex. Event > Random) */
            link_count = read_s32(offset + 0x00, sf);
            offset += 0x04 + 0x08 * link_count;

            /* localized id list of WavRes (can use this list instead of the prev one) */
            language_count = read_s32(offset + 0x00, sf);
            offset += 0x04;
            for (j = 0; j < language_count; j++) {
                /* 0x00: lang code, in reverse endianness: "en  ", "fr  ", etc */
                /* 0x04: possibly count of ids for this lang */
                /* 0x08: id1+2 */

                if (read_u32(offset + 0x04, sf) != 1) {
                    VGM_LOG("ubi hx: wrong lang count near %x\n", offset);
                    goto fail; /* WavRes doesn't have this field */
                }
                offset += 0x10;
            }
        }

        //todo figure out CProgramResData sequences
        // Format is pretty complex list of values and some offsets in between, then field names
        // then more values and finally a list of linked IDs Links are the same as in the index,
        // but doesn't seem to be a straight sequence list. Seems it can be used for other config too.

        /* identify all possible names so unknown platforms fail */
        if (strcmp(class_name, "CEventResData") == 0 ||      /* play/stop/etc event */
            strcmp(class_name, "CProgramResData") == 0 ||    /* some kind of map/object-like config to make sequences in some cases? */
            strcmp(class_name, "CActorResData") == 0 ||      /* same? */
            strcmp(class_name, "CRandomResData") == 0 ||     /* chooses random WavRes from a list */
            strcmp(class_name, "CTreeBank") == 0 ||          /* points to TreeRes? */
            strcmp(class_name, "CTreeRes") == 0 ||           /* points to TreeBank? */
            strcmp(class_name, "CSwitchResData") == 0 ||     /* big list of WavRes */
            strcmp(class_name, "CPCWavResData") == 0 ||      /* points to WaveFileIdObj */
            strcmp(class_name, "CPS2WavResData") == 0 ||
            strcmp(class_name, "CGCWavResData") == 0 ||
            strcmp(class_name, "CXBoxWavResData") == 0 ||
            strcmp(class_name, "CPS3WavResData") == 0) {
            continue;
        }
        else if (strcmp(class_name, "CPCWaveFileIdObj") == 0 ||
                 strcmp(class_name, "CPS2WaveFileIdObj") == 0 ||
                 strcmp(class_name, "CGCWaveFileIdObj") == 0 ||
                 strcmp(class_name, "CXBoxWaveFileIdObj") == 0 ||
                 strcmp(class_name, "CXBoxStaticHWWaveFileIdObj") == 0 ||
                 strcmp(class_name, "CXBoxStreamHWWaveFileIdObj") == 0 ||
                 strcmp(class_name, "CPS3StaticAC3WaveFileIdObj") == 0 ||
                 strcmp(class_name, "CPS3StreamAC3WaveFileIdObj") == 0) {
            ;
        }
        else {
            vgm_logi("UBI HX: unknown type: %s (report)\n", class_name);
            goto fail;
        }

        /* should only exist on non-wave objects (like CProgramResData) */
        if (link_count != 0) {
            vgm_logi("UBI HX: found links in wav object (report)\n");
            goto fail;
        }

        hx->total_subsongs++;
        if (hx->total_subsongs != target_subsong)
            continue;

        if (!parse_header(hx, sf, header_offset, header_size, i))
            goto fail;
        if (!parse_name(hx, sf))
            goto fail;

        build_readable_name(hx->readable_name,sizeof(hx->readable_name), hx);
    }

    if (target_subsong < 0 || target_subsong > hx->total_subsongs || hx->total_subsongs < 1) goto fail;


    return 1;
fail:
    return 0;
}


static STREAMFILE* open_hx_streamfile(ubi_hx_header* hx, STREAMFILE* sf) {
    STREAMFILE* sb = NULL;


    if (!hx->is_external)
        return NULL;

    sb = open_streamfile_by_filename(sf, hx->resource_name);
    if (sb == NULL) {
        vgm_logi("UBI HX: external file '%s' not found (put together)\n", hx->resource_name);
        goto fail;
    }

    /* streams often have a "RIFF" with "fmt" and "data" but stream offset/size is already adjusted to skip them */

    return sb;
fail:
    return NULL;
}

static VGMSTREAM* init_vgmstream_ubi_hx_header(ubi_hx_header* hx, STREAMFILE* sf) {
    STREAMFILE* temp_sf = NULL;
    STREAMFILE* sb = NULL;
    VGMSTREAM* vgmstream = NULL;


    if (hx->is_external) {
        temp_sf = open_hx_streamfile(hx, sf);
        if (temp_sf == NULL) goto fail;
        sb = temp_sf;
    }
    else {
        sb = sf;
    }

    /* very rarely a game uses Ubi ADPCM, but data is empty and has missing header [Rayman 3 demo 3 (PC) fixe.hxc#84] */ 
    if (hx->is_riff && hx->codec == UBI) { //todo improve
        if (read_u32le(hx->stream_offset, sb) == 0x02) {
            hx->codec = SILENCE;
        }
    }

    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(hx->channels, hx->loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->meta_type = meta_UBI_HX;
    vgmstream->sample_rate = hx->sample_rate;
    vgmstream->num_streams = hx->total_subsongs;
    vgmstream->stream_size = hx->stream_size;

    switch(hx->codec) {
        case PCM:
            vgmstream->coding_type = hx->big_endian ? coding_PCM16BE : coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(hx->stream_size, hx->channels, 16);
            break;

        case UBI:
            vgmstream->codec_data = init_ubi_adpcm(sb, hx->stream_offset, hx->stream_size, vgmstream->channels);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_UBI_ADPCM;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ubi_adpcm_get_samples(vgmstream->codec_data);

            /* XIII has 6-bit stereo music, Rayman 3 4-bit music, both use 6-bit mono) */
            break;

        case PSX:
            vgmstream->coding_type = coding_PSX;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x10;

            vgmstream->num_samples = ps_bytes_to_samples(hx->stream_size, hx->channels);
            break;

        case DSP:
            vgmstream->coding_type = coding_NGC_DSP;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x08;

            /* dsp header at start offset */
            vgmstream->num_samples = read_s32be(hx->stream_offset + 0x00, sb);
            dsp_read_coefs_be(vgmstream, sb, hx->stream_offset + 0x1c, 0x60);
            dsp_read_hist_be (vgmstream, sb, hx->stream_offset + 0x40, 0x60);
            hx->stream_offset += 0x60 * hx->channels;
            hx->stream_size -= 0x60 * hx->channels;
            break;

        case XIMA:
            vgmstream->coding_type = coding_XBOX_IMA;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = xbox_ima_bytes_to_samples(hx->stream_size, hx->channels);
            break;

#ifdef VGM_USE_FFMPEG
        case XMA2: {
            int block_size = 0x800;

            vgmstream->codec_data = init_ffmpeg_xma2_raw(sb, hx->stream_offset, hx->stream_size, hx->num_samples, hx->channels, hx->sample_rate, block_size, 0);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = hx->num_samples;

            xma_fix_raw_samples_ch(vgmstream, sb, hx->stream_offset,hx->stream_size, hx->channels, 0,0);
            break;
        }

        case ATRAC3: {
            int block_align, encoder_delay;

            encoder_delay = 1024 + 69*2;
            switch(hx->codec_id) {
                case 4: block_align = 0x60 * vgmstream->channels; break;
                case 5: block_align = 0x98 * vgmstream->channels; break;
                case 6: block_align = 0xC0 * vgmstream->channels; break;
                default: goto fail;
            }

            vgmstream->num_samples = atrac3_bytes_to_samples(hx->stream_size, block_align) - encoder_delay;

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(sb, hx->stream_offset, hx->stream_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

#ifdef VGM_USE_MPEG
        case MP3: {
            mpeg_custom_config cfg = {0};

            cfg.skip_samples = 0; /* ? */

            vgmstream->codec_data = init_mpeg_custom(sb, hx->stream_offset, &vgmstream->coding_type, vgmstream->channels, MPEG_STANDARD, &cfg);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = mpeg_get_samples_clean(sb, hx->stream_offset, hx->stream_size, NULL, NULL, 1);

            break;
        }
#endif

        case SILENCE: /* special hack */
            vgmstream->coding_type = coding_SILENCE;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = ps_bytes_to_samples(hx->stream_size, hx->channels);
            break;
        default:
            goto fail;
    }

    strcpy(vgmstream->stream_name, hx->readable_name);

    if (!vgmstream_open_stream(vgmstream, sb, hx->stream_offset))
        goto fail;
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

#include "meta.h"
#include "../layout/layout.h"
#include "../coding/coding.h"


typedef enum { PCM, UBI, PSX, DSP, XIMA, ATRAC3, XMA2 } ubi_hx_codec;

typedef struct {
    int big_endian;
    int total_subsongs;

    int codec_id;
    ubi_hx_codec codec;         /* unified codec */
    int header_index;           /* entry number within section2 */
    off_t header_offset;        /* entry offset within internal .HXx */
    size_t header_size;         /* entry offset within internal .HXx */
    char class_name[255];
    size_t class_size;
    size_t stream_mode;

    off_t stream_offset;        /* data offset within external stream */
    size_t stream_size;         /* data size within external stream */
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


static int parse_hx(ubi_hx_header * hx, STREAMFILE *sf, int target_subsong);
static VGMSTREAM * init_vgmstream_ubi_hx_header(ubi_hx_header *hx, STREAMFILE *sf);

/* .HXx - banks from Ubisoft's HXAudio engine games [Rayman Arena, Rayman 3, XIII] */
VGMSTREAM * init_vgmstream_ubi_hx(STREAMFILE *streamFile) {
    VGMSTREAM* vgmstream = NULL;
    ubi_hx_header hx = {0};
    int target_subsong = streamFile->stream_index;


    /* checks */
    /* .hxd: Rayman M/Arena (all), PK: Out of Shadows (all)
     * .hxc: Rayman 3 (PC), XIII (PC)
     * .hx2: Rayman 3 (PS2), XIII (PS2)
     * .hxg: Rayman 3 (GC), XIII (GC)
     * .hxx: Rayman 3 (Xbox), Rayman 3 HD (X360)
     * .hx3: Rayman 3 HD (PS3) */
    if (!check_extensions(streamFile, "hxd,hxc,hx2,hxg,hxx,hx3"))
        goto fail;

    /* .HXx is a slightly less bizarre bank with various resource classes (events, streams, etc, not unlike other Ubi's engines)
     * then an index to those types. Some games leave a companion .bnh with text info, probably leftover from their tools.
     * Game seems to play files by calling linked ids: EventResData (play/stop/etc) > Random/Program/Wav ResData (1..N refs) > FileIdObj */

    /* HX CONFIG */
    hx.big_endian = guess_endianness32bit(0x00, streamFile);

    /* HX HEADER */
    if (!parse_hx(&hx, streamFile, target_subsong))
        goto fail;

    /* CREATE VGMSTREAM */
    vgmstream = init_vgmstream_ubi_hx_header(&hx, streamFile);
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


static void build_readable_name(char * buf, size_t buf_size, ubi_hx_header * hx) {
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
static int parse_name_bnh(ubi_hx_header * hx, STREAMFILE *sf, uint32_t cuuid1, uint32_t cuuid2) {
    STREAMFILE *sf_t;
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
static int parse_name(ubi_hx_header * hx, STREAMFILE *sf) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = hx->big_endian ? read_32bitBE : read_32bitLE;
    off_t index_offset, offset;
    int i, index_entries;
    char class_name[255];


    index_offset = read_32bit(0x00, sf);
    index_entries = read_32bit(index_offset + 0x08, sf);
    offset = index_offset + 0x0c;
    for (i = 0; i < index_entries; i++) {
        off_t header_offset;
        size_t class_size;
        int j, link_count, language_count, is_found = 0;
        uint32_t cuuid1, cuuid2;


        class_size = read_32bit(offset + 0x00, sf);
        if (class_size > sizeof(class_name)+1) goto fail;
        read_string(class_name,class_size+1, offset + 0x04, sf); /* not null-terminated */
        offset += 0x04 + class_size;

        cuuid1 = (uint32_t)read_32bit(offset + 0x00, sf);
        cuuid2 = (uint32_t)read_32bit(offset + 0x04, sf);

        header_offset = read_32bit(offset + 0x08, sf);
        offset += 0x10;

        //unknown_count = read_32bit(offset + 0x00, sf);
        offset += 0x04;

        link_count = read_32bit(offset + 0x00, sf);
        offset += 0x04;
        for (j = 0; j < link_count; j++) {
            uint32_t link_id1 = (uint32_t)read_32bit(offset + 0x00, sf);
            uint32_t link_id2 = (uint32_t)read_32bit(offset + 0x04, sf);

            if (link_id1 == hx->cuuid1 && link_id2 == hx->cuuid2) {
                is_found = 1;
            }
            offset += 0x08;
        }

        language_count = read_32bit(offset + 0x00, sf);
        offset += 0x04;
        for (j = 0; j < language_count; j++) {
            uint32_t link_id1 = (uint32_t)read_32bit(offset + 0x08, sf);
            uint32_t link_id2 = (uint32_t)read_32bit(offset + 0x0c, sf);

            if (link_id1 == hx->cuuid1 && link_id2 == hx->cuuid2) {
                is_found = 1;
            }

            offset += 0x10;
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
            resclass_size = read_32bit(wavres_offset, sf);
            wavres_offset += 0x04 + resclass_size + 0x08 + 0x04; /* skip class + cuiid + flags */

            internal_size = read_32bit(wavres_offset + 0x00, sf);
            if (internal_size > sizeof(hx->internal_name)+1) goto fail;

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
    return 0;
}


/* parse a single known header resource at offset */
static int parse_header(ubi_hx_header * hx, STREAMFILE *sf, off_t offset, size_t size, int index)  {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = hx->big_endian ? read_32bitBE : read_32bitLE;
    int16_t (*read_16bit)(off_t,STREAMFILE*) = hx->big_endian ? read_16bitBE : read_16bitLE;
    off_t riff_offset, riff_size, chunk_offset, stream_adjust = 0, resource_size;
    size_t chunk_size;
    int cue_flag = 0;

    //todo cleanup/unify common readings

    //;VGM_LOG("UBI HX: header o=%lx, s=%x\n\n", offset, size);

    hx->header_index    = index;
    hx->header_offset   = offset;
    hx->header_size     = size;

    hx->class_size = read_32bit(offset + 0x00, sf);
    if (hx->class_size > sizeof(hx->class_name)+1) goto fail;
    read_string(hx->class_name,hx->class_size+1, offset + 0x04, sf);
    offset += 0x04 + hx->class_size;

    hx->cuuid1  = (uint32_t)read_32bit(offset + 0x00, sf);
    hx->cuuid2  = (uint32_t)read_32bit(offset + 0x04, sf);
    offset += 0x08;

    if (strcmp(hx->class_name, "CPCWaveFileIdObj") == 0 ||
        strcmp(hx->class_name, "CPS2WaveFileIdObj") == 0 ||
        strcmp(hx->class_name, "CGCWaveFileIdObj") == 0) {
        uint32_t flag_type = read_32bit(offset + 0x00, sf);

        if (flag_type == 0x01 || flag_type == 0x02) { /* Rayman Arena */
            if (read_32bit(offset + 0x04, sf) != 0x00) goto fail;
            hx->stream_mode = read_32bit(offset + 0x08, sf); /* flag: 0=internal, 1=external */
            /* 0x0c: flag: 0=static, 1=stream */
            offset += 0x10;
        }
        else if (flag_type == 0x03) { /* others */
            /* 0x04: some kind of parent id shared by multiple Waves, or 0 */
            offset += 0x08;

            if (strcmp(hx->class_name, "CGCWaveFileIdObj") == 0) {
                if (read_32bit(offset + 0x00, sf) != read_32bit(offset + 0x04, sf)) goto fail; /* meaning? */
                hx->stream_mode = read_32bit(offset + 0x04, sf);
                offset += 0x08;
            }
            else {
                hx->stream_mode = read_8bit(offset, sf);
                offset += 0x01;
            }
        }
        else {
            VGM_LOG("UBI HX: unknown flag-type\n");
            goto fail;
        }

        /* get bizarro adjust (found in XIII external files) */
        if (hx->stream_mode == 0x0a) {
            stream_adjust = read_32bit(offset, sf); /* what */
            offset += 0x04;
        }

        //todo probably a flag: &1=external, &2=stream, &8=has adjust (XIII), &4=??? (XIII PS2, small, mono)
        switch(hx->stream_mode) {
            case 0x00: /* memory (internal file) */
                riff_offset = offset;
                riff_size   = read_32bit(riff_offset + 0x04, sf) + 0x08;
                break;

            case 0x01: /* static (smaller external file) */
            case 0x03: /* stream (bigger external file) */
            case 0x07: /* static? */
            case 0x0a: /* static? */
                resource_size = read_32bit(offset + 0x00, sf);
                if (resource_size > sizeof(hx->resource_name)+1) goto fail;
                read_string(hx->resource_name,resource_size+1, offset + 0x04, sf);

                riff_offset = offset + 0x04 + resource_size;
                riff_size   = read_32bit(riff_offset + 0x04, sf) + 0x08;

                hx->is_external = 1;
                break;

            default:
                goto fail;
        }


        /* parse pseudo-RIFF "fmt" */
        if (read_32bit(riff_offset, sf) != 0x46464952) /* "RIFF" in machine endianness */
            goto fail;

        hx->codec_id = read_16bit(riff_offset + 0x14 , sf);
        switch(hx->codec_id) {
            case 0x01: hx->codec = PCM;  break;
            case 0x02: hx->codec = UBI;  break;
            case 0x03: hx->codec = PSX;  break;
            case 0x04: hx->codec = DSP;  break;
            case 0x05: hx->codec = XIMA; break;
            default: goto fail;
        }
        hx->channels    = read_16bit(riff_offset + 0x16, sf);
        hx->sample_rate = read_32bit(riff_offset + 0x18, sf);

        /* find "datx" (external) or "data" (internal) also in machine endianness */
        if (hx->is_external) {
            if (find_chunk_riff_ve(sf, 0x78746164,riff_offset + 0x0c,riff_size - 0x0c, &chunk_offset,NULL, hx->big_endian)) {
                hx->stream_size     = read_32bit(chunk_offset + 0x00, sf);
                hx->stream_offset   = read_32bit(chunk_offset + 0x04, sf) + stream_adjust;
            }
            else if ((flag_type == 0x01 || flag_type == 0x02) && /* Rayman M (not Arena) uses "data" instead */
                    find_chunk_riff_ve(sf, 0x61746164,riff_offset + 0x0c,riff_size - 0x0c, &chunk_offset,&chunk_size, hx->big_endian)) {
                hx->stream_size     = chunk_size;
                hx->stream_offset   = read_32bit(chunk_offset + 0x00, sf) + stream_adjust;
            }
            else {
                goto fail;
            }
        }
        else {
            if (!find_chunk_riff_ve(sf, 0x61746164,riff_offset + 0x0c,riff_size - 0x0c, &chunk_offset,NULL, hx->big_endian))
                goto fail;
            hx->stream_offset   = chunk_offset;
            hx->stream_size     = riff_size - (chunk_offset - riff_offset);
        }

        /* can contain other RIFF stuff like "cue ", "labl" and "ump3"
         * XIII music uses cue/labl to play/loop dynamic sections */
    }
    else if (strcmp(hx->class_name, "CXBoxStaticHWWaveFileIdObj") == 0 ||
             strcmp(hx->class_name, "CXBoxStreamHWWaveFileIdObj") == 0 ||
             strcmp(hx->class_name, "CPS3StaticAC3WaveFileIdObj") == 0 ||
             strcmp(hx->class_name, "CPS3StreamAC3WaveFileIdObj") == 0) {

        hx->stream_offset   = read_32bit(offset + 0x00, sf);
        hx->stream_size     = read_32bit(offset + 0x04, sf);
        offset += 0x08;

        //todo some dummy files have 0 size

        if (read_32bit(offset + 0x00, sf) != 0x01) goto fail;
        /* 0x04: some kind of parent id shared by multiple Waves, or 0 */
        offset += 0x08;

        hx->stream_mode = read_8bit(offset, sf);
        offset += 0x01;

        if ((strcmp(hx->class_name, "CXBoxStaticHWWaveFileIdObj") == 0 ||
             strcmp(hx->class_name, "CXBoxStreamHWWaveFileIdObj") == 0) && !hx->big_endian) {
            /* micro header: some mix of channels + block size + sample rate + flags, unsure of which bits */
            hx->codec       = XIMA;
            hx->channels    = (uint8_t)read_8bit(offset + 0x01, sf);
            switch(hx->channels) { /* upper 2 bits? */
                case 0x48: hx->channels = 1; break;
                case 0x90: hx->channels = 2; break;
                default: goto fail;
            }
            hx->sample_rate = (uint16_t)(read_16bit(offset + 0x02, sf) & 0x7FFF) << 1;  /* ??? */
            cue_flag        = (uint8_t) read_8bit (offset + 0x03, sf) & (1<<7);
            offset += 0x04;
        }
        else if ((strcmp(hx->class_name, "CXBoxStaticHWWaveFileIdObj") == 0 ||
                  strcmp(hx->class_name, "CXBoxStreamHWWaveFileIdObj") == 0) && hx->big_endian) {
            /* fake fmt chunk */
            hx->codec       = XMA2;
            hx->channels    = (uint16_t)read_16bit(offset + 0x02, sf);
            hx->sample_rate = read_32bit(offset + 0x04, sf);
            hx->num_samples = read_32bit(offset + 0x18, sf) / 0x02 / hx->channels;
            cue_flag        = read_32bit(offset + 0x34, sf);
            offset += 0x38;
        }
        else {
            /* MSFC header */
            hx->codec       = ATRAC3;
            hx->codec_id    = read_32bit(offset + 0x04, sf);
            hx->channels    = read_32bit(offset + 0x08, sf);
            hx->sample_rate = read_32bit(offset + 0x10, sf);
            cue_flag        = read_32bit(offset + 0x40, sf);
            offset += 0x44;
        }

        /* skip cue table that sometimes exists in streams */
        if (cue_flag) {
            int j;

            size_t cue_count = read_32bit(offset, sf);
            offset += 0x04;
            for (j = 0; j < cue_count; j++) {
                /* 0x00: id? */
                size_t description_size = read_32bit(offset + 0x04, sf); /* for next string */
                offset += 0x08 + description_size;
            }
        }


        switch(hx->stream_mode) {
            case 0x01: /* static (smaller external file) */
            case 0x03: /* stream (bigger external file) */
            case 0x07: /* stream? */
                resource_size = read_32bit(offset + 0x00, sf);
                if (resource_size > sizeof(hx->resource_name)+1) goto fail;
                read_string(hx->resource_name,resource_size+1, offset + 0x04, sf);

                hx->is_external = 1;
                break;

            default:
                goto fail;
        }
    }
    else {
        goto fail;
    }

    return 1;
fail:
    VGM_LOG("UBI HX: error parsing header at %lx\n", hx->header_offset);
    return 0;
}


/* parse a bank index and its possible audio headers (some info from Droolie's .bms) */
static int parse_hx(ubi_hx_header * hx, STREAMFILE *sf, int target_subsong) {
    int32_t (*read_32bit)(off_t,STREAMFILE*) = hx->big_endian ? read_32bitBE : read_32bitLE;
    off_t index_offset, offset;
    int i, index_entries;
    char class_name[255];


    index_offset = read_32bit(0x00, sf);
    if (read_32bit(index_offset + 0x00, sf) != 0x58444E49) /* "XDNI" (INDX in given endianness) */
        goto fail;
    if (read_32bit(index_offset + 0x04, sf) != 0x02) /* type? */
        goto fail;

    if (target_subsong == 0) target_subsong = 1;

    index_entries = read_32bit(index_offset + 0x08, sf);
    offset = index_offset + 0x0c;
    for (i = 0; i < index_entries; i++) {
        off_t header_offset;
        size_t class_size, header_size;
        int j, unknown_count, link_count, language_count;

        //;VGM_LOG("UBI HX: index %i at %lx\n", i, offset);

        /* parse index entries: offset to actual header plus some extra info also in the header */

        class_size = read_32bit(offset + 0x00, sf);
        if (class_size > sizeof(class_name)+1) goto fail;

        read_string(class_name,class_size+1, offset + 0x04, sf); /* not null-terminated */
        offset += 0x04 + class_size;

        /* 0x00: id1+2 */
        header_offset = read_32bit(offset + 0x08, sf);
        header_size   = read_32bit(offset + 0x0c, sf);
        offset += 0x10;

        /* not seen */
        unknown_count = read_32bit(offset + 0x00, sf);
        if (unknown_count != 0) {
            VGM_LOG("UBI HX: found unknown near %lx\n", offset);
            goto fail;
        }
        offset += 0x04;

        /* ids that this object directly points to (ex. Event > Random) */
        link_count = read_32bit(offset + 0x00, sf);
        offset += 0x04 + 0x08 * link_count;

        /* localized id list of WavRes (can use this list instead of the prev one) */
        language_count = read_32bit(offset + 0x00, sf);
        offset += 0x04;
        for (j = 0; j < language_count; j++) {
            /* 0x00: lang code, in reverse endianness: "en  ", "fr  ", etc */
            /* 0x04: possibly count of ids for this lang */
            /* 0x08: id1+2 */

            if (read_32bit(offset + 0x04, sf) != 1) {
                VGM_LOG("UBI HX: wrong lang count near %lx\n", offset);
                goto fail; /* WavRes doesn't have this field */
            }
            offset += 0x10;
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
                 strcmp(class_name, "CXBoxStaticHWWaveFileIdObj") == 0 ||
                 strcmp(class_name, "CXBoxStreamHWWaveFileIdObj") == 0 ||
                 strcmp(class_name, "CPS3StaticAC3WaveFileIdObj") == 0 ||
                 strcmp(class_name, "CPS3StreamAC3WaveFileIdObj") == 0) {
            ;
        }
        else {
            VGM_LOG("UBI HX: unknown type: %s\n", class_name);
            goto fail;
        }

        if (link_count != 0) {
            VGM_LOG("UBI HX: found links in wav object\n");
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


static STREAMFILE * open_hx_streamfile(ubi_hx_header *hx, STREAMFILE *sf) {
    STREAMFILE *streamData = NULL;


    if (!hx->is_external)
        return NULL;

    streamData = open_streamfile_by_filename(sf, hx->resource_name);
    if (streamData == NULL) {
        VGM_LOG("UBI HX: external stream '%s' not found\n", hx->resource_name);
        goto fail;
    }

    /* streams often have a "RIFF" with "fmt" and "data" but stream offset/size is already adjusted to skip them */

    return streamData;
fail:
    return NULL;
}

static VGMSTREAM * init_vgmstream_ubi_hx_header(ubi_hx_header *hx, STREAMFILE *streamFile) {
    STREAMFILE *streamTemp = NULL;
    STREAMFILE *streamData = NULL;
    VGMSTREAM* vgmstream = NULL;


    if (hx->is_external) {
        streamTemp = open_hx_streamfile(hx, streamFile);
        if (streamTemp == NULL) goto fail;
        streamData = streamTemp;
    }
    else {
        streamData = streamFile;
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
            vgmstream->coding_type = coding_PCM16LE;
            vgmstream->layout_type = layout_interleave;
            vgmstream->interleave_block_size = 0x02;

            vgmstream->num_samples = pcm_bytes_to_samples(hx->stream_size, hx->channels, 16);
            break;

        case UBI:
            vgmstream->codec_data = init_ubi_adpcm(streamData, hx->stream_offset, vgmstream->channels);
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
            vgmstream->num_samples = read_32bitBE(hx->stream_offset + 0x00, streamData);
            dsp_read_coefs_be(vgmstream, streamData, hx->stream_offset + 0x1c, 0x60);
            dsp_read_hist_be (vgmstream, streamData, hx->stream_offset + 0x40, 0x60);
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
            int bytes, block_count, block_size;
            uint8_t buf[0x200];

            block_size = 0x800;
            block_count = hx->stream_size / block_size;

            bytes = ffmpeg_make_riff_xma2(buf,0x200, hx->num_samples, hx->stream_size, hx->channels, hx->sample_rate, block_count, block_size);
            vgmstream->codec_data = init_ffmpeg_header_offset(streamData, buf,bytes, hx->stream_offset,hx->stream_size);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;

            vgmstream->num_samples = hx->num_samples;

            xma_fix_raw_samples_ch(vgmstream, streamData, hx->stream_offset,hx->stream_size, hx->channels, 0,0);
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

            vgmstream->codec_data = init_ffmpeg_atrac3_raw(streamData, hx->stream_offset,hx->stream_size, vgmstream->num_samples,vgmstream->channels,vgmstream->sample_rate, block_align, encoder_delay);
            if (!vgmstream->codec_data) goto fail;
            vgmstream->coding_type = coding_FFmpeg;
            vgmstream->layout_type = layout_none;
            break;
        }
#endif

        default:
            goto fail;
    }

    strcpy(vgmstream->stream_name, hx->readable_name);

    if (!vgmstream_open_stream(vgmstream, streamData, hx->stream_offset))
        goto fail;
    close_streamfile(streamTemp);
    return vgmstream;

fail:
    close_streamfile(streamTemp);
    close_vgmstream(vgmstream);
    return NULL;
}

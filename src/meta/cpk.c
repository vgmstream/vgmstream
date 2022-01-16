#include "meta.h"
#include "../coding/coding.h"
#include "cri_utf.h"


typedef enum { HCA, CWAV, ADX } cpk_type_t;

static void load_cpk_name(STREAMFILE* sf, STREAMFILE* sf_acb, VGMSTREAM* vgmstream, int waveid);

/* CPK - CRI container as audio bank [Metal Gear Solid: Snake Eater 3D (3DS), Street Fighter X Tekken (X360), Ace Combat Infinity (PS3)] */
VGMSTREAM* init_vgmstream_cpk(STREAMFILE* sf) {
    return init_vgmstream_cpk_memory(sf, NULL);
}

VGMSTREAM* init_vgmstream_cpk_memory(STREAMFILE* sf, STREAMFILE* sf_acb) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset = 0;
    size_t subfile_size = 0;
    utf_context* utf = NULL;
    int total_subsongs, target_subsong = sf->stream_index;
    int subfile_id = 0;
    cpk_type_t type;
    const char* extension = NULL;
    uint32_t* sizes = NULL;


    /* checks */
    if (!check_extensions(sf, "awb"))
        goto fail;
    if (!is_id32be(0x00,sf, "CPK "))
        goto fail;
    if (!is_id32be(0x10,sf, "@UTF"))
        goto fail;
    /* 04: 0xFF? */
    /* 08: 0x02A0? */
    /* 0c: null? */

    /* CPK .cpk is CRI's generic file container, but here we only support CPK .awb used as
     * early audio bank, that like standard AFS2 .awb comes with .acb */
    {
        int rows, i;
        const char* name;
        const char* Tvers;
        uint32_t table_offset = 0, offset;
        uint32_t Files = 0, FilesL = 0, FilesH = 0;
        uint64_t ContentOffset = 0, ItocOffset = 0;
        uint16_t Align = 0;
        uint32_t DataL_offset = 0, DataL_size = 0, DataH_offset = 0, DataH_size = 0;

        /* base header */
        table_offset = 0x10;
        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf || strcmp(name, "CpkHeader") != 0 || rows != 1)
            goto fail;

        if (!utf_query_string(utf, 0, "Tvers", &Tvers) ||
            !utf_query_u32(utf, 0, "Files", &Files) ||
            !utf_query_u64(utf, 0, "ContentOffset", &ContentOffset) || /* absolute */
            !utf_query_u64(utf, 0, "ItocOffset", &ItocOffset) || /* Toc seems used for regular files */
            !utf_query_u16(utf, 0, "Align", &Align))
            goto fail;

        utf_close(utf);
        utf = NULL;

        if (strncmp(Tvers, "awb", 3) != 0) /* starts with "awb" + ".(version)" (SFvTK, MGS3D) or " for (version)" (ACI) */
            goto fail;
        if (Files <= 0)
            goto fail;


        /* Itoc header (regular .CPK tend to use Toc or Etoc header) */
        table_offset = 0x10 + ItocOffset;
        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf) goto fail;

        if (rows != 1 || strcmp(name, "CpkItocInfo") != 0)
            goto fail;

        if (!utf_query_u32(utf, 0, "FilesL", &FilesL) || 
            !utf_query_u32(utf, 0, "FilesH", &FilesH) ||
            !utf_query_data(utf, 0, "DataL", &DataL_offset, &DataL_size) || /* absolute */
            !utf_query_data(utf, 0, "DataH", &DataH_offset, &DataH_size))   /* absolute */
            goto fail;

        utf_close(utf);
        utf = NULL;


        /* For maximum annoyance there are 2 tables (small+big files) that only list sizes,
         * and files can be mixed (small small big small big).
         * Must pre-read all entries to find actual offset plus subsongs number. */
        if (FilesL + FilesH != Files)
            goto fail;

        total_subsongs = Files;
        if (target_subsong == 0) target_subsong = 1;
        if (target_subsong > total_subsongs || total_subsongs <= 0) goto fail;

        sizes = calloc(Files, sizeof(uint32_t));
        if (!sizes) goto fail;

        /* DataL header */
        table_offset = DataL_offset;
        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf || strcmp(name, "CpkItocL") != 0 || rows != FilesL)
            goto fail;

        for (i = 0; i < rows; i++) {
            uint16_t ID = 0;
            uint16_t FileSize, ExtractSize;

            if (!utf_query_u16(utf, i, "ID", &ID) ||
                !utf_query_u16(utf, i, "FileSize", &FileSize) ||
                !utf_query_u16(utf, i, "ExtractSize", &ExtractSize))
                goto fail;

            if (ID >= Files || FileSize != ExtractSize || sizes[ID])
                goto fail;

            sizes[ID] = FileSize;
        }

        utf_close(utf);
        utf = NULL;

        /* DataR header */
        table_offset = DataH_offset;
        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf || strcmp(name, "CpkItocH") != 0 || rows != FilesH)
            goto fail;

        for (i = 0; i < rows; i++) {
            uint16_t ID = 0;
            uint32_t FileSize, ExtractSize;

            if (!utf_query_u16(utf, i, "ID", &ID) ||
                !utf_query_u32(utf, i, "FileSize", &FileSize) ||
                !utf_query_u32(utf, i, "ExtractSize", &ExtractSize))
                goto fail;

            if (ID >= Files || FileSize != ExtractSize || sizes[ID])
                goto fail;

            sizes[ID] = FileSize;
        }

        utf_close(utf);
        utf = NULL;


        /* find actual offset */
        offset = ContentOffset;
        for (i = 0; i < Files; i++) {
            uint32_t size = sizes[i];
            if (i + 1 == target_subsong) {
                subfile_id = i;
                subfile_offset = offset;
                subfile_size = size;
                break;
            }

            offset += size;
            if (Align && (offset % Align))
                offset += Align - (offset % Align);
        }
        
        free(sizes);
        sizes = NULL;
    }

    if (!subfile_offset)
        goto fail;

    //;VGM_LOG("CPK: subfile offset=%lx + %x, id=%i\n", subfile_offset, subfile_size, subfile_id);


    if ((read_u32be(subfile_offset,sf) & 0x7f7f7f7f) == get_id32be("HCA\0")) {
        type = HCA;
        extension = "hca";
    }
    else if (is_id32be(subfile_offset,sf, "CWAV")) {
        type = CWAV;
        extension = "bcwav";
    }
    else if (read_u16be(subfile_offset, sf) == 0x8000) {
        type = ADX;
        extension = "adx";
    }
    else {
        goto fail;
    }

    temp_sf = setup_subfile_streamfile(sf, subfile_offset, subfile_size, extension);
    if (!temp_sf) goto fail;

    switch(type) {
        case HCA:
            vgmstream = init_vgmstream_hca(temp_sf);
            if (!vgmstream) goto fail;
            break;
        case CWAV: /* Metal Gear Solid: Snake Eater 3D (3DS) */
            vgmstream = init_vgmstream_bcwav(temp_sf);
            if (!vgmstream) goto fail;
            break;
        case ADX: /* Sonic Generations (3DS) */
            vgmstream = init_vgmstream_adx(temp_sf);
            if (!vgmstream) goto fail;
            break;
        default:
            goto fail;
    }

    vgmstream->num_streams = total_subsongs;

    /* try to load cue names */
    load_cpk_name(sf, sf_acb, vgmstream, subfile_id);

    close_streamfile(temp_sf);
    return vgmstream;

fail:
    free(sizes);
    utf_close(utf);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

static void load_cpk_name(STREAMFILE* sf, STREAMFILE* sf_acb, VGMSTREAM* vgmstream, int waveid) {
    int is_memory = (sf_acb != NULL);
    int port = -1; /* cpk has no port numbers */

    /* .acb is passed when loading memory .awb inside .acb */
    if (!is_memory) {
        /* try parsing TXTM if present */
        sf_acb = read_filemap_file(sf, 0);

        /* try (name).awb + (name).awb */
        if (!sf_acb)
            sf_acb = open_streamfile_by_ext(sf, "acb");

        /* (name)_streamfiles.awb + (name).acb also exist */

        if (!sf_acb)
            return;

        /* companion .acb probably loaded */
        load_acb_wave_name(sf_acb, vgmstream, waveid, port, is_memory);

        close_streamfile(sf_acb);
    }
    else {
        load_acb_wave_name(sf_acb, vgmstream, waveid, port, is_memory);
    }
}

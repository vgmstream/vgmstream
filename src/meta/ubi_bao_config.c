#include "../util/endianness.h"
#include "../util/log.h"
#include "ubi_bao_config.h"


void ubi_bao_config_endian(ubi_bao_config_t* cfg, STREAMFILE* sf, off_t offset) {

    /* Detect endianness using the 'class' field (the 'header skip' field is LE in early
     * versions, and was removed in later versions).
     * This could be done once as all BAOs share endianness */

    /* negate as fields looks like LE (0xN0000000) */
    cfg->big_endian = !guess_endian32(offset + cfg->bao_class, sf);
}


static void config_bao_entry(ubi_bao_config_t* cfg, size_t header_base_size, size_t header_skip) {
    cfg->header_base_size       = header_base_size;
    cfg->header_skip            = header_skip;
}

/* audio header base */
static void config_bao_audio_b(ubi_bao_config_t* cfg, off_t stream_size, off_t stream_id, off_t stream_flag, off_t loop_flag, int stream_and, int loop_and) {
    cfg->audio_stream_size          = stream_size;
    cfg->audio_stream_id            = stream_id;
    cfg->audio_stream_flag          = stream_flag;
    cfg->audio_loop_flag            = loop_flag;
    cfg->audio_stream_and           = stream_and;
    cfg->audio_loop_and             = loop_and;
}
/* audio header main */
static void config_bao_audio_m(ubi_bao_config_t* cfg, off_t channels, off_t sample_rate, off_t num_samples, off_t num_samples2, off_t stream_type, off_t prefetch_size) {
    cfg->audio_channels             = channels;
    cfg->audio_sample_rate          = sample_rate;
    cfg->audio_num_samples          = num_samples;
    cfg->audio_num_samples2         = num_samples2;
    cfg->audio_stream_type          = stream_type;
    cfg->audio_prefetch_size        = prefetch_size;
}

static void config_bao_audio_c(ubi_bao_config_t* cfg, off_t cue_count, off_t cue_labels, off_t cue_size) {
    /* audio header extra */
    cfg->audio_cue_count            = cue_count;
    cfg->audio_cue_labels           = cue_labels;
  //cfg->audio_cue_size             = cue_size;
}

static void config_bao_sequence(ubi_bao_config_t* cfg, off_t sequence_count, off_t sequence_single, off_t sequence_loop, off_t entry_size) {
    /* sequence header and chain table */
    cfg->sequence_sequence_count    = sequence_count;
    cfg->sequence_sequence_single   = sequence_single;
    cfg->sequence_sequence_loop     = sequence_loop;
    cfg->sequence_entry_size        = entry_size;
    cfg->sequence_entry_number      = 0x00;
}

static void config_bao_layer_m(ubi_bao_config_t* cfg, off_t stream_id, off_t layer_count, off_t stream_flag, off_t stream_size, off_t extra_size, off_t prefetch_size, off_t cue_count, off_t cue_labels, int stream_and) {
    /* layer header in the main part */
    cfg->layer_stream_id            = stream_id;
    cfg->layer_layer_count          = layer_count;
    cfg->layer_stream_flag          = stream_flag;
    cfg->layer_stream_size          = stream_size;
    cfg->layer_extra_size           = extra_size;
    cfg->layer_prefetch_size        = prefetch_size;
    cfg->layer_cue_count            = cue_count;
    cfg->layer_cue_labels           = cue_labels;
    cfg->layer_stream_and           = stream_and;
}
static void config_bao_layer_e(ubi_bao_config_t* cfg, off_t entry_size, off_t sample_rate, off_t channels, off_t stream_type, off_t num_samples) {
    /* layer sub-headers in extra table */
    cfg->layer_entry_size           = entry_size;
    cfg->layer_sample_rate          = sample_rate;
    cfg->layer_channels             = channels;
    cfg->layer_stream_type          = stream_type;
    cfg->layer_num_samples          = num_samples;
}

static void config_bao_silence_f(ubi_bao_config_t* cfg, off_t duration) {
    /* silence headers in float value */
    cfg->silence_duration_float     = duration;
}


bool ubi_bao_config_version(ubi_bao_config_t* cfg, STREAMFILE* sf, uint32_t version) {

    /* Ubi BAO evolved from Ubi SB and are conceptually quite similar, see that first.
     *
     * BAOs (binary audio objects) always start with:
     * - 0x00(1): format (meaning defined by mode)
     * - 0x01(3): 8b*3 version, major/minor/release (numbering continues from .sb0/sm0)
     * - 0x04+: mini header (varies with version, see parse_header)
     *
     * Then are divided into "classes":
     * - 0x10000000: event (links by id to another event or header BAO, may set volume/reverb/filters/etc)
     * - 0x20000000: header
     * - 0x30000000: memory audio (in .pk/.bao)
     * - 0x40000000: project info
     * - 0x50000000: stream audio (in .spk/.sbao, sometimes .bao too)
     * - 0x60000000: unused?
     * - 0x70000000: info? has a count+table of id-things
     * - 0x80000000: unknown (some floats?)
     * - 0x90000000: unknown (some kind of command config?), rare [Ghost Recon Future Soldier (PC), Drawsome (Wii)]
     * Class 1/2/3 are roughly equivalent to Ubi SB's section1/2/3, and class 4 is
     * basically .spN project files.
     *
     * The project BAO (usually with special id 0x7FFFFFFF or 0x40000000) has version,
     * filenames (not complete) and current mode, "PACKAGE" (pk, index + BAOs with
     * external BAOs) or "ATOMIC" (file, separate BAOs). For .SPK the project BAO doesn't define type.
     *
     * Right after base BAO size is the extra table for that BAO (what sectionX had, plus
     * extra crap like cue-like labels, even for type 0x01).
     *
     * Just to throw us off, the base BAO size may add +0x04 (with a field value of 0/-1) on
     * some game versions/platforms (PC/Wii only?). Doesn't look like there is a header field
     * (comparing many BAOs from different platforms of the same games) so it's autodetected.
     *
     * Most types + tables are pretty much the same as SB (with config styles ported straight) but
     * now can "prefetch" part of the data (signaled by a size in the header, or perhaps a flag but
     * looks too erratic). The header points to a external/stream ID, and with prefetch enabled part
     * of the audio is in an internal/memory ID, and must join both during reads to get the full
     * stream. Prefetch may be used in some platforms of a game only (ex. AC1 PC does while PS3
     * doesn't, while Scott Pilgrim always does)
     */

    cfg->version = version;

    // We want usable 'header' types, also similar to SB types (see type_map in ubi_bao_parser.c):
    cfg->allowed_types[0x01] = true; //sfx
    cfg->allowed_types[0x05] = true; //sequence
    cfg->allowed_types[0x06] = true; //layers


    /* Most config offsets are relative to header_skip, since that's where the header payload actually starts.
     * Memory data also starts at header_skip.
     *
     * - base part in early versions:
     * 0x00: version ID
     * 0x04: header size (usually 0x28, rarely 0x24), can be LE unlike other fields (ex. Assassin's Creed PS3, but not in all games)
     * 0x08(10): GUID, or id-like fields in early versions
     * 0x18: null
     * 0x1c: null
     * 0x20: class
     * 0x24: config/version? (0x00/0x01/0x02), removed in some versions
     * (payload starts)
     *
     * - base part in later versions:
     * 0x00: version ID
     * 0x04(10): GUID
     * 0x14: class
     * 0x18: config/version? (0x02)
     * (payload starts)
     */

    cfg->bao_class      = 0x20; // absolute offset, unlike the rest
    cfg->header_id      = 0x00; // relative
    cfg->header_type    = 0x04; // relative
    cfg->parser = PARSER_1B;

    if ((cfg->version & 0xFFFF0000) >= 0x00290000) {
        cfg->bao_class   = 0x14; // absolute
        cfg->header_id   = 0x04; // relative
        cfg->header_type = 0x2c; // relative
        cfg->header_skip = 0x1c;

        cfg->engine_version = (version >> 8) & 0xFFFF00;
        cfg->parser = PARSER_29;
    }

    /* 2 configs with same ID, autodetect */
    if (cfg->version == 0x00220015) {
        off_t header_size = 0x40 + read_u32le(0x04, sf); // first is always LE

        /* next BAO uses machine endianness, entry should always exist
         * (maybe should use project BAO to detect?) */
        if (guess_endian32(header_size + 0x04, sf)) {
            version |= 0xF000; // signal Wii=BE, but don't modify original
        }
    }


    /* config per version */
    switch(version) {

        case 0x001B0100: // Assassin's Creed (PS3/X360/PC)-atomic-forge
                         // My Fitness Coach (Wii)-atomic-forge, Your Shape featuring Jenny McCarthy (Wii)-atomic-forge
        case 0x001B0200: // Beowulf (PS3/X360)-atomic-fat+bin
        case 0x001C0000: // Lost: Via Domus (PS3)-atomic-gear
            if (cfg->version == 0x001B0100)
                config_bao_entry(cfg, 0xA4, 0x28); // PC: 0xA8, PS3/X360: 0xA4
            else
                config_bao_entry(cfg, 0xA0, 0x24);

            config_bao_audio_b(cfg, 0x08, 0x1c, 0x28, 0x34, 1, 1); // 0x2c: prefetch flag?
            config_bao_audio_m(cfg, 0x44, 0x48, 0x50, 0x58, 0x64, 0x74);

            config_bao_sequence(cfg, 0x2c, 0x20, 0x1c, 0x14);

            config_bao_layer_m(cfg, 0x4c, 0x20, 0x2c, 0x44, 0x00, 0x50, 0x58, 0x5c, 1); // stream size: 0x48?
            config_bao_layer_e(cfg, 0x30, 0x00, 0x04, 0x08, 0x10);

            config_bao_silence_f(cfg, 0x1c);

            config_bao_audio_c(cfg, 0x68, 0x6c, 0x78); // only seen in Beowulf

            cfg->codec_map[0x00] = RAW_XMA1_mem;
            cfg->codec_map[0x01] = RAW_PCM; //rare (Beowulf)
            cfg->codec_map[0x02] = RAW_PSX;
            cfg->codec_map[0x03] = UBI_IMA;
            cfg->codec_map[0x04] = FMT_OGG;
            cfg->codec_map[0x05] = RAW_XMA1_str;
            cfg->codec_map[0x07] = RAW_AT3;

            cfg->audio_stream_subtype = 0x78;

            cfg->v1_bao = true; // 0x20xxxxxx 0x30xxxxxx 0x50xxxxxx names

            break;

        case 0x001D0A00: // Shaun White Snowboarding (PSP)-atomic-gear
            config_bao_entry(cfg, 0x84, 0x24);

            config_bao_audio_b(cfg, 0x08, 0x1c, 0x20, 0x20, (1 << 2), (1 << 5));
            config_bao_audio_m(cfg, 0x28, 0x30, 0x38, 0x40, 0x4c, 0x5c);

            config_bao_layer_m(cfg, 0x3c, 0x20, 0x24, 0x34, 0x3c, 0x40, 0x00, 0x00, (1 << 2)); // 0x1c: bao group id?, (1<<1) = layer loop flag
            config_bao_layer_e(cfg, 0x38, 0x00, 0x04, 0x08, 0x10);

            config_bao_silence_f(cfg, 0x1c);

            cfg->codec_map[0x06] = RAW_PSX_new;
            cfg->codec_map[0x07] = FMT_AT3; //layer only

            cfg->v1_bao = true; // 0x20xxxxxx 0x30xxxxxx 0x50xxxxxx names

            break;

        case 0x001F0008: // Rayman Raving Rabbids: TV Party (Wii)-package
        case 0x001F0010: // Prince of Persia 2008 (PC/PS3/X360)-atomic, Far Cry 2 (PS3)-spk
        case 0x001F0011: // Naruto: The Broken Bond (X360)-package
        case 0x0021000C: // Splinter Cell: Conviction (E3 2009 Demo)(X360)-package
        case 0x0022000D: // Just Dance (Wii)-package, We Dare (PS3/Wii)-package
        case 0x0022F015: // James Cameron's Avatar: The Game (Wii)-package
        case 0x00220017: // James Cameron's Avatar: The Game (PS3/X360/PC)-spk
        case 0x00220018: // James Cameron's Avatar: The Game (PS3/X360/PC)-spk
        case 0x0022001B: // Prince of Persia: The Forgotten Sands (Wii)-package
            config_bao_entry(cfg, 0xA4, 0x28); // PC/Wii: 0xA8

            config_bao_audio_b(cfg, 0x08, 0x1c, 0x28, 0x34, 1, 1);
            config_bao_audio_m(cfg, 0x44, 0x4c, 0x54, 0x5c, 0x64, 0x74);

            config_bao_sequence(cfg, 0x2c, 0x20, 0x1c, 0x14);

            config_bao_layer_m(cfg, 0x00, 0x20, 0x2c, 0x44, 0x4c, 0x50, 0x54, 0x58, 1); // 0x1c: id-like, 0x3c: prefetch flag?
            config_bao_layer_e(cfg, 0x28, 0x00, 0x04, 0x08, 0x10);

            config_bao_silence_f(cfg, 0x1c);

            if (cfg->version == 0x0022000D) // We Dare (Wii)
                config_bao_audio_c(cfg, 0x68, 0x6c, 0x78);

          //cfg->codec_map[0x00] = RAW_XMA1_MEM;
            cfg->codec_map[0x01] = RAW_PCM;
          //cfg->codec_map[0x02] = RAW_PSX;
            cfg->codec_map[0x03] = UBI_IMA;
            cfg->codec_map[0x04] = FMT_OGG;
            cfg->codec_map[0x05] = RAW_XMA1_str;
            cfg->codec_map[0x07] = RAW_AT3;
            cfg->codec_map[0x09] = RAW_DSP;

            if (cfg->version >= 0x00220000) // Avatar (X360)
                cfg->codec_map[0x05] = RAW_XMA2_old;

            if (cfg->version == 0x0022000D) // Just Dance (Wii) oddity
                cfg->audio_ignore_external_size = true;

            cfg->audio_stream_subtype = 0x78;

            break;

        case 0x00220015: // James Cameron's Avatar: The Game (PSP)-package
        case 0x0022001E: // Prince of Persia: The Forgotten Sands (PSP)-package
            config_bao_entry(cfg, 0x84, 0x28);

            config_bao_audio_b(cfg, 0x08, 0x1c, 0x20, 0x20, (1 << 2), (1 << 5)); // (1 << 4): prefetch flag?
            config_bao_audio_m(cfg, 0x28, 0x30, 0x38, 0x40, 0x48, 0x58);

            config_bao_layer_m(cfg, 0x00, 0x20, 0x24, 0x34, 0x3c, 0x40, 0x00, 0x00, (1 << 2)); // 0x1c: bao group id?
            config_bao_layer_e(cfg, 0x28, 0x00, 0x04, 0x08, 0x10);

            cfg->codec_map[0x06] = RAW_PSX_new;
            cfg->codec_map[0x07] = FMT_AT3;

            break;

        case 0x00230008: // Splinter Cell: Conviction (X360/PC)-package
            config_bao_entry(cfg, 0xB4, 0x28); // PC: 0xB8, X360: 0xB4

            config_bao_audio_b(cfg, 0x08, 0x24, 0x38, 0x44, 1, 1);
            config_bao_audio_m(cfg, 0x54, 0x5c, 0x64, 0x6c, 0x74, 0x84);

            config_bao_sequence(cfg, 0x34, 0x28, 0x24, 0x14);

            config_bao_layer_m(cfg, 0x00, 0x28, 0x3c, 0x54, 0x5c, 0x00 /*0x60?*/, 0x00, 0x00, 1); // 0x24: id-like
            config_bao_layer_e(cfg, 0x30, 0x00, 0x04, 0x08, 0x18);

            cfg->codec_map[0x01] = RAW_PCM;
            cfg->codec_map[0x02] = UBI_IMA;
            cfg->codec_map[0x03] = FMT_OGG;
            cfg->codec_map[0x04] = RAW_XMA2_old;

            cfg->layer_ignore_error = true; //TODO: last sfx layer (bass) may have smaller sample rate

            break;

        case 0x00250108: // Scott Pilgrim vs the World (PS3/X360)-package
        case 0x0025010A: // Prince of Persia: The Forgotten Sands (PS3/X360)-atomic-forge
        case 0x00250119: // Shaun White Skateboarding (Wii)-package
        case 0x0025011D: // Shaun White Skateboarding (PC/PS3)-atomic-forge
            config_bao_entry(cfg, 0xB4, 0x28); // PC: 0xB0, PS3/X360: 0xB4, Wii: 0xB8, 0x0025010A: 0x84

            config_bao_audio_b(cfg, 0x08, 0x24, 0x2c, 0x38, 1, 1);
            config_bao_audio_m(cfg, 0x48, 0x50, 0x58, 0x60, 0x68, 0x78);

            config_bao_sequence(cfg, 0x34, 0x28, 0x24, 0x14);

            config_bao_layer_m(cfg, 0x00, 0x28, 0x30, 0x48, 0x50, 0x54, 0x58, 0x5c, 1); // 0x24: id-like
            config_bao_layer_e(cfg, 0x30, 0x00, 0x04, 0x08, 0x18);

            config_bao_silence_f(cfg, 0x24);

            cfg->codec_map[0x01] = RAW_PCM;
            cfg->codec_map[0x02] = UBI_IMA;
            cfg->codec_map[0x03] = FMT_OGG;
            cfg->codec_map[0x04] = RAW_XMA2_new;
            cfg->codec_map[0x05] = RAW_PSX;
            cfg->codec_map[0x06] = RAW_AT3;

            if (cfg->version == 0x0025011D)
                cfg->header_less_le_flag = true;

            cfg->audio_stream_subtype = 0x88;

            cfg->layer_default_subtype = 2; //Scott Pilgrim, Shaun White
            if (cfg->version == 0x0025010A) // no apparent flag
                cfg->layer_default_subtype = 1; // not used?

            //TODO: some SPvsTW layers look like should loop (0x30 flag?)
            //TODO: some POP layers have different sample rates (ambience)

            break;

        case 0x00260000: // Michael Jackson: The Experience (X360)-package
        case 0x00260102: // Prince of Persia Trilogy HD (PS3)-package-gear
            config_bao_entry(cfg, 0xB8, 0x28);

            config_bao_audio_b(cfg, 0x08, 0x28, 0x30, 0x3c, 1, 1); //loop?
            config_bao_audio_m(cfg, 0x4c, 0x54, 0x5c, 0x64, 0x6c, 0x7c);

            config_bao_sequence(cfg, 0x38, 0x2c, 0x28, 0x14);

            config_bao_layer_m(cfg, 0x00, 0x2c, 0x34,  0x4c, 0x54, 0x58,  0x00, 0x00, 1);
            config_bao_layer_e(cfg, 0x34, 0x00, 0x04, 0x08, 0x1c);

          //cfg->codec_map[0x01] = RAW_PCM;
            cfg->codec_map[0x02] = UBI_IMA;
            cfg->codec_map[0x03] = FMT_OGG;
            cfg->codec_map[0x04] = RAW_XMA2_new;
          //cfg->codec_map[0x05] = RAW_PSX;
            cfg->codec_map[0x06] = RAW_AT3;

            cfg->audio_ignore_external_size = true; // leave_me_alone.pk

            cfg->audio_stream_subtype = 0x8c;

            //TODO: some POP SOT IMA sound off, but seem correctly parsed
            // (rip error? ex. 2a86c5ca.pk + d0a4ef615adf1c5a4e17f3ac.spk)

            break;

        case 0x00270102: // Drawsome (Wii)-package
        case 0x00280102: // Tom Clancy's Ghost Recon Future Soldier Beta (X360)-package
            config_bao_entry(cfg, 0xAC, 0x28);

            config_bao_audio_b(cfg, 0x08, 0x28, 0x2c, 0x38, 1, 1);
            config_bao_audio_m(cfg, 0x44, 0x4c, 0x54, 0x5c, 0x64, 0x70);

            config_bao_sequence(cfg, 0x38, 0x2c, 0x28, 0x14);

            config_bao_layer_m(cfg, 0x00, 0x2c, 0x34, 0x48, 0x50, 0x54, 0x58, 0x5c, 1);
            config_bao_layer_e(cfg, 0x2c, 0x00, 0x04, 0x08, 0x1c);

            config_bao_audio_c(cfg, 0x68, 0x6c, 0x80);

            cfg->codec_map[0x02] = UBI_IMA;
            cfg->codec_map[0x04] = FMT_OGG;
            cfg->codec_map[0x05] = RAW_XMA2_new;

            // ex. Mus_Proto_BodyDetection.pk (standard) vs MP_Proto_Dynamic_Insertion.pk (odd)
            cfg->audio_fix_xma_memory_baos = true; 
            cfg->layer_ignore_error = true; // some layers have different sample rates

            break;

        case 0x00280303: // Tom Clancy's Ghost Recon Future Soldier (PC/PS3)-package
        case 0x00280305: // Far Cry 3: Blood Dragon (X360/PS3)-spk-dunia
        case 0x00280306: // Far Cry 3: Blood Dragon (X360/PS3)-spk-dunia
            config_bao_entry(cfg, 0xBC, 0x28);

            config_bao_audio_b(cfg, 0x08, 0x38, 0x3c, 0x48, 1, 1);
            config_bao_audio_m(cfg, 0x54, 0x5c, 0x64, 0x6c, 0x74, 0x80);

            config_bao_sequence(cfg, 0x48, 0x3c, 0x38, 0x14);

            config_bao_layer_m(cfg, 0x00, 0x3c, 0x44, 0x58, 0x60, 0x64, 0x68, 0x6c, 1);
            config_bao_layer_e(cfg, 0x2c, 0x00, 0x04, 0x08, 0x1c);

            config_bao_silence_f(cfg, 0x38);

            config_bao_audio_c(cfg, 0x78, 0x7c, 0x00);

            cfg->codec_map[0x01] = RAW_PCM;
            cfg->codec_map[0x02] = UBI_IMA; // v6
            cfg->codec_map[0x03] = UBI_IMA_seek; // v6
            cfg->codec_map[0x04] = FMT_OGG;
            cfg->codec_map[0x05] = RAW_XMA2_new;
            cfg->codec_map[0x06] = RAW_PSX_new;
            cfg->codec_map[0x07] = RAW_AT3;

            cfg->audio_fix_xma_samples = true; // W H Y
            cfg->layer_ignore_error = true; //TODO: some GR layer sample rates don't match
            cfg->audio_stream_subtype = 0x90;

            //TODO: some GR files have strange prefetch+stream of same size (2 segments?), ex. CEND_30_VOX.lpk

            // FC3 has a few files that are truncated a few bytes (extraction issue?)
            // FC3 also has file with garbage in the upper bytes of type (00448bc8.spk)
            break;

        case 0x00290106: // Splinter Cell: Blacklist (PS3/X360)-atomic-gear
        case 0x002A0300: // Watch_Dogs (X360/PS3/Wii U)-spk-dunia, Far Cry 3: Blood Dragon (PS4)-spk-dunia
            // - v29: new BAO format
            // - v2A: modified layers, inline data with no header

            cfg->codec_map[0x00] = CODEC_NONE;
            cfg->codec_map[0x01] = RAW_PCM;
            cfg->codec_map[0x02] = UBI_IMA; // v6
            cfg->codec_map[0x03] = UBI_IMA_seek; // v6 //TODO: header format is a bit different
            cfg->codec_map[0x04] = FMT_OGG;
            cfg->codec_map[0x05] = RAW_XMA2_new;
          //cfg->codec_map[0x06] = RAW_PSX_new;
            cfg->codec_map[0x07] = RAW_MP3;
          //cfg->codec_map[0x08] = RAW_DSP;
            cfg->codec_map[0x09] = RAW_AT9;
            break;

        case 0x002B0000: // Far Cry 4 (multi)-spk-dunia
        case 0x002B0100: // Far Cry 4 (multi)-spk-dunia, Far Cry Primal (multi)-spk-dunia
            // - v2B: extra audio/layer flags, modified audio ATRAC9 extradata

            cfg->codec_map[0x00] = CODEC_NONE;      //SAMPLE_INVALID
            cfg->codec_map[0x01] = RAW_PCM;         //'PCM'
            cfg->codec_map[0x02] = UBI_IMA;         //'IMAADPCM' (v6)
            cfg->codec_map[0x03] = UBI_IMA_seek;    //'IMADPCM Seekable Everywhere' (v6)
            cfg->codec_map[0x04] = UBI_IMA_mark;    //'IMADPCM Seekable on Wave Markers'
            cfg->codec_map[0x05] = FMT_OGG;         //'OGG'
            cfg->codec_map[0x06] = RAW_XMA2_new;    //'XMA2'
          //cfg->codec_map[0x07] = RAW_PSX_new;     //'VAG'
            cfg->codec_map[0x08] = RAW_MP3;         //'MP3'
          //cfg->codec_map[0x09] = RAW_DSP;         //'Nintendon ADPCM' (sic)
            cfg->codec_map[0x0A] = RAW_AT9;         //'ATRAC9'

            // FC4 PS3 very rarely has wrong .sbao size (001a400f.spk + 001a3fa5.sbao), should clamp atomic BAO
            //cfg->ignore_atomic_bao_size = true;
            break;

        default: 
            // others possibly using BAO: Tom Clancy's series (Far Cry 5+, Watch_Dogs 2+ uses Wwise)
            vgm_logi("UBI BAO: unknown BAO version %08x (report)\n", cfg->version);
            return false;
    }

    return true;
}

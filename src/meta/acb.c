#include "meta.h"
#include "../coding/coding.h"
#include "cri_utf.h"


/* ACB (Atom Cue sheet Binary) - CRI container of memory audio, often together with a .awb wave bank */
VGMSTREAM* init_vgmstream_acb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    uint32_t subfile_offset;
    uint32_t subfile_size;
    utf_context* utf = NULL;


    /* checks */
    if (!is_id32be(0x00,sf, "@UTF"))
        goto fail;
    if (!check_extensions(sf, "acb"))
        goto fail;

    /* .acb is a cue sheet that uses @UTF (CRI's generic table format) to store row/columns
     * with complex info (cues, sequences, spatial info, etc). It can store a memory .awb
     * (our target here), or reference external/streamed .awb (loaded elsewhere)
     * we only want .awb with actual waves but may use .acb to get names */
    {
        int rows;
        const char* name;
        uint32_t offset = 0, size = 0;
        uint32_t table_offset = 0x00;

        utf = utf_open(sf, table_offset, &rows, &name);
        if (!utf) goto fail;

        if (rows != 1 || strcmp(name, "Header") != 0)
            goto fail;

        if (!utf_query_data(utf, 0, "AwbFile", &offset, &size))
            goto fail;

        subfile_offset = table_offset + offset;
        subfile_size = size;

        /* column exists but can be empty */
        if (subfile_size == 0) {
            vgm_logi("ACB: bank has no subsongs (ignore)\n");
            goto fail;
        }
    }

    //;VGM_LOG("ACB: subfile offset=%lx + %x\n", subfile_offset, subfile_size);

    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, "awb");
    if (!temp_sf) goto fail;

    if (is_id32be(0x00, temp_sf, "CPK ")) {
        vgmstream = init_vgmstream_cpk_memory(temp_sf, sf); /* older */
        if (!vgmstream) goto fail;
    }
    else {
        vgmstream = init_vgmstream_awb_memory(temp_sf, sf); /* newer */
        if (!vgmstream) goto fail;
    }

    /* name-loading for this for memory .awb will be called from init_vgmstream_awb/cpk_memory */

    utf_close(utf);
    close_streamfile(temp_sf);
    return vgmstream;

fail:
    utf_close(utf);
    close_streamfile(temp_sf);
    close_vgmstream(vgmstream);
    return NULL;
}

/* ************************************** */

/* extra config for .acb with lots of sounds, since there is a lot of IO back and forth,
 * ex. +7000 acb+awb subsongs in Ultra Despair Girls (PC) */
//TODO: could pre-load all sections first, but needs cache for multiple subsongs (+semaphs, if multiple read the same thing)
#define ACB_TABLE_BUFFER_CUENAME 0x4000
#define ACB_TABLE_BUFFER_CUE 0x2000
#define ACB_TABLE_BUFFER_BLOCKSEQUENCE 0x8000
#define ACB_TABLE_BUFFER_BLOCK 0x8000
#define ACB_TABLE_BUFFER_SEQUENCE 0x4000
#define ACB_TABLE_BUFFER_TRACK 0x1000
#define ACB_TABLE_BUFFER_TRACKCOMMAND 0x2000
#define ACB_TABLE_BUFFER_SYNTH 0x4000
#define ACB_TABLE_BUFFER_WAVEFORM 0x4000

#define ACB_MAX_NAMELIST 255
#define ACB_MAX_NAME 1024 /* even more is possible in rare cases [Senran Kagura Burst Re:Newal (PC)] */

#define ACB_MAX_BUFFER 0x8000
#define ACB_PRELOAD 1 //todo: remove non-preloading code

static STREAMFILE* setup_acb_streamfile(STREAMFILE* sf, size_t buffer_size) {
    STREAMFILE* new_sf = NULL;

#ifndef ACB_PRELOAD
    /* buffer seems better than reopening when opening multiple subsongs at the same time with STDIO,
     * even though there is more buffer trashing, maybe concurrent IO is slower */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_buffer_streamfile_f(new_sf, buffer_size);
#else
    new_sf = reopen_streamfile(sf, buffer_size);
#endif
    return new_sf;
}


typedef struct  {
    uint16_t CueIndex;
    const char* CueName;
} CueName_t;

typedef struct  {
    uint8_t ReferenceType;
    uint16_t ReferenceIndex;
} Cue_t;

typedef struct  {
    uint16_t NumTracks;
    uint32_t TrackIndex_offset;
    uint32_t TrackIndex_size;
    uint16_t NumBlocks;
    uint32_t BlockIndex_offset;
    uint32_t BlockIndex_size;
} BlockSequence_t;

typedef struct  {
    uint16_t NumTracks;
    uint32_t TrackIndex_offset;
    uint32_t TrackIndex_size;
} Block_t;

typedef struct  {
    uint16_t NumTracks;
    uint32_t TrackIndex_offset;
    uint32_t TrackIndex_size;
    uint16_t ActionTrackStartIndex;
    uint16_t NumActionTracks;
    uint32_t TrackValues_offset;
    uint32_t TrackValues_size;
    uint8_t Type;
} Sequence_t;

typedef struct  {
    uint16_t EventIndex;
} Track_t;

typedef struct  {
    uint32_t Command_offset;
    uint32_t Command_size;
} TrackCommand_t;

typedef struct  {
    uint8_t Type;
    uint32_t ReferenceItems_offset;
    uint32_t ReferenceItems_size;
} Synth_t;

typedef struct  {
    uint16_t Id;
    uint16_t PortNo;
    uint8_t Streaming;
} Waveform_t;


typedef struct {
    STREAMFILE* acbFile; /* original reference, don't close */

    /* keep track of these tables so they can be closed when done */
    utf_context* Header;
    utf_context* TempTable;

    utf_context* CueNameTable;
    utf_context* CueTable;
    utf_context* BlockSequenceTable;
    utf_context* BlockTable;
    utf_context* SequenceTable;
    utf_context* TrackTable;
    utf_context* TrackCommandTable;
    utf_context* SynthTable;
    utf_context* WaveformTable;

    STREAMFILE* TempSf;
    STREAMFILE* CueNameSf;
    STREAMFILE* CueSf;
    STREAMFILE* BlockSequenceSf;
    STREAMFILE* BlockSf;
    STREAMFILE* SequenceSf;
    STREAMFILE* TrackSf;
    STREAMFILE* TrackCommandSf;
    STREAMFILE* SynthSf;
    STREAMFILE* WaveformSf;

    Cue_t* Cue;
    CueName_t* CueName;
    BlockSequence_t* BlockSequence;
    Block_t* Block;
    Sequence_t* Sequence;
    Track_t* Track;
    TrackCommand_t* TrackCommand;
    Synth_t* Synth;
    Waveform_t* Waveform;

    int Cue_rows;
    int CueName_rows;
    int BlockSequence_rows;
    int Block_rows;
    int Sequence_rows;
    int Track_rows;
    int TrackCommand_rows;
    int Synth_rows;
    int Waveform_rows;

    uint8_t* buf;
    int buf_size;

    /* config */
    int is_memory;
    int target_waveid;
    int target_port;
    //todo remove
    int has_TrackEventTable;
    int has_CommandTable;
    int is_preload;

    /* to avoid infinite/circular references (AtomViewer crashes otherwise) */
    int synth_depth;
    int sequence_depth;

    /* name stuff */
    int16_t cuename_index;
    const char* cuename_name;
    int awbname_count;
    int16_t awbname_list[ACB_MAX_NAMELIST];
    char name[ACB_MAX_NAME];


} acb_header;


static int read_buffer(acb_header* acb, uint32_t offset, uint32_t size, STREAMFILE* sf) {
    int bytes;

    if (acb->buf_size < size) {
        if (size > ACB_MAX_BUFFER) {
            VGM_LOG("ACB: buffer too big: %x\n", size);
            goto fail;
        }
        /* could realloc and stuff but... */
        acb->buf_size = ACB_MAX_BUFFER;
        acb->buf = malloc(acb->buf_size * sizeof(uint8_t));
        if (!acb->buf) goto fail;
    }


    bytes = read_streamfile(acb->buf, offset, size, sf);
    if (bytes != size) goto fail;

    return 1;
fail:
    VGM_LOG("ACB: failed buffer");
    return 0;
}

static int open_utf_subtable(acb_header* acb, STREAMFILE* *TableSf, utf_context* *Table, const char* TableName, int* rows, int buffer) {
    uint32_t offset = 0;

    /* already loaded */
    if (*Table != NULL)
        return 1;

    if (!utf_query_data(acb->Header, 0, TableName, &offset, NULL))
        goto fail;

    *TableSf = setup_acb_streamfile(acb->acbFile, buffer);
    if (!*TableSf) goto fail;

    *Table = utf_open(*TableSf, offset, rows, NULL);
    if (!*Table) goto fail;

    //;VGM_LOG("ACB: loaded table %s\n", TableName);
    return 1;
fail:
    return 0;
}

//todo safeops, avoid recalc lens 
static void acb_cat(char* dst, int dst_max, const char* src) {
    int dst_len = strlen(dst);
    int src_len = strlen(dst);
    if (dst_len + src_len > dst_max - 1)
        return;
    strcat(dst, src);
}
static void acb_cpy(char* dst, int dst_max, const char* src) {
    int src_len = strlen(dst);
    if (src_len > dst_max - 1)
        return;
    strcpy(dst, src);
}

static void add_acb_name(acb_header* acb, int8_t Streaming) {
    if (!acb->cuename_name) {
        VGM_LOG("ACB: no name\n");
        return;
    }

    /* ignore name repeats */
    if (acb->awbname_count) {
        int i;
        for (i = 0; i < acb->awbname_count; i++) {
            if (acb->awbname_list[i] == acb->cuename_index)
                return;
        }
    }

    /* since waveforms can be reused by cues, multiple names are a thing */
    if (acb->awbname_count) {
        acb_cat(acb->name, sizeof(acb->name), "; ");
        acb_cat(acb->name, sizeof(acb->name), acb->cuename_name);
    }
    else {
        acb_cpy(acb->name, sizeof(acb->name), acb->cuename_name);
    }
    if (Streaming == 2 && acb->is_memory) {
        acb_cat(acb->name, sizeof(acb->name), " [pre]");
    }

    acb->awbname_list[acb->awbname_count] = acb->cuename_index;
    acb->awbname_count++;
    if (acb->awbname_count >= ACB_MAX_NAMELIST)
        acb->awbname_count = ACB_MAX_NAMELIST - 1; /* ??? */

    //;VGM_LOG("ACB: found cue for waveid=%i: %s\n", acb->target_waveid, acb->cuename_name);
}


/*****************************************************************************/
/* OBJECT HANDLERS */

static int preload_acb_waveform(acb_header* acb) {
    utf_context* Table = acb->WaveformTable;
    int* p_rows = &acb->Waveform_rows;
    int i, c_Id, c_MemoryAwbId, c_StreamAwbId, c_StreamAwbPortNo, c_Streaming;

    if (*p_rows)
        return 1;
    if (!open_utf_subtable(acb, &acb->WaveformSf, &Table, "WaveformTable", p_rows, ACB_TABLE_BUFFER_WAVEFORM))
        goto fail;
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload Waveform=%i\n", *p_rows);

    acb->Waveform = malloc(*p_rows * sizeof(Waveform_t));
    if (!acb->Waveform) goto fail;

    c_Id = utf_get_column(Table, "Id");
    c_MemoryAwbId = utf_get_column(Table, "MemoryAwbId");
    c_StreamAwbId = utf_get_column(Table, "StreamAwbId");
    c_StreamAwbPortNo = utf_get_column(Table, "StreamAwbPortNo");
    c_Streaming = utf_get_column(Table, "Streaming");

    for (i = 0; i < *p_rows; i++) {
        Waveform_t* r = &acb->Waveform[i];

        if (!utf_query_col_u16(Table, i, c_Id, &r->Id)) { /* older versions use Id */
            if (acb->is_memory) {
                utf_query_col_u16(Table, i, c_MemoryAwbId, &r->Id);
                r->PortNo = 0xFFFF;
            } else {
                utf_query_col_u16(Table, i, c_StreamAwbId, &r->Id);
                utf_query_col_u16(Table, i, c_StreamAwbPortNo, &r->PortNo); /* assumed default 0 if doesn't exist */
            }
        }
        else {
            r->PortNo = 0xFFFF;
        }
        utf_query_col_u8(Table, i, c_Streaming, &r->Streaming);
    }

    ;VGM_LOG("acb: preload Waveform done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed Waveform preload\n");
    return 0;
}

static int load_acb_waveform(acb_header* acb, uint16_t Index) {
    Waveform_t* r;
    Waveform_t tmp;

    /* read Waveform[Index] */
    if (acb->is_preload) {
        if (!preload_acb_waveform(acb)) goto fail;
        if (Index > acb->Waveform_rows) goto fail;
        r = &acb->Waveform[Index];
    }
    else {
        r = &tmp;
        if (!open_utf_subtable(acb, &acb->WaveformSf, &acb->WaveformTable, "WaveformTable", NULL, ACB_TABLE_BUFFER_WAVEFORM))
            goto fail;

        if (!utf_query_u16(acb->WaveformTable, Index, "Id", &r->Id)) { /* older versions use Id */
            if (acb->is_memory) {
                if (!utf_query_u16(acb->WaveformTable, Index, "MemoryAwbId", &r->Id))
                    goto fail;
                r->PortNo = 0xFFFF;
            } else {
                if (!utf_query_u16(acb->WaveformTable, Index, "StreamAwbId", &r->Id))
                    goto fail;
                if (!utf_query_u16(acb->WaveformTable, Index, "StreamAwbPortNo", &r->PortNo))
                    r->PortNo = 0; /* assumed */
            }
        }
        else {
            r->PortNo = 0xFFFF;
        }
        if (!utf_query_u8(acb->WaveformTable, Index, "Streaming", &r->Streaming))
            goto fail;
        //;VGM_LOG("ACB: Waveform[%i]: Id=%i, PortNo=%i, Streaming=%i\n", Index, r->Id, r->PortNo, r->Streaming);
    }

    /* not found but valid */
    if (r->Id != acb->target_waveid)
        return 1;

    /* correct AWB port (check ignored if set to -1) */
    if (acb->target_port >= 0 && r->PortNo != 0xFFFF && r->PortNo != acb->target_port)
        return 1;

    /* must match our target's (0=memory, 1=streaming, 2=memory (prefetch)+stream) */
    if ((acb->is_memory && r->Streaming == 1) || (!acb->is_memory && r->Streaming == 0))
        return 1;

    /* aaand finally get name (phew) */
    add_acb_name(acb, r->Streaming);

    return 1;
fail:
    VGM_LOG("ACB: failed Waveform %i\n", Index);
    return 0;
}

/*****************************************************************************/

/* define here for Synths pointing to Sequences */
static int load_acb_sequence(acb_header* acb, uint16_t Index);

static int preload_acb_synth(acb_header* acb) {
    utf_context* Table = acb->SynthTable;
    int* p_rows = &acb->Synth_rows;
    int i, c_Type, c_ReferenceItems;

    if (*p_rows)
        return 1;
    if (!open_utf_subtable(acb, &acb->SynthSf, &Table, "SynthTable", p_rows, ACB_TABLE_BUFFER_SYNTH))
        goto fail;
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload Synth=%i\n", *p_rows);

    acb->Synth = malloc(*p_rows * sizeof(Synth_t));
    if (!acb->Synth) goto fail;

    c_Type = utf_get_column(Table, "Type");
    c_ReferenceItems = utf_get_column(Table, "ReferenceItems");

    for (i = 0; i < *p_rows; i++) {
        Synth_t* r = &acb->Synth[i];

        utf_query_col_u8(Table, i, c_Type, &r->Type);
        utf_query_col_data(Table, i, c_ReferenceItems, &r->ReferenceItems_offset, &r->ReferenceItems_size);
        //;VGM_LOG("ACB: Synth[%i]: Type=%x, ReferenceItems={%x,%x}\n", Index, r->Type, r->ReferenceItems_offset, r->ReferenceItems_size);
    }

    ;VGM_LOG("acb: preload Synth done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed Synth preload\n");
    return 0;
}


static int load_acb_synth(acb_header* acb, uint16_t Index) {
    int i, count;
    Synth_t* r;
    Synth_t tmp;


    /* read Synth[Index] */
    if (acb->is_preload) {
        if (!preload_acb_synth(acb)) goto fail;
        if (Index > acb->Synth_rows) goto fail;
        r = &acb->Synth[Index];
    }
    else {
        r = &tmp;
        if (!open_utf_subtable(acb, &acb->SynthSf, &acb->SynthTable, "SynthTable", NULL, ACB_TABLE_BUFFER_SYNTH))
            goto fail;

        if (!utf_query_u8(acb->SynthTable, Index, "Type", &r->Type))
            goto fail;
        if (!utf_query_data(acb->SynthTable, Index, "ReferenceItems", &r->ReferenceItems_offset, &r->ReferenceItems_size))
            goto fail;
        //;VGM_LOG("ACB: Synth[%i]: Type=%x, ReferenceItems={%x,%x}\n", Index, r->Type, r->ReferenceItems_offset, r->ReferenceItems_size);
    }

    acb->synth_depth++;

    /* sometimes 2 (ex. Yakuza 6) or even 3 (Street Fighter vs Tekken) */
    if (acb->synth_depth > 3) {
        VGM_LOG("ACB: Synth depth too high\n");
        goto fail;
    }

    //todo .CommandIndex > CommandTable
    //todo .TrackValues > TrackTable?

    /* Cue.ReferenceType 2 uses Synth.Type, while 3 always sets it to 0 and uses Sequence.Type instead
     * Both look the same and probably affect which item in the ReferenceItems list is picked:
     * - 0: polyphonic (1 item)
     * - 1: sequential (1 to N?)
     * - 2: shuffle (1 from N?)
     * - 3: random (1 from N?)
     * - 4: random no repeat
     * - 5: switch game variable
     * - 6: combo sequential
     * - 7: switch selector
     * - 8: track transition by selector
     * - other: undefined?
     * Since we want to find all possible Waveforms that could match our id, we ignore Type and just parse all ReferenceItems.
     */

    if (!read_buffer(acb, r->ReferenceItems_offset, r->ReferenceItems_size, acb->SynthSf))
        goto fail;

    count = r->ReferenceItems_size / 0x04;
    for (i = 0; i < count; i++) {
        uint16_t item_type  = get_u16be(acb->buf + i*0x04 + 0x00);
        uint16_t item_index = get_u16be(acb->buf + i*0x04 + 0x02);
        //;VGM_LOG("ACB: Synth.ReferenceItem: type=%x, index=%x\n", item_type, item_index);

        switch(item_type) {
            case 0x00: /* no reference */
                count = 0;
                break;

            case 0x01: /* Waveform (most common) */
                if (!load_acb_waveform(acb, item_index))
                    goto fail;
                break;

            case 0x02: /* Synth, possibly random (rare, found in Sonic Lost World with ReferenceType 2) */
                if (!load_acb_synth(acb, item_index))
                    goto fail;
                break;

            case 0x03: /* Sequence of Synths w/ % in Synth.TrackValues (rare, found in Sonic Lost World with ReferenceType 2) */
                if (!load_acb_sequence(acb, item_index))
                    goto fail;
                break;

            /* others: same as cue's ReferenceType? */

            case 0x06:  /* this seems to point to Synth but results don't make sense (rare, from Sonic Lost World) */
            default: /* undefined/crashes AtomViewer */
                VGM_LOG("ACB: unknown Synth.ReferenceItem type %x at %x + %x\n", item_type, r->ReferenceItems_offset, r->ReferenceItems_size);
                count = 0; /* force end without failing */
                break;
        }
    }

    acb->synth_depth--;

    return 1;
fail:
    VGM_LOG("ACB: failed Synth %i\n", Index);
    return 0;
}

/*****************************************************************************/

static int load_acb_command_tlvs(acb_header* acb, STREAMFILE* sf, uint32_t Command_offset, uint32_t Command_size) {
    uint16_t tlv_code, tlv_type, tlv_index;
    uint8_t tlv_size;
    uint32_t pos = 0;
    uint32_t max_pos = Command_size;

    if (!read_buffer(acb, Command_offset, Command_size, sf))
        goto fail;

    /* read a (name)Command multiple TLV data */
    while (pos < max_pos) {
        tlv_code = get_u16be(acb->buf + pos + 0x00);
        tlv_size = get_u8   (acb->buf + pos + 0x02);
        pos  += 0x03;

        /* There are around 160 codes (some unused), with things like set volume, pan, stop, mute, and so on.
         * Multiple commands are linked and only "note on" seems to point so other objects, so maybe others
         * apply to current object (since there is "note off" without reference. */
        switch(tlv_code) {
            case 2000: /* noteOn */
            case 2003: /* noteOnWithNo plus 16b (null?) [rare, ex. PES 2014] */
                if (tlv_size < 0x04) {
                    VGM_LOG("ACB: TLV with unknown size\n");
                    break;
                }

                tlv_type = get_u16be(acb->buf + pos + 0x00); /* ReferenceItem */
                tlv_index = get_u16be(acb->buf + pos + 0x02);
                //;VGM_LOG("ACB: TLV at %x: type %x, index=%x\n", offset, tlv_type, tlv_index);

                /* same as Synth's ReferenceItem type? */
                switch(tlv_type) {
                    case 0x02: /* Synth (common) */
                        if (!load_acb_synth(acb, tlv_index))
                            goto fail;
                        break;

                    case 0x03: /* Sequence (common, ex. Yakuza 6, Yakuza Kiwami 2) */
                        if (!load_acb_sequence(acb, tlv_index))
                            goto fail;
                        break;

                    default:
                        VGM_LOG("ACB: unknown TLV type %x at %x + %x\n", tlv_type, Command_offset + pos, tlv_size);
                        max_pos = 0; /* force end without failing */
                        break;
                }
                break;

            case 2004: /* noteOnWithDuration */
                /* same as the above plus extra field */
                //;VGM_LOG("ACB: TLV at %x: usable code %i?\n", offset-0x03, tlv_code);
                break;

            case 33:   /* mute */
            case 124:  /* stopAtLoopEnd */
            case 1000: /* noteOff */
            case 1251: /* sequenceCallbackWithId */
            case 1252: /* sequenceCallbackWithString */
            case 1253: /* sequenceCallbackWithIdAndString */
            case 2002: /* setSynthOrWaveform */
            case 4051: /* transitionTrack */
            case 7102: /* muteTrackAction */
            case 7100: /* startAction */
            case 7101: /* stopAction */
                /* may be needed? */
                //;VGM_LOG("ACB: TLV at %x: check code %i?\n", offset-0x03, tlv_code);
                break;

            case 0:    /* no-op */
            case 998:  /* sequenceStartRandom (plays following note ons in random?)*/
            case 999:  /* sequenceStart (plays following note ons in sequence?) */
            default:   /* ignore others */
                break;
        }

        pos += tlv_size;
    }

    return 1;
fail:
    VGM_LOG("ACB: failed Command TLVs\n");
    return 0;
}

/*****************************************************************************/

static int preload_acb_trackcommand(acb_header* acb) {
    utf_context* Table = acb->TrackCommandTable;
    int* p_rows = &acb->TrackCommand_rows;
    int i, c_Command;


    if (*p_rows)
        return 1;
    /* load either TrackEvent (>=v1.28) or Command () <=v1.27 */
    if (!open_utf_subtable(acb, &acb->TrackCommandSf, &Table, "TrackEventTable", p_rows, ACB_TABLE_BUFFER_TRACKCOMMAND)) {
        if (!open_utf_subtable(acb, &acb->TrackCommandSf, &Table, "CommandTable", p_rows, ACB_TABLE_BUFFER_TRACKCOMMAND))
            goto fail;
    }
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload TrackEvent/Command=%i\n", *p_rows);

    acb->TrackCommand = malloc(*p_rows * sizeof(TrackCommand_t));
    if (!acb->TrackCommand) goto fail;

    c_Command = utf_get_column(Table, "Command");

    for (i = 0; i < *p_rows; i++) {
        TrackCommand_t* r = &acb->TrackCommand[i];

        utf_query_col_data(Table, i, c_Command, &r->Command_offset, &r->Command_size);
        //;VGM_LOG("ACB: TrackEvent/Command[%i]: Command={%x,%x}\n", i, r->Command_offset, r->Command_size);
    }

    ;VGM_LOG("acb: preload TrackEvent/Command done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed TrackEvent/Command preload\n");
    return 0;
}

static int load_acb_trackcommand(acb_header* acb, uint16_t Index) {
    TrackCommand_t* r;
    TrackCommand_t tmp;


    /* read TrackEvent/Command[Index] */
    if (acb->is_preload) {
        if (!preload_acb_trackcommand(acb)) goto fail;
        if (Index > acb->TrackCommand_rows) goto fail;
        r = &acb->TrackCommand[Index];
    }
    else {
        r = &tmp;
        if (acb->has_CommandTable) { /* <=v1.27 */
            if (!open_utf_subtable(acb, &acb->TrackCommandSf, &acb->TrackCommandTable, "CommandTable", NULL, ACB_TABLE_BUFFER_TRACKCOMMAND))
                goto fail;
        }
        else if (acb->has_TrackEventTable) { /* >=v1.28 */
            if (!open_utf_subtable(acb, &acb->TrackCommandSf, &acb->TrackCommandTable, "TrackEventTable", NULL, ACB_TABLE_BUFFER_TRACKCOMMAND))
                goto fail;
        }
        else {
            VGM_LOG("ACB: unknown command table\n");
            goto fail;
        }
        if (!utf_query_data(acb->TrackCommandTable, Index, "Command", &r->Command_offset, &r->Command_size))
            goto fail;
        //;VGM_LOG("ACB: TrackEvent/Command[%i]: Command={%x,%x}\n", Index, r->Command_offset, r->Command_size);
    }


    /* read Command's TLVs */
    if (!load_acb_command_tlvs(acb, acb->TrackCommandSf, r->Command_offset, r->Command_size))
        goto fail;

    return 1;
fail:
    VGM_LOG("ACB: failed TrackCommand %i\n", Index);
    return 0;
}

/*****************************************************************************/

static int preload_acb_track(acb_header* acb) {
    utf_context* Table = acb->TrackTable;
    int* p_rows = &acb->Track_rows;
    int i, c_EventIndex;

    if (*p_rows)
        return 1;
    if (!open_utf_subtable(acb, &acb->TrackSf, &Table, "TrackTable", p_rows, ACB_TABLE_BUFFER_CUE))
        goto fail;
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload Track=%i\n", *p_rows);

    acb->Track = malloc(*p_rows * sizeof(Track_t));
    if (!acb->Track) goto fail;

    c_EventIndex = utf_get_column(Table, "EventIndex");

    for (i = 0; i < *p_rows; i++) {
        Track_t* r = &acb->Track[i];

        if (!utf_query_col_u16(Table, i, c_EventIndex, &r->EventIndex))
            goto fail;
        //;VGM_LOG("ACB: Track[%i]: EventIndex=%i\n", i, r->EventIndex);
    }

    ;VGM_LOG("acb: preload Track done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed Track preload\n");
    return 0;
}

static int load_acb_track(acb_header* acb, uint16_t Index) {
    Track_t* r;
    Track_t tmp;


    /* read Track[Index] */
    if (acb->is_preload) {
        if (!preload_acb_track(acb)) goto fail;
        if (Index > acb->Track_rows) goto fail;
        r = &acb->Track[Index];
    }
    else {
        r = &tmp;
        if (!open_utf_subtable(acb, &acb->TrackSf, &acb->TrackTable, "TrackTable", NULL, ACB_TABLE_BUFFER_TRACK))
            goto fail;
        if (!utf_query_u16(acb->TrackTable, Index, "EventIndex", &r->EventIndex))
            goto fail;
    }

    //todo CommandIndex?

    /* happens with some odd track without anything useful */
    if (r->EventIndex == 65535)
        return 1;

    if (!load_acb_trackcommand(acb, r->EventIndex))
        goto fail;

    return 1;
fail:
    VGM_LOG("ACB: failed Track %i\n", Index);
    return 0;
}

/*****************************************************************************/

static int preload_acb_sequence(acb_header* acb) {
    utf_context* Table = acb->SequenceTable;
    int* p_rows = &acb->Sequence_rows;
    int i, c_NumTracks, c_TrackIndex, c_ActionTrackStartIndex, c_NumActionTracks, c_TrackValues, c_Type;

    if (*p_rows)
        return 1;
    if (!open_utf_subtable(acb, &acb->SequenceSf, &Table, "SequenceTable", p_rows, ACB_TABLE_BUFFER_SEQUENCE))
        goto fail;
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload Sequence=%i\n", *p_rows);

    acb->Sequence = malloc(*p_rows * sizeof(Sequence_t));
    if (!acb->Sequence) goto fail;

    c_NumTracks = utf_get_column(Table, "NumTracks");
    c_TrackIndex = utf_get_column(Table, "TrackIndex");
    c_ActionTrackStartIndex = utf_get_column(Table, "ActionTrackStartIndex");
    c_NumActionTracks = utf_get_column(Table, "NumActionTracks");
    c_TrackValues = utf_get_column(Table, "TrackValues");
    c_Type = utf_get_column(Table, "Type");

    for (i = 0; i < *p_rows; i++) {
        Sequence_t* r = &acb->Sequence[i];

        utf_query_col_u16(Table, i, c_NumTracks, &r->NumTracks);
        utf_query_col_data(Table, i, c_TrackIndex, &r->TrackIndex_offset, &r->TrackIndex_size);
        utf_query_col_u16(Table, i, c_ActionTrackStartIndex, &r->ActionTrackStartIndex);
        utf_query_col_u16(Table, i, c_NumActionTracks, &r->NumActionTracks);
        utf_query_col_data(Table, i, c_TrackValues, &r->TrackValues_offset, &r->TrackValues_size);
        utf_query_col_u8(Table, i, c_Type, &r->Type);
        //;VGM_LOG("ACB: Sequence[%i]: NumTracks=%i, TrackIndex={%x, %x}, TrackIndex={%x, %x}, Type=%x\n", i, r->NumTracks, r->TrackIndex_offset, r->TrackIndex_size, r->TrackValues_offset, r->TrackValues_size, r->Type);
    }

    ;VGM_LOG("acb: preload Sequence done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed Sequence preload\n");
    return 0;
}

static int load_acb_sequence(acb_header* acb, uint16_t Index) {
    int i;
    Sequence_t* r;
    Sequence_t tmp;


    /* read Sequence[Index] */
    if (acb->is_preload) {
        if (!preload_acb_sequence(acb)) goto fail;
        if (Index > acb->Sequence_rows) goto fail;
        r = &acb->Sequence[Index];
    }
    else {
        r = &tmp;
        if (!open_utf_subtable(acb, &acb->SequenceSf, &acb->SequenceTable, "SequenceTable", NULL, ACB_TABLE_BUFFER_SEQUENCE))
            goto fail;

        if (!utf_query_u16(acb->SequenceTable, Index, "NumTracks", &r->NumTracks))
            goto fail;
        if (!utf_query_data(acb->SequenceTable, Index, "TrackIndex", &r->TrackIndex_offset, &r->TrackIndex_size))
            goto fail;
    }

    //todo .CommandIndex > SequenceCommand?
    // most unknown types can be found in Ultra Despair Girls (PC)

    acb->sequence_depth++;

    if (acb->sequence_depth > 3) {
        VGM_LOG("ACB: Sequence depth too high\n");
        goto fail; /* max Sequence > Sequence > Sequence > Synth > Waveform (ex. Yakuza 6) */
    }


    /* read Tracks inside Sequence */
    if (r->NumActionTracks) {
        VGM_LOG_ONCE("ACB: ignored ActionTrack[%i~%i]\n", r->ActionTrackStartIndex, r->NumActionTracks);
    }
    else {
        if (r->NumTracks * 0x02 > r->TrackIndex_size) { /* padding may exist */
            VGM_LOG("ACB: wrong Sequence.TrackIndex size\n");
            goto fail;
        }

        switch(r->Type) {
            case 0: /* common */
                if (!read_buffer(acb, r->TrackIndex_offset, r->TrackIndex_size, acb->SequenceSf))
                    goto fail;

                for (i = 0; i < r->NumTracks; i++) {
                    int16_t TrackIndex_index = get_s16be(acb->buf + i*0x02);

                    if (!load_acb_track(acb, TrackIndex_index))
                        goto fail;
                }
                break;

            case 1: /* TrackIndex only, similar to 0? (rare) */
                VGM_LOG_ONCE("ACB: unknown Sequence.Type=%x\n", r->Type);
                break;
            case 3: /* TrackIndex + TrackValues */
            case 4: /* same */
            default:
                VGM_LOG_ONCE("ACB: unknown Sequence.Type=%x\n", r->Type);
                break;
        }
    }

    acb->sequence_depth--;

    return 1;
fail:
    VGM_LOG("ACB: failed Sequence %i\n", Index);
    return 0;
}

/*****************************************************************************/

static int preload_acb_block(acb_header* acb) {
    utf_context* Table = acb->BlockTable;
    int* p_rows = &acb->Block_rows;
    int i, c_NumTracks, c_TrackIndex;

    if (*p_rows)
        return 1;
    if (!open_utf_subtable(acb, &acb->BlockSf, &Table, "BlockTable", p_rows, ACB_TABLE_BUFFER_BLOCK))
        goto fail;
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload Block=%i\n", *p_rows);

    acb->Block = malloc(*p_rows * sizeof(Block_t));
    if (!acb->Block) goto fail;

    c_NumTracks = utf_get_column(Table, "NumTracks");
    c_TrackIndex = utf_get_column(Table, "TrackIndex");

    for (i = 0; i < *p_rows; i++) {
        Block_t* r = &acb->Block[i];

        utf_query_col_u16(Table, i, c_NumTracks, &r->NumTracks);
        utf_query_col_data(Table, i, c_TrackIndex, &r->TrackIndex_offset, &r->TrackIndex_size);
        //;VGM_LOG("ACB: Block[%i]: NumTracks=%i, TrackIndex={%x, %x}\n", i, r->NumTracks, r->TrackIndex_offset, r->TrackIndex_size);
    }

    ;VGM_LOG("acb: preload Block done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed Block preload\n");
    return 0;
}

static int load_acb_block(acb_header* acb, uint16_t Index) {
    int i;
    Block_t* r;
    Block_t tmp;


    /* read Block[Index] */
    if (acb->is_preload) {
        if (!preload_acb_block(acb)) goto fail;
        if (Index > acb->Block_rows) goto fail;
        r = &acb->Block[Index];
    }
    else {
        r = &tmp;
        if (!open_utf_subtable(acb, &acb->BlockSf, &acb->BlockTable, "BlockTable", NULL, ACB_TABLE_BUFFER_BLOCK))
            goto fail;

        if (!utf_query_u16(acb->BlockTable, Index, "NumTracks", &r->NumTracks))
            goto fail;
        if (!utf_query_data(acb->BlockTable, Index, "TrackIndex", &r->TrackIndex_offset, &r->TrackIndex_size))
            goto fail;
    }

    if (r->NumTracks * 0x02 > r->TrackIndex_size) { /* padding may exist */
        VGM_LOG("ACB: wrong Block.TrackIndex size\n");
        goto fail;
    }

    //todo .ActionTrackStartIndex/NumActionTracks > ?

    if (!read_buffer(acb, r->TrackIndex_offset, r->TrackIndex_size, acb->BlockSf))
        goto fail;

    /* read Tracks inside Block */
    for (i = 0; i < r->NumTracks; i++) {
        int16_t TrackIndex_index = get_s16be(acb->buf + i*0x02);

        if (!load_acb_track(acb, TrackIndex_index))
            goto fail;
    }

    return 1;
fail:
    VGM_LOG("ACB: failed Block %i\n", Index);
    return 0;
}

/*****************************************************************************/

static int preload_acb_blocksequence(acb_header* acb) {
    utf_context* Table = acb->BlockSequenceTable;
    int* p_rows = &acb->BlockSequence_rows;
    int i, c_NumTracks, c_TrackIndex, c_NumBlocks, c_BlockIndex;

    if (*p_rows)
        return 1;
    if (!open_utf_subtable(acb, &acb->BlockSequenceSf, &Table, "BlockSequenceTable", p_rows, ACB_TABLE_BUFFER_BLOCKSEQUENCE))
        goto fail;
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload BlockSequence=%i\n", *p_rows);

    acb->BlockSequence = malloc(*p_rows * sizeof(BlockSequence_t));
    if (!acb->BlockSequence) goto fail;

    c_NumTracks = utf_get_column(Table, "NumTracks");
    c_TrackIndex = utf_get_column(Table, "TrackIndex");
    c_NumBlocks = utf_get_column(Table, "NumBlocks");
    c_BlockIndex = utf_get_column(Table, "BlockIndex");

    for (i = 0; i < *p_rows; i++) {
        BlockSequence_t* r = &acb->BlockSequence[i];

        utf_query_col_u16(Table, i, c_NumTracks, &r->NumTracks);
        utf_query_col_data(Table, i, c_TrackIndex, &r->TrackIndex_offset, &r->TrackIndex_size);
        utf_query_col_u16(Table, i, c_NumBlocks, &r->NumBlocks);
        utf_query_col_data(Table, i, c_BlockIndex, &r->BlockIndex_offset, &r->BlockIndex_size);
    }

    ;VGM_LOG("acb: preload BlockSequence done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed BlockSequence preload\n");
    return 0;
}

static int load_acb_blocksequence(acb_header* acb, uint16_t Index) {
    int i;
    BlockSequence_t* r;
    BlockSequence_t tmp;


    /* read BlockSequence[Index] */
    if (acb->is_preload) {
        if (!preload_acb_blocksequence(acb)) goto fail;
        if (Index > acb->BlockSequence_rows) goto fail;
        r = &acb->BlockSequence[Index];
    }
    else {
        r = &tmp;
        if (!open_utf_subtable(acb, &acb->BlockSequenceSf, &acb->BlockSequenceTable, "BlockSequenceTable", NULL, ACB_TABLE_BUFFER_BLOCKSEQUENCE))
            goto fail;

        if (!utf_query_u16(acb->BlockSequenceTable, Index, "NumTracks", &r->NumTracks))
            goto fail;
        if (!utf_query_data(acb->BlockSequenceTable, Index, "TrackIndex", &r->TrackIndex_offset, &r->TrackIndex_size))
            goto fail;
        if (!utf_query_u16(acb->BlockSequenceTable, Index, "NumBlocks", &r->NumBlocks))
            goto fail;
        if (!utf_query_data(acb->BlockSequenceTable, Index, "BlockIndex", &r->BlockIndex_offset, &r->BlockIndex_size))
            goto fail;
        //;VGM_LOG("ACB: BlockSequence[%i]: NumTracks=%i, TrackIndex={%x, %x}, NumBlocks=%i, BlockIndex={%x, %x}\n", Index, r->NumTracks, r->TrackIndex_offset,TrackIndex_size, r->NumBlocks, r->BlockIndex_offset, r->BlockIndex_size);
    }


    if (r->NumTracks * 0x02 > r->TrackIndex_size) { /* padding may exist */
        VGM_LOG("ACB: wrong BlockSequence.TrackIndex size\n");
        goto fail;
    }

    if (!read_buffer(acb, r->TrackIndex_offset, r->TrackIndex_size, acb->BlockSequenceSf))
        goto fail;

    /* read Tracks inside BlockSequence */
    for (i = 0; i < r->NumTracks; i++) {
        int16_t TrackIndex_index = get_s16be(acb->buf + i*0x02);

        if (!load_acb_track(acb, TrackIndex_index))
            goto fail;
    }

    if (r->NumBlocks * 0x02 > r->BlockIndex_size) {
        VGM_LOG("ACB: wrong BlockSequence.BlockIndex size\n");
        goto fail;
    }

    if (!read_buffer(acb, r->BlockIndex_offset, r->BlockIndex_size, acb->BlockSequenceSf))
        goto fail;

    /* read Blocks inside BlockSequence */
    for (i = 0; i < r->NumBlocks; i++) {
        int16_t BlockIndex_index = get_s16be(acb->buf + i*0x02);

        if (!load_acb_block(acb, BlockIndex_index))
            goto fail;
    }

    return 1;
fail:
    VGM_LOG("ACB: failed BlockSequence %i\n", Index);
    return 0;
}

/*****************************************************************************/

static int preload_acb_cue(acb_header* acb) {
    utf_context* Table = acb->CueTable;
    int* p_rows = &acb->Cue_rows;
    int i, c_ReferenceType, c_ReferenceIndex;

    if (*p_rows)
        return 1;
    if (!open_utf_subtable(acb, &acb->CueSf, &Table, "CueTable", p_rows, ACB_TABLE_BUFFER_CUE))
        goto fail;
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload Cue=%i\n", *p_rows);

    acb->Cue = malloc(*p_rows * sizeof(Cue_t));
    if (!acb->Cue) goto fail;

    c_ReferenceType = utf_get_column(Table, "ReferenceType");
    c_ReferenceIndex = utf_get_column(Table, "ReferenceIndex");

    for (i = 0; i < *p_rows; i++) {
        Cue_t* r = &acb->Cue[i];

        utf_query_col_u8(Table, i, c_ReferenceType, &r->ReferenceType);
        utf_query_col_u16(Table, i, c_ReferenceIndex, &r->ReferenceIndex);
        //;VGM_LOG("ACB: Cue[%i]: ReferenceType=%i, ReferenceIndex=%i\n", i, r->ReferenceType, r->ReferenceIndex);
    }

    ;VGM_LOG("acb: preload Cue done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed Cue preload\n");
    return 0;
}

static int load_acb_cue(acb_header* acb, uint16_t Index) {
    Cue_t* r;
    Cue_t tmp;

    /* read Cue[Index] */
    if (acb->is_preload) {
        if (!preload_acb_cue(acb)) goto fail;
        if (Index > acb->Cue_rows) goto fail;
        r = &acb->Cue[Index];
    }
    else {
        if (!open_utf_subtable(acb, &acb->CueSf, &acb->CueTable, "CueTable", NULL, ACB_TABLE_BUFFER_CUE))
            goto fail;
        r = &tmp;
        if (!utf_query_u8(acb->CueTable, Index, "ReferenceType", &r->ReferenceType))
            goto fail;
        if (!utf_query_u16(acb->CueTable, Index, "ReferenceIndex", &r->ReferenceIndex))
            goto fail;
    }


    /* usually older games use older references but not necessarily */
    switch(r->ReferenceType) {

        case 0x01: /* Cue > Waveform (ex. PES 2015) */
            if (!load_acb_waveform(acb, r->ReferenceIndex))
                goto fail;
            break;

        case 0x02: /* Cue > Synth > Waveform (ex. Ukiyo no Roushi) */
            if (!load_acb_synth(acb, r->ReferenceIndex))
                goto fail;
            break;

        case 0x03: /* Cue > Sequence > Track > Command > Synth > Waveform (ex. Valkyrie Profile anatomia, Yakuza Kiwami 2) */
            if (!load_acb_sequence(acb, r->ReferenceIndex))
                goto fail;
            break;

        case 0x08: /* Cue > BlockSequence > Track / Block > Track > Command > Synth > Waveform (ex. Sonic Lost World, Kandagawa Jet Girls, rare) */
            if (!load_acb_blocksequence(acb, r->ReferenceIndex))
                goto fail;
            break;

        case 0x00: /* none */
        case 0x04: /* "track" */
        case 0x05: /* "outsideLink" */
        case 0x06: /* "insideLinkSynth" (ex. PES 2014) */
        case 0x07: /* "insideLinkSequence" (ex. PES 2014) */
        case 0x09: /* "insideLinkBlockSequence" */
        case 0x0a: /* "eventCue_UnUse" */
        case 0x0b: /* "soundGenerator" */
        default:
            VGM_LOG_ONCE("ACB: unknown Cue.ReferenceType=%x, Cue.ReferenceIndex=%x\n", r->ReferenceType, r->ReferenceIndex);
            break; /* ignore and continue */
    }


    return 1;
fail:
    VGM_LOG("ACB: failed Cue %i\n", Index);
    return 0;
}

/*****************************************************************************/

static int preload_acb_cuename(acb_header* acb) {
    utf_context* Table = acb->CueNameTable;
    int* p_rows = &acb->CueName_rows;
    int i, c_CueIndex, c_CueName;


    if (*p_rows) 
        return 1;
    if (!open_utf_subtable(acb, &acb->CueNameSf, &Table, "CueNameTable", p_rows, ACB_TABLE_BUFFER_CUENAME))
        goto fail;
    if (!*p_rows)
        return 1;
    ;VGM_LOG("acb: preload CueName=%i\n", *p_rows);

    acb->CueName = malloc(*p_rows * sizeof(CueName_t));
    if (!acb->CueName) goto fail;

    c_CueIndex = utf_get_column(Table, "CueIndex");
    c_CueName = utf_get_column(Table, "CueName");

    for (i = 0; i < *p_rows; i++) {
        CueName_t* r = &acb->CueName[i];

        utf_query_col_u16(Table, i, c_CueIndex, &r->CueIndex);
        utf_query_col_string(Table, i, c_CueName, &r->CueName);
        //;VGM_LOG("ACB: CueName[%i]: CueIndex=%i, CueName=%s\n", i, r->CueIndex, r->CueName);
    }

    ;VGM_LOG("acb: preload CueName done\n");
    return 1;
fail:
    VGM_LOG("ACB: failed CueName preload\n");
    return 0;
}

static int load_acb_cuename(acb_header* acb, uint16_t Index) {
    CueName_t* r;
    CueName_t tmp;

    /* read CueName[Index] */
    if (acb->is_preload) {
        if (!preload_acb_cuename(acb)) goto fail;
        if (Index > acb->CueName_rows) goto fail;
        r = &acb->CueName[Index];
    }
    else {
        if (!open_utf_subtable(acb, &acb->CueNameSf, &acb->CueNameTable, "CueNameTable", NULL, ACB_TABLE_BUFFER_CUENAME))
            goto fail;
        r = &tmp;
        if (!utf_query_u16(acb->CueNameTable, Index, "CueIndex", &r->CueIndex))
            goto fail;
        if (!utf_query_string(acb->CueNameTable, Index, "CueName", &r->CueName))
            goto fail;
    }


    /* save as will be needed if references waveform */
    acb->cuename_index = Index;
    acb->cuename_name = r->CueName;

    if (!load_acb_cue(acb, r->CueIndex))
        goto fail;

    return 1;
fail:
    VGM_LOG("ACB: failed CueName %i\n", Index);
    return 0;
}

/*****************************************************************************/

void load_acb_wave_name(STREAMFILE* sf, VGMSTREAM* vgmstream, int waveid, int port, int is_memory) {
    acb_header acb = {0};
    int i, CueName_rows;


    if (!sf || !vgmstream || waveid < 0)
        return;

    /* Normally games load a .acb + .awb, and asks the .acb to play a cue by name or index.
     * Since we only care for actual waves, to get its name we need to find which cue uses our wave.
     * Multiple cues can use the same wave (meaning multiple names), and one cue may use multiple waves.
     * There is no easy way to map cue name <> wave name so basically we parse the whole thing.
     *
     * .acb are created in CRI Atom Craft, where user defines N Cues with CueName each, then link somehow
     * to a Waveform (.awb=streamed or memory .acb=internal, data 'material' encoded in some format),
     * depending on reference types. Typical links are:
     * - CueName > Cue > Waveform (type 1)
     * - CueName > Cue > Synth > Waveform (type 2)
     * - CueName > Cue > Sequence > Track > Command > Synth > Waveform (type 3, <=v1.27)
     * - CueName > Cue > Sequence > Track > Command > Synth > Synth > Waveform (type 3, <=v1.27)
     * - CueName > Cue > Sequence > Track > TrackEvent > Command > Synth > Waveform (type 3, >=v1.28)
     * - CueName > Cue > Sequence > Track > TrackEvent > Command > Synth > Synth > Waveform (type 3, >=v1.28)
     * - CueName > Cue > Sequence > Track > TrackEvent > Command > Sequence > (...) > Synth > Waveform (type 3, >=v1.28)
     * - CueName > Cue > Block > Track > Command > Synth > Synth > Waveform (type 8)
     * - others should be possible
     * Atom Craft may only target certain .acb versions so some links are later removed
     * Not all cues to point to Waveforms, some are just config events/commands.
     * .acb link to .awb by name (loaded manually), though they have a checksum/hash/header to validate.
     *
     * .acb can contain info for multiple .awb, that are loaded sequentially and assigned "port numbers" (0 to N).
     * Both Wave ID and port number must be passed externally to find appropriate song name.
     * 
     * To improve performance we pre-read each table objects's useful fields. Extra complex files may include +8000 objects,
     * per table, meaning it uses a decent chunk of memory, but  having to re-read with streamfiles is much slower.
     */

    //;VGM_LOG("ACB: find waveid=%i, port=%i\n", waveid, port);

    acb.acbFile = sf;

    acb.Header = utf_open(acb.acbFile, 0x00, NULL, NULL);
    if (!acb.Header) goto fail;

    acb.target_waveid = waveid;
    acb.target_port = port;
    acb.is_memory = is_memory;


#ifdef ACB_PRELOAD
    acb.is_preload = 1;
#else
    acb.has_TrackEventTable = utf_query_data(acb.Header, 0, "TrackEventTable", NULL,NULL);
    acb.has_CommandTable = utf_query_data(acb.Header, 0, "CommandTable", NULL,NULL);
#endif

    //todo preload cuename table
    /* read all possible cue names and find which waveids are referenced by it */
    if (!open_utf_subtable(&acb, &acb.TempSf, &acb.TempTable, "CueNameTable", &CueName_rows, ACB_TABLE_BUFFER_CUENAME))
        goto fail;
    for (i = 0; i < CueName_rows; i++) {
        if (!load_acb_cuename(&acb, i))
            goto fail;
    }

    /* meh copy */
    if (acb.awbname_count > 0) {
        strncpy(vgmstream->stream_name, acb.name, STREAM_NAME_SIZE);
        vgmstream->stream_name[STREAM_NAME_SIZE - 1] = '\0';
    }

fail:
    utf_close(acb.Header);

    utf_close(acb.TempTable);
    close_streamfile(acb.TempSf);

    utf_close(acb.CueNameTable);
    utf_close(acb.CueTable);
    utf_close(acb.BlockSequenceTable);
    utf_close(acb.BlockTable);
    utf_close(acb.SequenceTable);
    utf_close(acb.TrackTable);
    utf_close(acb.TrackCommandTable);
    utf_close(acb.SynthTable);
    utf_close(acb.WaveformTable);

    close_streamfile(acb.CueNameSf);
    close_streamfile(acb.CueSf);
    close_streamfile(acb.BlockSequenceSf);
    close_streamfile(acb.BlockSf);
    close_streamfile(acb.SequenceSf);
    close_streamfile(acb.TrackSf);
    close_streamfile(acb.TrackCommandSf);
    close_streamfile(acb.SynthSf);
    close_streamfile(acb.WaveformSf);

    free(acb.buf);

    free(acb.CueName);
    free(acb.Cue);
    free(acb.BlockSequence);
    free(acb.Block);
    free(acb.Sequence);
    free(acb.Track);
    free(acb.TrackCommand);
    free(acb.Synth);
    free(acb.Waveform);
}

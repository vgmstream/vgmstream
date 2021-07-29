#include "meta.h"
#include "../coding/coding.h"
#include "cri_utf.h"


/* ACB (Atom Cue sheet Binary) - CRI container of memory audio, often together with a .awb wave bank */
VGMSTREAM* init_vgmstream_acb(STREAMFILE* sf) {
    VGMSTREAM* vgmstream = NULL;
    STREAMFILE* temp_sf = NULL;
    off_t subfile_offset;
    size_t subfile_size;
    utf_context *utf = NULL;


    /* checks */
    if (!check_extensions(sf, "acb"))
        goto fail;
    if (read_u32be(0x00,sf) != 0x40555446) /* "@UTF" */
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
        if (subfile_size == 0)
            goto fail;
    }

    //;VGM_LOG("ACB: subfile offset=%lx + %x\n", subfile_offset, subfile_size);

    temp_sf = setup_subfile_streamfile(sf, subfile_offset,subfile_size, "awb");
    if (!temp_sf) goto fail;

    if (read_u32be(0x00, temp_sf) == 0x43504B20) { /* "CPK " */
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
#define ACB_TABLE_BUFFER_CUENAME 0x8000
#define ACB_TABLE_BUFFER_CUE 0x40000
#define ACB_TABLE_BUFFER_BLOCKSEQUENCE 0x8000
#define ACB_TABLE_BUFFER_BLOCK 0x8000
#define ACB_TABLE_BUFFER_SEQUENCE 0x40000
#define ACB_TABLE_BUFFER_TRACK 0x10000
#define ACB_TABLE_BUFFER_TRACKCOMMAND 0x20000
#define ACB_TABLE_BUFFER_SYNTH 0x40000
#define ACB_TABLE_BUFFER_WAVEFORM 0x20000

#define ACB_MAX_NAMELIST 255
#define ACB_MAX_NAME 1024 /* even more is possible in rare cases [Senran Kagura Burst Re:Newal (PC)] */


static STREAMFILE* setup_acb_streamfile(STREAMFILE* sf, size_t buffer_size) {
    STREAMFILE* new_sf = NULL;

    /* buffer seems better than reopening when opening multiple subsongs at the same time with STDIO,
     * even though there is more buffer trashing, maybe concurrent IO is slower */
    new_sf = open_wrap_streamfile(sf);
    new_sf = open_buffer_streamfile_f(new_sf, buffer_size);
    //new_sf = reopen_streamfile(sf, buffer_size);
    return new_sf;
}


typedef struct {
    STREAMFILE* acbFile; /* original reference, don't close */

    /* keep track of these tables so they can be closed when done */
    utf_context *Header;

    utf_context *CueNameTable;
    utf_context *CueTable;
    utf_context *BlockSequenceTable;
    utf_context *BlockTable;
    utf_context *SequenceTable;
    utf_context *TrackTable;
    utf_context *TrackCommandTable;
    utf_context *SynthTable;
    utf_context *WaveformTable;

    STREAMFILE* CueNameSf;
    STREAMFILE* CueSf;
    STREAMFILE* BlockSequenceSf;
    STREAMFILE* BlockSf;
    STREAMFILE* SequenceSf;
    STREAMFILE* TrackSf;
    STREAMFILE* TrackCommandSf;
    STREAMFILE* SynthSf;
    STREAMFILE* WaveformSf;

    /* config */
    int is_memory;
    int target_waveid;
    int target_port;
    int has_TrackEventTable;
    int has_CommandTable;

    /* to avoid infinite/circular references (AtomViewer crashes otherwise) */
    int synth_depth;
    int sequence_depth;

    /* name stuff */
    int16_t cuename_index;
    const char * cuename_name;
    int awbname_count;
    int16_t awbname_list[ACB_MAX_NAMELIST];
    char name[ACB_MAX_NAME];

} acb_header;

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


/*******************************************************************************/
/* OBJECT HANDLERS */

static int load_acb_waveform(acb_header* acb, int16_t Index) {
    uint16_t Id, PortNo;
    uint8_t Streaming;

    /* read Waveform[Index] */
    if (!open_utf_subtable(acb, &acb->WaveformSf, &acb->WaveformTable, "WaveformTable", NULL, ACB_TABLE_BUFFER_WAVEFORM))
        goto fail;
    if (!utf_query_u16(acb->WaveformTable, Index, "Id", &Id)) { /* older versions use Id */
        if (acb->is_memory) {
            if (!utf_query_u16(acb->WaveformTable, Index, "MemoryAwbId", &Id))
                goto fail;
            PortNo = 0xFFFF;
        } else {
            if (!utf_query_u16(acb->WaveformTable, Index, "StreamAwbId", &Id))
                goto fail;
            if (!utf_query_u16(acb->WaveformTable, Index, "StreamAwbPortNo", &PortNo))
                PortNo = 0; /* assumed */
        }
    }
    else {
        PortNo = 0xFFFF;
    }

    if (!utf_query_u8(acb->WaveformTable, Index, "Streaming", &Streaming))
        goto fail;
    //;VGM_LOG("ACB: Waveform[%i]: Id=%i, PortNo=%i, Streaming=%i\n", Index, Id, PortNo, Streaming);

    /* not found but valid */
    if (Id != acb->target_waveid)
        return 1;

    /* correct AWB port (check ignored if set to -1) */
    if (acb->target_port >= 0 && PortNo != 0xFFFF && PortNo != acb->target_port)
        return 1;

    /* must match our target's (0=memory, 1=streaming, 2=memory (prefetch)+stream) */
    if ((acb->is_memory && Streaming == 1) || (!acb->is_memory && Streaming == 0))
        return 1;

    /* aaand finally get name (phew) */
    add_acb_name(acb, Streaming);

    return 1;
fail:
    return 0;
}

/* define here for Synths pointing to Sequences */
static int load_acb_sequence(acb_header* acb, int16_t Index);

static int load_acb_synth(acb_header* acb, int16_t Index) {
    int i, count;
    uint8_t Type;
    uint32_t ReferenceItems_offset, ReferenceItems_size;


    /* read Synth[Index] */
    if (!open_utf_subtable(acb, &acb->SynthSf, &acb->SynthTable, "SynthTable", NULL, ACB_TABLE_BUFFER_SYNTH))
        goto fail;
    if (!utf_query_u8(acb->SynthTable, Index, "Type", &Type))
        goto fail;
    if (!utf_query_data(acb->SynthTable, Index, "ReferenceItems", &ReferenceItems_offset, &ReferenceItems_size))
        goto fail;
    //;VGM_LOG("ACB: Synth[%i]: Type=%x, ReferenceItems={%x,%x}\n", Index, Type, ReferenceItems_offset, ReferenceItems_size);

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

    count = ReferenceItems_size / 0x04;
    for (i = 0; i < count; i++) {
        uint16_t item_type  = read_u16be(ReferenceItems_offset + i*0x04 + 0x00, acb->SynthSf);
        uint16_t item_index = read_u16be(ReferenceItems_offset + i*0x04 + 0x02, acb->SynthSf);
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
                VGM_LOG("ACB: unknown Synth.ReferenceItem type %x at %x + %x\n", item_type, ReferenceItems_offset, ReferenceItems_size);
                count = 0; /* force end without failing */
                break;
        }
    }

    acb->synth_depth--;

    return 1;
fail:
    return 0;
}


static int load_acb_command_tlvs(acb_header* acb, STREAMFILE* sf, uint32_t Command_offset, uint32_t Command_size) {
    uint32_t offset = Command_offset;
    uint32_t max_offset = Command_offset + Command_size;
    uint16_t tlv_code, tlv_type, tlv_index;
    uint8_t tlv_size;

    //todo read full offsets

    /* read a (name)Command multiple TLV data */
    while (offset < max_offset) {
        tlv_code = read_u16be(offset + 0x00, sf);
        tlv_size = read_u8   (offset + 0x02, sf);
        offset += 0x03;

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

                tlv_type = read_u16be(offset + 0x00, sf); /* ReferenceItem */
                tlv_index = read_u16be(offset + 0x02, sf);
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
                        VGM_LOG("ACB: unknown TLV type %x at %x + %x\n", tlv_type, offset, tlv_size);
                        max_offset = 0; /* force end without failing */
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

        offset += tlv_size;
    }

    return 1;
fail:
    return 0;
}

static int load_acb_track_event_command(acb_header* acb, int16_t Index) {
    uint16_t EventIndex;
    uint32_t Command_offset, Command_size;


    /* read Track[Index] */
    if (!open_utf_subtable(acb, &acb->TrackSf, &acb->TrackTable, "TrackTable", NULL, ACB_TABLE_BUFFER_TRACK ))
        goto fail;
    if (!utf_query_u16(acb->TrackTable, Index, "EventIndex", &EventIndex))
        goto fail;
    //;VGM_LOG("ACB: Track[%i]: EventIndex=%i\n", Index, EventIndex);

    //todo CommandIndex?

    /* happens with some odd track without anything useful */
    if (EventIndex == 65535)
        return 1;

    /* next link varies with version, check by table existence */
    if (acb->has_CommandTable) { /* <=v1.27 */
        /* read Command[EventIndex] */
        if (!open_utf_subtable(acb, &acb->TrackCommandSf, &acb->TrackCommandTable, "CommandTable", NULL, ACB_TABLE_BUFFER_TRACKCOMMAND))
            goto fail;
        if (!utf_query_data(acb->TrackCommandTable, EventIndex, "Command", &Command_offset, &Command_size))
            goto fail;
        //;VGM_LOG("ACB: Command[%i]: Command={%x,%x}\n", EventIndex, Command_offset,Command_size);
    }
    else if (acb->has_TrackEventTable) { /* >=v1.28 */
        /* read TrackEvent[EventIndex] */
        if (!open_utf_subtable(acb, &acb->TrackCommandSf, &acb->TrackCommandTable, "TrackEventTable", NULL, ACB_TABLE_BUFFER_TRACKCOMMAND))
            goto fail;
        if (!utf_query_data(acb->TrackCommandTable, EventIndex, "Command", &Command_offset, &Command_size))
            goto fail;
        //;VGM_LOG("ACB: TrackEvent[%i]: Command={%x,%x}\n", EventIndex, Command_offset,Command_size);
    }
    else {
        VGM_LOG("ACB: unknown command table\n");
        goto fail;
    }

    /* read Command's TLVs */
    if (!load_acb_command_tlvs(acb, acb->TrackCommandSf, Command_offset, Command_size))
        goto fail;

    return 1;
fail:
    return 0;
}

static int load_acb_sequence(acb_header* acb, int16_t Index) {
    int i;
    uint16_t NumTracks;
    uint32_t TrackIndex_offset, TrackIndex_size;


    /* read Sequence[Index] */
    if (!open_utf_subtable(acb, &acb->SequenceSf, &acb->SequenceTable, "SequenceTable", NULL, ACB_TABLE_BUFFER_SEQUENCE))
        goto fail;
    if (!utf_query_u16(acb->SequenceTable, Index, "NumTracks", &NumTracks))
        goto fail;
    if (!utf_query_data(acb->SequenceTable, Index, "TrackIndex", &TrackIndex_offset, &TrackIndex_size))
        goto fail;
    //;VGM_LOG("ACB: Sequence[%i]: NumTracks=%i, TrackIndex={%x, %x}\n", Index, NumTracks, TrackIndex_offset,TrackIndex_size);

    //todo .CommandIndex > SequenceCommand?

    acb->sequence_depth++;

    if (acb->sequence_depth > 3) {
        VGM_LOG("ACB: Sequence depth too high\n");
        goto fail; /* max Sequence > Sequence > Sequence > Synth > Waveform (ex. Yakuza 6) */
    }

    if (NumTracks * 0x02 > TrackIndex_size) { /* padding may exist */
        VGM_LOG("ACB: wrong Sequence.TrackIndex size\n");
        goto fail;
    }

    /* read Tracks inside Sequence */
    for (i = 0; i < NumTracks; i++) {
        int16_t TrackIndex_index = read_s16be(TrackIndex_offset + i*0x02, acb->SequenceSf);

        if (!load_acb_track_event_command(acb, TrackIndex_index))
            goto fail;
    }

    acb->sequence_depth--;

    return 1;
fail:
    return 0;
}

static int load_acb_block(acb_header* acb, int16_t Index) {
    int i;
    uint16_t NumTracks;
    uint32_t TrackIndex_offset, TrackIndex_size;


    /* read Block[Index] */
    if (!open_utf_subtable(acb, &acb->BlockSf, &acb->BlockTable, "BlockTable", NULL, ACB_TABLE_BUFFER_BLOCK))
        goto fail;
    if (!utf_query_u16(acb->BlockTable, Index, "NumTracks", &NumTracks))
        goto fail;
    if (!utf_query_data(acb->BlockTable, Index, "TrackIndex", &TrackIndex_offset, &TrackIndex_size))
        goto fail;
    //;VGM_LOG("ACB: Block[%i]: NumTracks=%i, TrackIndex={%x, %x}\n", Index, NumTracks, TrackIndex_offset,TrackIndex_size);

    if (NumTracks * 0x02 > TrackIndex_size) { /* padding may exist */
        VGM_LOG("ACB: wrong Block.TrackIndex size\n");
        goto fail;
    }

    //todo .ActionTrackStartIndex/NumActionTracks > ?

    /* read Tracks inside Block */
    for (i = 0; i < NumTracks; i++) {
        int16_t TrackIndex_index = read_s16be(TrackIndex_offset + i*0x02, acb->BlockSf);

        if (!load_acb_track_event_command(acb, TrackIndex_index))
            goto fail;
    }

    return 1;
fail:
    return 0;
}

static int load_acb_blocksequence(acb_header* acb, int16_t Index) {
    int i;

    uint16_t NumTracks;
    uint32_t TrackIndex_offset, TrackIndex_size;
    uint16_t NumBlocks;
    uint32_t BlockIndex_offset, BlockIndex_size;


    /* read BlockSequence[Index] */
    if (!open_utf_subtable(acb, &acb->BlockSequenceSf, &acb->BlockSequenceTable, "BlockSequenceTable", NULL, ACB_TABLE_BUFFER_BLOCKSEQUENCE))
        goto fail;

    if (!utf_query_u16(acb->BlockSequenceTable, Index, "NumTracks", &NumTracks))
        goto fail;
    if (!utf_query_data(acb->BlockSequenceTable, Index, "TrackIndex", &TrackIndex_offset, &TrackIndex_size))
        goto fail;
    if (!utf_query_u16(acb->BlockSequenceTable, Index, "NumBlocks", &NumBlocks))
        goto fail;
    if (!utf_query_data(acb->BlockSequenceTable, Index, "BlockIndex", &BlockIndex_offset, &BlockIndex_size))
        goto fail;
    //;VGM_LOG("ACB: BlockSequence[%i]: NumTracks=%i, TrackIndex={%x, %x}, NumBlocks=%i, BlockIndex={%x, %x}\n", Index, NumTracks, TrackIndex_offset,TrackIndex_size, NumBlocks, BlockIndex_offset,BlockIndex_size);


    if (NumTracks * 0x02 > TrackIndex_size) { /* padding may exist */
        VGM_LOG("ACB: wrong BlockSequence.TrackIndex size\n");
        goto fail;
    }

    /* read Tracks inside BlockSequence */
    for (i = 0; i < NumTracks; i++) {
        int16_t TrackIndex_index = read_s16be(TrackIndex_offset + i*0x02, acb->BlockSequenceSf);

        if (!load_acb_track_event_command(acb, TrackIndex_index))
            goto fail;
    }

    if (NumBlocks * 0x02 > BlockIndex_size) {
        VGM_LOG("ACB: wrong BlockSequence.BlockIndex size\n");
        goto fail;
    }

    /* read Blocks inside BlockSequence */
    for (i = 0; i < NumBlocks; i++) {
        int16_t BlockIndex_index = read_s16be(BlockIndex_offset + i*0x02, acb->BlockSequenceSf);

        if (!load_acb_block(acb, BlockIndex_index))
            goto fail;
    }

    return 1;
fail:
    return 0;
}

static int load_acb_cue(acb_header* acb, int16_t Index) {
    uint8_t ReferenceType;
    uint16_t ReferenceIndex;


    /* read Cue[Index] */
    if (!open_utf_subtable(acb, &acb->CueSf, &acb->CueTable, "CueTable", NULL, ACB_TABLE_BUFFER_CUE))
        goto fail;
    if (!utf_query_u8(acb->CueTable, Index, "ReferenceType", &ReferenceType))
        goto fail;
    if (!utf_query_u16(acb->CueTable, Index, "ReferenceIndex", &ReferenceIndex))
        goto fail;
    //;VGM_LOG("ACB: Cue[%i]: ReferenceType=%i, ReferenceIndex=%i\n", Index, ReferenceType, ReferenceIndex);


    /* usually older games use older references but not necessarily */
    switch(ReferenceType) {

        case 0x01: /* Cue > Waveform (ex. PES 2015) */
            if (!load_acb_waveform(acb, ReferenceIndex))
                goto fail;
            break;

        case 0x02: /* Cue > Synth > Waveform (ex. Ukiyo no Roushi) */
            if (!load_acb_synth(acb, ReferenceIndex))
                goto fail;
            break;

        case 0x03: /* Cue > Sequence > Track > Command > Synth > Waveform (ex. Valkyrie Profile anatomia, Yakuza Kiwami 2) */
            if (!load_acb_sequence(acb, ReferenceIndex))
                goto fail;
            break;

        case 0x08: /* Cue > BlockSequence > Track / Block > Track > Command > Synth > Waveform (ex. Sonic Lost World, Kandagawa Jet Girls, rare) */
            if (!load_acb_blocksequence(acb, ReferenceIndex))
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
            VGM_LOG("ACB: unknown Cue.ReferenceType=%x, Cue.ReferenceIndex=%x\n", ReferenceType, ReferenceIndex);
            break; /* ignore and continue */
    }


    return 1;
fail:
    return 0;
}

static int load_acb_cuename(acb_header* acb, int16_t Index) {
    uint16_t CueIndex;
    const char* CueName;


    /* read CueName[Index] */
    if (!open_utf_subtable(acb, &acb->CueNameSf, &acb->CueNameTable, "CueNameTable", NULL, ACB_TABLE_BUFFER_CUENAME))
        goto fail;
    if (!utf_query_u16(acb->CueNameTable, Index, "CueIndex", &CueIndex))
        goto fail;
    if (!utf_query_string(acb->CueNameTable, Index, "CueName", &CueName))
        goto fail;
    //;VGM_LOG("ACB: CueName[%i]: CueIndex=%i, CueName=%s\n", Index, CueIndex, CueName);


    /* save as will be needed if references waveform */
    acb->cuename_index = Index;
    acb->cuename_name = CueName;

    if (!load_acb_cue(acb, CueIndex))
        goto fail;

    return 1;
fail:
    return 0;
}


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
     */

    //;VGM_LOG("ACB: find waveid=%i, port=%i\n", waveid, port);

    acb.acbFile = sf;

    acb.Header = utf_open(acb.acbFile, 0x00, NULL, NULL);
    if (!acb.Header) goto fail;

    acb.target_waveid = waveid;
    acb.target_port = port;
    acb.is_memory = is_memory;
    acb.has_TrackEventTable = utf_query_data(acb.Header, 0, "TrackEventTable", NULL,NULL);
    acb.has_CommandTable = utf_query_data(acb.Header, 0, "CommandTable", NULL,NULL);


    /* read all possible cue names and find which waveids are referenced by it */
    if (!open_utf_subtable(&acb, &acb.CueNameSf, &acb.CueNameTable, "CueNameTable", &CueName_rows, ACB_TABLE_BUFFER_CUENAME))
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
}

#include "meta.h"
#include "../coding/coding.h"
#include "acb_utf.h"


/* ACB (Atom Cue sheet Binary) - CRI container of memory audio, often together with a .awb wave bank */
VGMSTREAM * init_vgmstream_acb(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    STREAMFILE *temp_streamFile = NULL;
    off_t subfile_offset;
    size_t subfile_size;
    utf_context *utf = NULL;


    /* checks */
    if (!check_extensions(streamFile, "acb"))
        goto fail;
    if (read_32bitBE(0x00,streamFile) != 0x40555446) /* "@UTF" */
        goto fail;

    /* .acb is a cue sheet that uses @UTF (CRI's generic table format) to store row/columns
     * with complex info (cues, sequences, spatial info, etc). it can store a memory .awb
     * (our target here), or reference external/streamed .awb (loaded elsewhere)
     * we only want .awb with actual waves but may use .acb to get names */
    {
        int rows;
        const char* name;
        uint32_t offset = 0, size = 0;
        uint32_t table_offset = 0x00;

        utf = utf_open(streamFile, table_offset, &rows, &name);
        if (!utf) goto fail;

        if (rows != 1 || strcmp(name, "Header") != 0)
            goto fail;

        //todo acb+cpk is also possible

        if (!utf_query_data(streamFile, utf, 0, "AwbFile", &offset, &size))
            goto fail;

        subfile_offset = table_offset + offset;
        subfile_size = size;

        /* column exists but can be empty */
        if (subfile_size == 0)
            goto fail;
    }

    //;VGM_LOG("ACB: subfile offset=%lx + %x\n", subfile_offset, subfile_size);

    temp_streamFile = setup_subfile_streamfile(streamFile, subfile_offset,subfile_size, "awb");
    if (!temp_streamFile) goto fail;

    vgmstream = init_vgmstream_awb_memory(temp_streamFile, streamFile);
    if (!vgmstream) goto fail;

    /* name-loading for this for memory .awb will be called from init_vgmstream_awb_memory */

    utf_close(utf);
    close_streamfile(temp_streamFile);
    return vgmstream;

fail:
    utf_close(utf);
    close_streamfile(temp_streamFile);
    close_vgmstream(vgmstream);
    return NULL;
}

/* ************************************** */

typedef struct {
    /* keep track of these tables so they can be closed when done */
    utf_context *Header;
    utf_context *CueNameTable;
    utf_context *CueTable;
    utf_context *BlockTable;
    utf_context *SequenceTable;
    utf_context *TrackTable;
    utf_context *TrackEventTable;
    utf_context *CommandTable;
    utf_context *SynthTable;
    utf_context *WaveformTable;

    /* config */
    int is_memory;
    int target_waveid;
    int has_TrackEventTable;
    int has_CommandTable;

    /* to avoid infinite/circular references (AtomViewer crashes otherwise) */
    int synth_depth;
    int sequence_depth;

    /* name stuff */
    int16_t cuename_index;
    const char * cuename_name;
    int awbname_count;
    int16_t awbname_list[255];
    char name[1024];

} acb_header;

static int load_utf_subtable(STREAMFILE *acbFile, acb_header* acb, utf_context* *Table, const char* TableName, int* rows) {
    uint32_t offset = 0;

    /* already loaded */
    if (*Table != NULL)
        return 1;

    if (!utf_query_data(acbFile, acb->Header, 0, TableName, &offset, NULL))
        goto fail;
    *Table = utf_open(acbFile, offset, rows, NULL);
    if (!*Table) goto fail;

    //;VGM_LOG("ACB: loaded table %s\n", TableName);
    return 1;
fail:
    return 0;
}


static void add_acb_name(STREAMFILE *acbFile, acb_header* acb, int8_t Waveform_Streaming) {
    //todo safe string ops

    /* ignore name repeats */
    if (acb->awbname_count) {
        int i;
        for (i = 0; i < acb->awbname_count; i++) {
            if (acb->awbname_list[i] == acb->cuename_index)
                return;
        }
    }

    /* since waveforms can be reused by cues multiple names are a thing */
    if (acb->awbname_count) {
        strcat(acb->name, "; ");
        strcat(acb->name, acb->cuename_name);
    }
    else {
        strcpy(acb->name, acb->cuename_name);
    }
    if (Waveform_Streaming == 2 && acb->is_memory) {
        strcat(acb->name, " [pre]");
    }

    acb->awbname_list[acb->awbname_count] = acb->cuename_index;
    acb->awbname_count++;
    if (acb->awbname_count >= 254)
        acb->awbname_count = 254; /* ??? */

    //;VGM_LOG("ACB: found cue for waveid=%i: %s\n", acb->target_waveid, acb->cuename_name);
}


static int load_acb_waveform(STREAMFILE *acbFile, acb_header* acb, int16_t Index) {
    int16_t Waveform_Id;
    int8_t Waveform_Streaming;

    /* read Waveform[Index] */
    if (!load_utf_subtable(acbFile, acb, &acb->WaveformTable, "WaveformTable", NULL))
        goto fail;
    if (!utf_query_s16(acbFile, acb->WaveformTable, Index, "Id", &Waveform_Id)) { /* older versions use Id */
        if (acb->is_memory) {
            if (!utf_query_s16(acbFile, acb->WaveformTable, Index, "MemoryAwbId", &Waveform_Id))
                goto fail;
        } else {
            if (!utf_query_s16(acbFile, acb->WaveformTable, Index, "StreamAwbId", &Waveform_Id))
                goto fail;
        }
    }
    if (!utf_query_s8(acbFile, acb->WaveformTable, Index, "Streaming", &Waveform_Streaming))
        goto fail;
    //;VGM_LOG("ACB: Waveform[%i]: Id=%i, Streaming=%i\n", Index, Waveform_Id, Waveform_Streaming);

    /* not found but valid */
    if (Waveform_Id != acb->target_waveid)
        return 1;
    /* must match our target's (0=memory, 1=streaming, 2=memory (prefetch)+stream) */
    if ((acb->is_memory && Waveform_Streaming == 1) || (!acb->is_memory && Waveform_Streaming == 0))
        return 1;

    /* aaand finally get name (phew) */
    add_acb_name(acbFile, acb, Waveform_Streaming);

    return 1;
fail:
    return 0;
}

/* define here for Synths pointing to Sequences */
static int load_acb_sequence(STREAMFILE *acbFile, acb_header* acb, int16_t Index);

static int load_acb_synth(STREAMFILE *acbFile, acb_header* acb, int16_t Index) {
    int i, count;
    int8_t Synth_Type;
    uint32_t Synth_ReferenceItems_offset;
    uint32_t Synth_ReferenceItems_size;


    /* read Synth[Index] */
    if (!load_utf_subtable(acbFile, acb, &acb->SynthTable, "SynthTable", NULL))
        goto fail;
    if (!utf_query_s8(acbFile, acb->SynthTable, Index, "Type", &Synth_Type))
        goto fail;
    if (!utf_query_data(acbFile, acb->SynthTable, Index, "ReferenceItems", &Synth_ReferenceItems_offset, &Synth_ReferenceItems_size))
        goto fail;
    //;VGM_LOG("ACB: Synth[%i]: Type=%x, ReferenceItems={%x,%x}\n", Index, Synth_Type, Synth_ReferenceItems_offset, Synth_ReferenceItems_size);

    acb->synth_depth++;

    if (acb->synth_depth > 2) {
        VGM_LOG("ACB: Synth depth too high\n");
        goto fail; /* max Synth > Synth > Waveform (ex. Yakuza 6) */
    }

    /* Cue.ReferenceType 2 uses Synth.Type, while 3 always sets it to 0 and uses Sequence.Type instead
     * Both look the same and probably affect which item in the ReferenceItems list is picked:
     * - 0: polyphonic (1 item)
     * - 1: sequential (1 to N?)
     * - 2: shuffle (1 from N?)
     * - 3: random (1 from N?)
     * - 4: no repeat
     * - 5: switch game variable
     * - 6: combo sequential
     * - 7: switch selector
     * - 8: track transition by selector
     * - other: undefined?
     * Since we want to find all possible Waveforms that could match our id, we ignore Type and just parse all ReferenceItems.
     */

    count = Synth_ReferenceItems_size / 0x04;
    for (i = 0; i < count; i++) {
        uint16_t Synth_ReferenceItem_type  = read_u16be(Synth_ReferenceItems_offset + i*0x04 + 0x00, acbFile);
        uint16_t Synth_ReferenceItem_index = read_u16be(Synth_ReferenceItems_offset + i*0x04 + 0x02, acbFile);
        //;VGM_LOG("ACB: Synth.ReferenceItem: type=%x, index=%x\n", Synth_ReferenceItem_type, Synth_ReferenceItem_index);

        switch(Synth_ReferenceItem_type) {
            case 0x00: /* no reference */
                count = 0;
                break;

            case 0x01: /* Waveform (most common) */
                if (!load_acb_waveform(acbFile, acb, Synth_ReferenceItem_index))
                    goto fail;
                break;

            case 0x02: /* Synth, possibly random (rare, found in Sonic Lost World with ReferenceType 2) */
                if (!load_acb_synth(acbFile, acb, Synth_ReferenceItem_index))
                    goto fail;
                break;

            case 0x03: /* Sequence of Synths w/ % in Synth.TrackValues (rare, found in Sonic Lost World with ReferenceType 2) */
                if (!load_acb_sequence(acbFile, acb, Synth_ReferenceItem_index))
                    goto fail;
                break;

            case 0x06:  /* this seems to point to Synth but results don't make sense (rare, from Sonic Lost World) */
            default: /* undefined/crashes AtomViewer */
                VGM_LOG("ACB: unknown Synth.ReferenceItem type %x at %x + %x\n", Synth_ReferenceItem_type, Synth_ReferenceItems_offset, Synth_ReferenceItems_size);
                count = 0; /* force end without failing */
                break;
        }
    }

    acb->synth_depth--;

    return 1;
fail:
    return 0;
}

static int load_acb_track_event_command(STREAMFILE *acbFile, acb_header* acb, int16_t Index) {
    int16_t Track_EventIndex;
    uint32_t Track_Command_offset;
    uint32_t Track_Command_size;


    /* read Track[Index] */
    if (!load_utf_subtable(acbFile, acb, &acb->TrackTable, "TrackTable", NULL))
        goto fail;
    if (!utf_query_s16(acbFile, acb->TrackTable, Index, "EventIndex", &Track_EventIndex))
        goto fail;
    //;VGM_LOG("ACB: Track[%i]: EventIndex=%i\n", Index, Track_EventIndex);

    /* next link varies with version, check by table existence */
    if (acb->has_CommandTable) { /* <=v1.27 */
        /* read Command[EventIndex] */
        if (!load_utf_subtable(acbFile, acb, &acb->CommandTable, "CommandTable", NULL))
            goto fail;
        if (!utf_query_data(acbFile, acb->CommandTable, Track_EventIndex, "Command", &Track_Command_offset, &Track_Command_size))
            goto fail;
        //;VGM_LOG("ACB: Command[%i]: Command={%x,%x}\n", Track_EventIndex, Track_Command_offset,Track_Command_size);
    }
    else if (acb->has_TrackEventTable) { /* >=v1.28 */
        /* read TrackEvent[EventIndex] */
        if (!load_utf_subtable(acbFile, acb, &acb->TrackEventTable, "TrackEventTable", NULL))
            goto fail;
        if (!utf_query_data(acbFile, acb->TrackEventTable, Track_EventIndex, "Command", &Track_Command_offset, &Track_Command_size))
            goto fail;
        //;VGM_LOG("ACB: TrackEvent[%i]: Command={%x,%x}\n", Track_EventIndex, Track_Command_offset,Track_Command_size);
    }
    else {
        VGM_LOG("ACB: unknown command table\n");
        goto fail;
    }

    /* read Command (some kind of multiple TLVs, this seems ok) */
    {
        uint32_t offset = Track_Command_offset;
        uint32_t max_offset = Track_Command_offset + Track_Command_size;
        uint16_t tlv_code, tlv_type, tlv_index;
        uint8_t tlv_size;


        while (offset < max_offset) {
            tlv_code = read_u16be(offset + 0x00, acbFile);
            tlv_size = read_u8   (offset + 0x02, acbFile);
            offset += 0x03;

            if (tlv_code == 0x07D0) {
                if (tlv_size < 0x04) {
                    VGM_LOG("ACB: TLV with unknown size\n");
                    break;
                }

                tlv_type = read_u16be(offset + 0x00, acbFile);
                tlv_index = read_u16be(offset + 0x02, acbFile);
                //;VGM_LOG("ACB: TLV at %x: type %x, index=%x\n", offset, tlv_type, tlv_index);

                /* probably same as Synth_ReferenceItem_type */
                switch(tlv_type) {

                    case 0x02: /* Synth (common) */
                        if (!load_acb_synth(acbFile, acb, tlv_index))
                            goto fail;
                        break;

                    case 0x03: /* Sequence of Synths (common, ex. Yakuza 6, Yakuza Kiwami 2) */
                        if (!load_acb_sequence(acbFile, acb, tlv_index))
                            goto fail;
                        break;

                    default:
                        VGM_LOG("ACB: unknown TLV type %x at %x + %x\n", tlv_type, offset, tlv_size);
                        max_offset = 0; /* force end without failing */
                        break;
                }
            }

            /* 0x07D1 comes suspiciously often paired with 0x07D0 too */

            offset += tlv_size;
        }
    }

    return 1;
fail:
    return 0;
}

static int load_acb_sequence(STREAMFILE *acbFile, acb_header* acb, int16_t Index) {
    int i;
    int16_t Sequence_NumTracks;
    uint32_t Sequence_TrackIndex_offset;
    uint32_t Sequence_TrackIndex_size;


    /* read Sequence[Index] */
    if (!load_utf_subtable(acbFile, acb, &acb->SequenceTable, "SequenceTable", NULL))
        goto fail;
    if (!utf_query_s16(acbFile, acb->SequenceTable, Index, "NumTracks", &Sequence_NumTracks))
        goto fail;
    if (!utf_query_data(acbFile, acb->SequenceTable, Index, "TrackIndex", &Sequence_TrackIndex_offset, &Sequence_TrackIndex_size))
        goto fail;
    //;VGM_LOG("ACB: Sequence[%i]: NumTracks=%i, TrackIndex={%x, %x}\n", Index, Sequence_NumTracks, Sequence_TrackIndex_offset,Sequence_TrackIndex_size);

    acb->sequence_depth++;

    if (acb->sequence_depth > 3) {
        VGM_LOG("ACB: Sequence depth too high\n");
        goto fail; /* max Sequence > Sequence > Sequence > Synth > Waveform (ex. Yakuza 6) */
    }

    if (Sequence_NumTracks * 0x02 > Sequence_TrackIndex_size) { /* padding may exist */
        VGM_LOG("ACB: wrong Sequence.TrackIndex size\n");
        goto fail;
    }

    /* read Tracks inside Sequence */
    for (i = 0; i < Sequence_NumTracks; i++) {
        int16_t Sequence_TrackIndex_index = read_s16be(Sequence_TrackIndex_offset + i*0x02, acbFile);

        if (!load_acb_track_event_command(acbFile, acb, Sequence_TrackIndex_index))
            goto fail;
    }

    acb->sequence_depth--;

    return 1;
fail:
    return 0;
}

static int load_acb_block(STREAMFILE *acbFile, acb_header* acb, int16_t Index) {
    int i;
    int16_t Block_NumTracks;
    uint32_t Block_TrackIndex_offset;
    uint32_t Block_TrackIndex_size;


    /* read Block[Index] */
    if (!load_utf_subtable(acbFile, acb, &acb->BlockTable, "BlockTable", NULL))
        goto fail;
    if (!utf_query_s16(acbFile, acb->BlockTable, Index, "NumTracks", &Block_NumTracks))
        goto fail;
    if (!utf_query_data(acbFile, acb->BlockTable, Index, "TrackIndex", &Block_TrackIndex_offset, &Block_TrackIndex_size))
        goto fail;
    //;VGM_LOG("ACB: Block[%i]: NumTracks=%i, TrackIndex={%x, %x}\n", Index, Block_NumTracks, Block_TrackIndex_offset,Block_TrackIndex_size);

    if (Block_NumTracks * 0x02 > Block_TrackIndex_size) { /* padding may exist */
        VGM_LOG("ACB: wrong Block.TrackIndex size\n");
        goto fail;
    }

    /* read Tracks inside Block */
    for (i = 0; i < Block_NumTracks; i++) {
        int16_t Block_TrackIndex_index = read_s16be(Block_TrackIndex_offset + i*0x02, acbFile);

        if (!load_acb_track_event_command(acbFile, acb, Block_TrackIndex_index))
            goto fail;
    }

    return 1;
fail:
    return 0;

}

static int load_acb_cue(STREAMFILE *acbFile, acb_header* acb, int16_t Index) {
    int8_t Cue_ReferenceType;
    int16_t Cue_ReferenceIndex;


    /* read Cue[Index] */
    if (!load_utf_subtable(acbFile, acb, &acb->CueTable, "CueTable", NULL))
        goto fail;
    if (!utf_query_s8 (acbFile, acb->CueTable, Index, "ReferenceType", &Cue_ReferenceType))
        goto fail;
    if (!utf_query_s16(acbFile, acb->CueTable, Index, "ReferenceIndex", &Cue_ReferenceIndex))
        goto fail;
    //;VGM_LOG("ACB: Cue[%i]: ReferenceType=%i, ReferenceIndex=%i\n", Index, Cue_ReferenceType, Cue_ReferenceIndex);


    /* usually older games use older references but not necessarily */
    switch(Cue_ReferenceType) {

        case 1: /* Cue > Waveform (ex. PES 2015) */
            if (!load_acb_waveform(acbFile, acb, Cue_ReferenceIndex))
                goto fail;
            break;

        case 2: /* Cue > Synth > Waveform (ex. Ukiyo no Roushi) */
            if (!load_acb_synth(acbFile, acb, Cue_ReferenceIndex))
                goto fail;
            break;

        case 3: /* Cue > Sequence > Track > Command > Synth > Waveform (ex. Valkyrie Profile anatomia, Yakuza Kiwami 2) */
            if (!load_acb_sequence(acbFile, acb, Cue_ReferenceIndex))
                goto fail;
            break;

        case 8: /* Cue > Block > Track > Command > Synth > Waveform (ex. Sonic Lost World, rare) */
            if (!load_acb_block(acbFile, acb, Cue_ReferenceIndex))
                goto fail;
            break;

        default:
            VGM_LOG("ACB: unknown Cue.ReferenceType=%x, Cue.ReferenceIndex=%x\n", Cue_ReferenceType, Cue_ReferenceIndex);
            break; /* ignore and continue */
    }


    return 1;
fail:
    return 0;

}

static int load_acb_cuename(STREAMFILE *acbFile, acb_header* acb, int16_t Index) {
    int16_t CueName_CueIndex;
    const char* CueName_CueName;


    /* read CueName[Index] */
    if (!load_utf_subtable(acbFile, acb, &acb->CueNameTable, "CueNameTable", NULL))
        goto fail;
    if (!utf_query_s16(acbFile, acb->CueNameTable, Index, "CueIndex", &CueName_CueIndex))
        goto fail;
    if (!utf_query_string(acbFile, acb->CueNameTable, Index, "CueName", &CueName_CueName))
        goto fail;
    //;VGM_LOG("ACB: CueName[%i]: CueIndex=%i, CueName=%s\n", Index, CueName_CueIndex, CueName_CueName);


    /* save as will be needed if references waveform */
    acb->cuename_index = Index;
    acb->cuename_name = CueName_CueName;

    if (!load_acb_cue(acbFile, acb, CueName_CueIndex))
        goto fail;

    return 1;
fail:
    return 0;
}


void load_acb_wave_name(STREAMFILE *acbFile, VGMSTREAM* vgmstream, int waveid, int is_memory) {
    acb_header acb = {0};
    int i, CueName_rows;


    if (!acbFile || !vgmstream || waveid < 0)
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
     * - others should be possible but haven't been observed
     * Atom Craft may only target certain .acb versions so some links are later removed
     * Not all cues to point to though Waveforms, as some are just config events/commands.
     * .acb link to .awb by name (loaded manually), though they have a checksum/hash to validate.
     */

    //;VGM_LOG("ACB: find waveid=%i\n", waveid);

    acb.Header = utf_open(acbFile, 0x00, NULL, NULL);
    if (!acb.Header) goto fail;

    acb.target_waveid = waveid;
    acb.is_memory = is_memory;
    acb.has_TrackEventTable = utf_query_data(acbFile, acb.Header, 0, "TrackEventTable", NULL,NULL);
    acb.has_CommandTable = utf_query_data(acbFile, acb.Header, 0, "CommandTable", NULL,NULL);


    /* read all possible cue names and find which waveids are referenced by it */
    if (!load_utf_subtable(acbFile, &acb, &acb.CueNameTable, "CueNameTable", &CueName_rows))
        goto fail;
    for (i = 0; i < CueName_rows; i++) {

        if (!load_acb_cuename(acbFile, &acb, i))
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
    utf_close(acb.SequenceTable);
    utf_close(acb.TrackTable);
    utf_close(acb.TrackEventTable);
    utf_close(acb.CommandTable);
    utf_close(acb.SynthTable);
    utf_close(acb.WaveformTable);
}

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
    utf_context *Header;
    utf_context *CueNameTable;
    utf_context *CueTable;
    utf_context *SequenceTable;
    utf_context *TrackTable;
    utf_context *TrackEventTable;
    utf_context *CommandTable;
    utf_context *SynthTable;
    utf_context *WaveformTable;

    char name[1024];
    int is_memory;
    int has_TrackEventTable;
    int has_CommandTable;

    int16_t CueNameIndex;
    const char* CueName;
    int16_t CueIndex;
    int16_t ReferenceIndex;
    int8_t ReferenceType;

    int16_t NumTracks;
    uint32_t TrackIndex_offset;
    uint32_t TrackIndex_size;
    int16_t TrackIndex;
    int16_t EventIndex;
    uint32_t Command_offset;
    uint32_t Command_size;
    int16_t SynthIndex_count;
    int16_t SynthIndex_list[255];
    int16_t SynthIndex;

    int8_t SynthType;
    uint32_t ReferenceItems_offset;
    uint32_t ReferenceItems_size;
    int ReferenceItems_count;
    int16_t ReferenceItems_list[255];

    int16_t ReferenceItem;
    int16_t AwbId;
    int8_t AwbStreaming;

    int is_wave_found;

    int AwbName_count;
    int16_t AwbName_list[255];

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


    ;VGM_LOG("ACB: loaded table %s\n", TableName);
    return 1;
fail:
    return 0;
}

static int load_acb_cue_info(STREAMFILE *acbFile, acb_header* acb) {

    /* read Cue[CueNameIndex] */
    if (!utf_query_s16(acbFile, acb->CueNameTable, acb->CueNameIndex, "CueIndex", &acb->CueIndex))
        goto fail;
    if (!utf_query_string(acbFile, acb->CueNameTable, acb->CueNameIndex, "CueName", &acb->CueName))
        goto fail;
    ;VGM_LOG("ACB: CueName[%i]: CueIndex=%i, CueName=%s\n", acb->CueNameIndex, acb->CueIndex, acb->CueName);

    /* read Cue[CueIndex] */
    if (!load_utf_subtable(acbFile, acb, &acb->CueTable, "CueTable", NULL))
        goto fail;
    if (!utf_query_s8 (acbFile, acb->CueTable, acb->CueIndex, "ReferenceType", &acb->ReferenceType))
        goto fail;
    if (!utf_query_s16(acbFile, acb->CueTable, acb->CueIndex, "ReferenceIndex", &acb->ReferenceIndex))
        goto fail;
    ;VGM_LOG("ACB: Cue[%i]: ReferenceType=%i, ReferenceIndex=%i\n", acb->CueIndex, acb->ReferenceType, acb->ReferenceIndex);

    return 1;
fail:
    return 0;
}

static int load_acb_sequence(STREAMFILE *acbFile, acb_header* acb) {

    /* read Sequence[ReferenceIndex] */
    if (!load_utf_subtable(acbFile, acb, &acb->SequenceTable, "SequenceTable", NULL))
        goto fail;
    if (!utf_query_s16(acbFile, acb->SequenceTable, acb->ReferenceIndex, "NumTracks", &acb->NumTracks))
        goto fail;
    if (!utf_query_data(acbFile, acb->SequenceTable, acb->ReferenceIndex, "TrackIndex", &acb->TrackIndex_offset, &acb->TrackIndex_size))
        goto fail;
    ;VGM_LOG("ACB: Sequence[%i]: NumTracks=%i, TrackIndex={%x, %x}\n", acb->ReferenceIndex, acb->NumTracks, acb->TrackIndex_offset,acb->TrackIndex_size);

    if (acb->NumTracks * 0x02 > acb->TrackIndex_size) { /* padding may exist */
        VGM_LOG("ACB: unknown TrackIndex size\n");
        goto fail;
    }

    return 1;
fail:
    return 0;
}

static int load_acb_track_command(STREAMFILE *acbFile, acb_header* acb) {

    /* read Track[TrackIndex] */
    if (!load_utf_subtable(acbFile, acb, &acb->TrackTable, "TrackTable", NULL))
        goto fail;
    if (!utf_query_s16(acbFile, acb->TrackTable, acb->TrackIndex, "EventIndex", &acb->EventIndex))
        goto fail;
    ;VGM_LOG("ACB: Track[%i]: EventIndex=%i\n", acb->TrackIndex, acb->EventIndex);

    /* depending on version next stuff varies a bit, check by table existence */
    if (acb->has_TrackEventTable) {
        /* read TrackEvent[EventIndex] */
        if (!load_utf_subtable(acbFile, acb, &acb->TrackEventTable, "TrackEventTable", NULL))
            goto fail;
        if (!utf_query_data(acbFile, acb->TrackEventTable, acb->EventIndex, "Command", &acb->Command_offset, &acb->Command_size))
            goto fail;
        ;VGM_LOG("ACB: TrackEvent[%i]: Command={%x,%x}\n", acb->EventIndex, acb->Command_offset,acb->Command_size);
    }
    else if (acb->has_CommandTable) {
        /* read Command[EventIndex] */
        if (!load_utf_subtable(acbFile, acb, &acb->CommandTable, "CommandTable", NULL))
            goto fail;
        if (!utf_query_data(acbFile, acb->CommandTable, acb->EventIndex, "Command", &acb->Command_offset, &acb->Command_size))
            goto fail;
        ;VGM_LOG("ACB: Command[%i]: Command={%x,%x}\n", acb->EventIndex, acb->Command_offset,acb->Command_size);
    }
    else {
        VGM_LOG("ACB: unknown command table\n");
    }

    /* read Command (some kind of multiple TLVs, this seems ok) */
    {
        uint32_t offset = acb->Command_offset;
        uint32_t max_offset = acb->Command_offset + acb->Command_size;
        uint16_t code, subcode, subindex;
        uint8_t size;

        acb->SynthIndex_count = 0;

        while (offset < max_offset) {
            code = read_u16be(offset + 0x00, acbFile);
            size = read_u8   (offset + 0x02, acbFile);
            offset += 0x03;

            if (code == 0x07D0) {
                if (size < 0x04) {
                    VGM_LOG("ACB: subcommand with unknown size\n");
                    break;
                }

                subcode = read_u16be(offset + 0x00, acbFile);
                subindex = read_u16be(offset + 0x02, acbFile);

                /* reference to Synth/Waveform like those in Synth? */
                if (subcode != 0x02) { //todo some in Yakuza Kiwami 2 usen.acb use 03 (random? see Synth)
                    VGM_LOG("ACB: subcommand with unknown subcode\n");
                    break;
                }

                acb->SynthIndex_list[acb->SynthIndex_count] = subindex;
                acb->SynthIndex_count++;
                if (acb->SynthIndex_count >= 254)
                    acb->ReferenceItems_count = 254; /* ??? */

                ;VGM_LOG("ACB: subcommand index %i found\n", subindex);
            }

            /* 0x07D1 comes suspiciously often paired with 0x07D0 too */

            offset += size;
        }
    }

    return 1;
fail:
    return 0;
}

static int load_acb_synth(STREAMFILE *acbFile, acb_header* acb) {
    int i;

    /* read Synth[SynthIndex] */
    if (!load_utf_subtable(acbFile, acb, &acb->SynthTable, "SynthTable", NULL))
        goto fail;
    if (!utf_query_s8(acbFile, acb->SynthTable, acb->SynthIndex, "Type", &acb->SynthType))
        goto fail;
    if (!utf_query_data(acbFile, acb->SynthTable, acb->SynthIndex, "ReferenceItems", &acb->ReferenceItems_offset, &acb->ReferenceItems_size))
        goto fail;
    ;VGM_LOG("ACB: Synth[%i]: ReferenceItems={%x,%x}\n", acb->SynthIndex, acb->ReferenceItems_offset, acb->ReferenceItems_size);


    acb->ReferenceItems_count = acb->ReferenceItems_size / 0x04;
    if (acb->ReferenceItems_count >= 254)
        acb->ReferenceItems_count = 254; /* ??? */

    /* ReferenceType 2 uses Synth.Type, while 3 always sets it to 0 and uses Sequence.Type instead
     * they probably affect which item in the reference list is picked:
     * 0: polyphonic
     * 1: sequential
     * 2: shuffle
     * 3: random
     * 4: no repeat
     * 5: switch game variable
     * 6: combo sequential
     * 7: switch selector
     * 8: track transition by selector
     * other: undefined?
     */

    for (i = 0; i < acb->ReferenceItems_count; i++) {
        uint16_t type, subtype, index, subindex;
        uint32_t suboffset, subsize;

        type  = read_u16be(acb->ReferenceItems_offset + i*0x04 + 0x00, acbFile);
        index = read_u16be(acb->ReferenceItems_offset + i*0x04 + 0x02, acbFile);
        ;VGM_LOG("ACB: Synth reference type=%x, index=%x\n", type, index);

        switch(type) {
            case 0x00: /* no reference */
                acb->ReferenceItems_count = 0;
                break;

            case 0x01: /* Waveform reference (most common) */
                acb->ReferenceItems_list[i] = index;
                break;

            case 0x02: /* Synth reference (rare, found in Sonic Lost World with ReferenceType 2) */
                if (!utf_query_data(acbFile, acb->SynthTable, index, "ReferenceItems", &suboffset, &subsize))
                    goto fail;

                /* assuming only 1:1 references are ok */
                if (subsize != 0x04) {
                    VGM_LOG("ACB: unknown Synth subreference size\n");
                    break;
                }

                subtype  = read_u16be(suboffset + 0x00, acbFile);
                subindex = read_u16be(suboffset + 0x02, acbFile);

                /* AtomViewer crashes if it points to another to Synth */
                if (subtype != 0x01) {
                    VGM_LOG("ACB: unknown Synth subreference type\n");
                    break;
                }

                acb->ReferenceItems_list[i] = subindex;
                ;VGM_LOG("ACB: Synth subreference type=%x, index=%x\n", subtype, subindex);
                break;

            case 0x03: /* random Synths with % in TrackValues (rare, found in Sonic Lost World with ReferenceType 2) */
                //todo fix
                /* this points to next? N Synths (in turn pointing to a Waveform), but I see no relation between
                 * index value and pointed Synths plus those Synths don't seem referenced otherwise.
                 * ex. se_phantom_asteroid.acb
                 *  Synth[26] with index 7 points to Synth[27/28]
                 *  Synth[26] with index 6 points to Synth[24/25]
                 *  Synth[26] with index 8 points to Synth[29/30]
                 *  Synth[26] with index 9 points to Synth[32/33] (Synth[30] is another random Type 3)
                 */
            default: /* undefined/crashes AtomViewer */
                VGM_LOG("ACB: unknown Synth reference type\n");
                acb->ReferenceItems_count = 0;
                break;
        }
    }

    return 1;
fail:
    return 0;
}

static int load_acb_waveform(STREAMFILE *acbFile, acb_header* acb, int waveid) {

    /* read Waveform[ReferenceItem] */
    if (!load_utf_subtable(acbFile, acb, &acb->WaveformTable, "WaveformTable", NULL))
        goto fail;
    if (!utf_query_s8(acbFile, acb->WaveformTable, acb->ReferenceItem, "Streaming", &acb->AwbStreaming))
        goto fail;
    if (!utf_query_s16(acbFile, acb->WaveformTable, acb->ReferenceItem, "Id", &acb->AwbId)) { /* older versions use Id */
        if (acb->is_memory) {
            if (!utf_query_s16(acbFile, acb->WaveformTable, acb->ReferenceItem, "MemoryAwbId", &acb->AwbId))
                goto fail;
        } else {
            if (!utf_query_s16(acbFile, acb->WaveformTable, acb->ReferenceItem, "StreamAwbId", &acb->AwbId))
                goto fail;
        }
    }
    ;VGM_LOG("ACB: Waveform[%i]: AwbId=%i, AwbStreaming=%i\n", acb->ReferenceItem, acb->AwbId, acb->AwbStreaming);

    acb->is_wave_found = 0; /* reset */

    if (acb->AwbId != waveid)
        return 1;
    /* 0=memory, 1=streaming, 2=memory (preload)+stream */
    if ((acb->is_memory && acb->AwbStreaming == 1) || (!acb->is_memory && acb->AwbStreaming == 0))
        return 1;

    acb->is_wave_found = 1;

    return 1;
fail:
    return 0;
}

static void add_acb_name(STREAMFILE *acbFile, acb_header* acb) {
    //todo safe string ops

    /* aaand finally get name (phew) */

    /* ignore name repeats */
    if (acb->AwbName_count) {
        int i;
        for (i = 0; i < acb->AwbName_count; i++) {
            if (acb->AwbName_list[i] == acb->CueNameIndex)
                return;
        }
    }

    /* since waveforms can be reused by cues multiple names are a thing */
    if (acb->AwbName_count) {
        strcat(acb->name, "; ");
        strcat(acb->name, acb->CueName);
    }
    else {
        strcpy(acb->name, acb->CueName);
    }
    if (acb->AwbStreaming == 2 && acb->is_memory) {
        strcat(acb->name, " [pre]");
    }

    acb->AwbName_list[acb->AwbName_count] = acb->CueNameIndex;
    acb->AwbName_count++;
    if (acb->AwbName_count >= 254)
        acb->AwbName_count = 254; /* ??? */

    ;VGM_LOG("ACB: found cue for waveid=%i: %s\n", acb->AwbId, acb->CueName);
}


void load_acb_wave_name(STREAMFILE *acbFile, VGMSTREAM* vgmstream, int waveid, int is_memory) {
    acb_header acb = {0};
    int CueName_rows, CueName_i, TrackIndex_i, ReferenceItems_i, SynthIndex_i;

    if (!acbFile || !vgmstream || waveid < 0)
        return;

    /* Normally games load a .acb + .awb, and asks the .acb to play a cue by name or index.
     * Since we only care for actual waves, to get its name we need to find which cue uses our wave.
     * Multiple cues can use the same wave (meaning multiple names), and one cue may use multiple waves.
     * There is no easy way to map cue name <> wave name so basically we parse the whole thing.
     *
     * .acb cues are created in CRI Atom Craft roughly like this:
     * - user creates N Cues with CueName
     * - Cues define Sequences of Tracks
     * - depending on reference types:
     *   - Track points directly to Waveform (type 1)
     *   - Track points to Synth then to Waveform (type 2)
     *   - Track points to Commands with binary Command that points to Synth then to Waveform (type 3 <=v1.27)
     *   - Track points to TrackEvent with binary Command that points to Synth then to Waveform (type 3 >=v1.28)
     *   (games may use multiple versions and reference types)
     * - Waveforms are audio materials encoded in some format
     * - Waveforms are saved into .awb, that can be streamed (loaded manually) or internal,
     *   and has a checksum/hash to validate
     * - there is a version value (sometimes written incorrectly) and string,
     *   Atom Craft may only target certain .acb versions
     */

    ;VGM_LOG("ACB: find waveid=%i\n", waveid);

    acb.Header = utf_open(acbFile, 0x00, NULL, NULL);
    if (!acb.Header) goto done;

    acb.is_memory = is_memory;
    acb.has_TrackEventTable = utf_query_data(acbFile, acb.Header, 0, "TrackEventTable", NULL,NULL);
    acb.has_CommandTable = utf_query_data(acbFile, acb.Header, 0, "CommandTable", NULL,NULL);


    /* read CueName[i] */
    if (!load_utf_subtable(acbFile, &acb, &acb.CueNameTable, "CueNameTable", &CueName_rows)) goto done;
    ;VGM_LOG("ACB: CueNames=%i\n", CueName_rows);

    for (CueName_i = 0; CueName_i < CueName_rows; CueName_i++) {
        acb.CueNameIndex = CueName_i;

        if (!load_acb_cue_info(acbFile, &acb))
            goto done;

        /* meaning of Cue.ReferenceIndex */
        switch(acb.ReferenceType) {

            case 1: { /* Cue > Waveform (ex. PES 2015) */
                acb.ReferenceItem = acb.ReferenceIndex;

                if (!load_acb_waveform(acbFile, &acb, waveid))
                    goto done;

                if (acb.is_wave_found)
                    add_acb_name(acbFile, &acb);

                break;
            }

            case 2: { /* Cue > Synth > Waveform (ex. Ukiyo no Roushi) */
                acb.SynthIndex = acb.ReferenceIndex;

                if (!load_acb_synth(acbFile, &acb))
                    goto done;

                for (ReferenceItems_i = 0; ReferenceItems_i < acb.ReferenceItems_count; ReferenceItems_i++) {
                    acb.ReferenceItem = acb.ReferenceItems_list[ReferenceItems_i];

                    if (!load_acb_waveform(acbFile, &acb, waveid))
                        goto done;

                    if (acb.is_wave_found)
                        add_acb_name(acbFile, &acb);
                }

                break;
            }

            case 3: { /* Cue > Sequence > Track > Command > Synth > Waveform (ex. Valkyrie Profile anatomia, Yakuza Kiwami 2) */

                if (!load_acb_sequence(acbFile, &acb))
                    goto done;

                /* read Tracks inside Sequence */
                for (TrackIndex_i = 0; TrackIndex_i < acb.NumTracks; TrackIndex_i++) {
                    acb.TrackIndex = read_s16be(acb.TrackIndex_offset + TrackIndex_i*0x02, acbFile);

                    if (!load_acb_track_command(acbFile, &acb))
                        goto done;

                    for (SynthIndex_i = 0; SynthIndex_i < acb.SynthIndex_count; SynthIndex_i++) {
                        acb.SynthIndex = acb.SynthIndex_list[SynthIndex_i];

                        if (!load_acb_synth(acbFile, &acb))
                            goto done;

                        for (ReferenceItems_i = 0; ReferenceItems_i < acb.ReferenceItems_count; ReferenceItems_i++) {
                            acb.ReferenceItem = acb.ReferenceItems_list[ReferenceItems_i];

                            if (!load_acb_waveform(acbFile, &acb, waveid))
                                goto done;

                            if (acb.is_wave_found)
                                add_acb_name(acbFile, &acb);
                        }
                    }
                }

                break;
            }

            case 8: //todo found on Sonic Lost World (maybe not references wave?)
            default:
                VGM_LOG("ACB: unknown reference type\n");
                break;
        }

        //if (acb.AwbId_count > 0)
        //    break;
    }

    /* meh copy */
    if (acb.AwbName_count > 0) {
        strncpy(vgmstream->stream_name, acb.name, STREAM_NAME_SIZE);
        vgmstream->stream_name[STREAM_NAME_SIZE - 1] = '\0';
    }

done:
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

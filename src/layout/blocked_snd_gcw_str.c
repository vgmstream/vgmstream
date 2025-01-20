#include "layout.h" 
#include "../coding/coding.h" 
#include "../vgmstream.h" 
 
static bool check_blank_space_from_snd_gcw_str_block(STREAMFILE* sf, off_t offset); 
 
/* bigfile-adjacent dsp block, basically interleave layout with a "header" in the first block. */ 
void block_update_snd_gcw_str(off_t block_offset, VGMSTREAM* vgmstream) 
{ 
    snd_gcw_str_blocked_layout_data* data = NULL; 
    snd_gcw_str_first_block_header_info* info = NULL; 
 
    if (vgmstream->layout_data && vgmstream->coding_type == coding_NGC_DSP) 
    { 
        // locking up blocked layout code behind an memory-allocated pointer is not ideal 
        // but i couldn't see it being done any other way. 
        data = ((snd_gcw_str_blocked_layout_data*)vgmstream->layout_data); 
        if (data && data->finished_all_calcs) 
        { 
            vgmstream->current_block_offset = block_offset; 
            vgmstream->next_block_offset = block_offset + (data->block_size * data->channels); 
            if ((vgmstream->current_sample >= data->first_sample_threshold) 
                && 
                (vgmstream->current_sample < data->first_block_samples_threshold)) 
            { 
                vgmstream->current_block_size = data->first_block_size; 
                vgmstream->current_block_samples = data->first_block_samples; 
            } 
            else if ((vgmstream->current_sample >= data->last_block_samples_threshold) 
                && 
                (vgmstream->current_sample < data->last_sample_threshold)) 
            { 
                vgmstream->current_block_size = data->last_block_size; 
                vgmstream->current_block_samples = data->last_block_samples; 
            } 
            else 
            { 
                vgmstream->current_block_size = data->current_block_size; 
                vgmstream->current_block_samples = data->current_block_samples; 
            } 
 
            for (int i = 0; i < vgmstream->channels; i++) 
            { 
                if ((vgmstream->current_sample >= data->first_sample_threshold) 
                    && 
                    (vgmstream->current_sample < data->first_block_samples_threshold)) 
                { 
                    vgmstream->ch[i].offset = block_offset + ((data->first_block_header_size + data->first_block_size) * i); 
 
                    if (data->info[i]->exists 
                        && !data->info[i]->coef_parsed 
                        && !data->info[i]->has_blank_space) 
                    { 
                        info = data->info[i]; 
 
                        info->sf = vgmstream->ch[i].streamfile; 
                        info->coef_offset = vgmstream->ch[i].offset; 
 
                        dsp_read_coefs_separately_be(vgmstream, 
                            info->sf, 
                            info->coef_offset, i); 
                        info->coef_parsed = true; 
 
                        info->has_blank_space = check_blank_space_from_snd_gcw_str_block( 
                            info->sf, 
                            info->coef_offset); 
                    } 
 
                    vgmstream->ch[i].offset += data->first_block_header_size; 
                } 
                else if ((vgmstream->current_sample >= data->last_block_samples_threshold) 
                    && 
                    (vgmstream->current_sample < data->last_sample_threshold)) 
                { 
                    vgmstream->ch[i].offset = (data->blocks == 2) 
                        ? block_offset + (data->last_block_size * i) 
                        : block_offset + (data->current_block_size * i); 
                } 
                else 
                    vgmstream->ch[i].offset = block_offset + (data->current_block_size * i); 
            } 
        } 
    } 
} 
 
/* checks validity of a blank space (coming after coef, no less) from the header of the first block. */ 
bool check_blank_space_from_snd_gcw_str_block(STREAMFILE* sf, off_t offset) 
{ 
    int blank_space_offset = 0x20; 
    int blank_space_count = 0; 
 
    for (int i = 0; i < 16; i++) 
    { 
        if (!read_s16be((offset + blank_space_offset + (i * 2)), sf)) 
            blank_space_count++; 
    } 
 
    if (blank_space_count == 16) 
        return true; 
    else 
        return false; 
} 

CODING_OBJS=coding/adx_decoder.o \
    coding/g721_decoder.o \
    coding/ima_decoder.o \
    coding/ngc_afc_decoder.o \
    coding/ngc_dsp_decoder.o \
    coding/ngc_dtk_decoder.o \
    coding/pcm_decoder.o \
    coding/psx_decoder.o \
    coding/xa_decoder.o \
    coding/eaxa_decoder.o \
    coding/ogg_vorbis_decoder.o \
    coding/sdx2_decoder.o \
    coding/ws_decoder.o \
    coding/mpeg_decoder.o \
    coding/acm_decoder.o

LAYOUT_OBJS=layout/ast_blocked.o \
    layout/blocked.o \
    layout/halpst_blocked.o \
    layout/interleave.o \
    layout/nolayout.o \
    layout/xa_blocked.o \
    layout/caf_blocked.o \
    layout/ea_block.o \
    layout/wsi_blocked.o \
    layout/str_snds_blocked.o \
    layout/ws_aud_blocked.o \
    layout/interleave_byte.o \
	layout/mus_acm_layout.o

META_OBJS=meta/adx_header.o \
    meta/afc_header.o \
    meta/agsc.o \
    meta/ast.o \
    meta/brstm.o \
    meta/halpst.o \
    meta/nds_strm.o \
    meta/ngc_adpdtk.o \
    meta/rsf.o \
    meta/rs03.o \
    meta/ngc_dsp_std.o \
    meta/Cstr.o \
    meta/gcsw.o \
    meta/ps2_ads.o \
    meta/ps2_npsf.o \
    meta/rwsd.o \
    meta/psx_cdxa.o \
    meta/ps2_rxw.o \
    meta/ps2_int.o \
    meta/ps2_exst.o \
    meta/ps2_svag.o \
    meta/ps2_mib.o \
    meta/ps2_mic.o \
    meta/raw.o \
    meta/ps2_vag.o \
    meta/psx_gms.o \
    meta/ps2_str.o \
    meta/ps2_ild.o \
    meta/ps2_pnb.o \
    meta/xbox_wavm.o \
    meta/xbox_xwav.o \
    meta/ea_header.o \
    meta/ngc_caf.o \
    meta/ps2_vpk.o \
    meta/genh.o \
    meta/ogg_vorbis_file.o \
    meta/ps2_bmdx.o \
    meta/aifc.o \
    meta/str_snds.o \
    meta/ws_aud.o \
    meta/ahx.o \
    meta/ivb.o \
    meta/svs.o \
    meta/riff.o \
    meta/pos.o \
    meta/nwa.o \
    meta/ps2_rws.o \
    meta/ps2_hgc1.o \
    meta/xss.o \
    meta/ps2_sl3.o \
    meta/ps2_aus.o \
    meta/fsb.o \
    meta/rsd.o \
    meta/rwx.o \
    meta/xwb.o \
    meta/ea_old.o \
    meta/ps2_xa30.o \
    meta/musc.o \
    meta/musx.o \
    meta/ps2_leg.o \
    meta/ps2_filp.o \
    meta/ps2_ikm.o \
    meta/ps2_sfs.o \
    meta/sat_dvi.o \
    meta/ps2_bg00.o \
    meta/dc_kcey.o \
    meta/ps2_rstm.o \
    meta/acm.o \
    meta/mus_acm.o \
    meta/ps2_kces.o \
    meta/ps2_dxh.o \
    meta/ps2_psh.o \
    meta/sli.o \
    meta/sfl.o \
    meta/ps2_pcm.c \
    meta/ps2_psw.c \
    meta/ps2_rkv.c \
    meta/ps2_vas.c
		
OBJECTS=vgmstream.o streamfile.o util.o $(CODING_OBJS) $(LAYOUT_OBJS) $(META_OBJS)

libvgmstream.a: $(OBJECTS)
	$(AR) crs libvgmstream.a $(OBJECTS)

vgmstream-deps:
	$(CC) $(CFLAGS) -M -o vgmstream-deps

clean:
	rm -f $(OBJECTS) libvgmstream.a

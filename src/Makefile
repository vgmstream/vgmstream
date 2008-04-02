CODING_OBJS=coding/adx_decoder.o \
			coding/g721_decoder.o \
			coding/ima_decoder.o \
			coding/ngc_afc_decoder.o \
			coding/ngc_dsp_decoder.o \
			coding/ngc_dtk_decoder.o \
			coding/pcm_decoder.o

LAYOUT_OBJS=layout/ast_blocked.o \
			layout/blocked.o \
			layout/halpst_blocked.o \
			layout/interleave.o \
			layout/nolayout.o

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
		  meta/gcsw.o

OBJECTS=vgmstream.o streamfile.o util.o $(CODING_OBJS) $(LAYOUT_OBJS) $(META_OBJS)

libvgmstream.a: $(OBJECTS)
	$(AR) crs libvgmstream.a $(OBJECTS)

vgmstream-deps:
	$(CC) $(CFLAGS) -M -o vgmstream-deps

clean:
	rm -f $(OBJECTS) libvgmstream.a

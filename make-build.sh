#!/bin/sh

# example script that builds vgmstream with most libs enabled using the basic makefiles
# (needs some manual fiddling)
# most libs use system libs if possible, but should be possible to download them, compile, and
# pass INCS/LIBSs that use static libs

# todo more testing
# - don't use ext_libs includes for linux (uses system libs)
# - libcelt 0.6 works but 11.0 gives "Floating point exception (core dumped)" (VM?)
exit

# config for make
INCS=
LIBS=
FLAGS=

###############################################################################
# base deps
sudo apt-get install -y gcc g++ make build-essential git


###############################################################################
# vgmstream123 deps
sudo apt-get install -y libao-dev


###############################################################################
# vorbis deps
sudo apt-get install -y libvorbis-dev
FLAGS+=" VGM_VORBIS=1"


###############################################################################
# mpeg deps
sudo apt-get install -y libmpg123-dev
FLAGS+=" VGM_MPEG=1"


###############################################################################
# speex deps
sudo apt-get install -y libspeex-dev
FLAGS+=" VGM_SPEEX=1"


###############################################################################
# ffmpeg deps
sudo apt-get install -y libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
FLAGS+=" VGM_FFMPEG=1"


###############################################################################
# atrac9 deps (compile)
mkdir dependencies
cd dependencies
git clone https://github.com/Thealexbarney/LibAtrac9
cd LibAtrac9/C
make
cd ../../../

FLAGS+=" VGM_ATRAC9=2"
#INCS+=" -I../dependencies/LibAtrac9/C/src"
LIBS+=" -L../dependencies/LibAtrac9/C/bin"


###############################################################################
# celt deps (compile x2)
mkdir dependencies
cd dependencies

# used renames followed by unused renamed (but seems needed to avoid clashes)
CELT0061_RENAMES=" \
    -Dcelt_decode=celt_0061_decode \
    -Dcelt_decoder_create=celt_0061_decoder_create \
    -Dcelt_decoder_destroy=celt_0061_decoder_destroy \
    -Dcelt_mode_create=celt_0061_mode_create \
    -Dcelt_mode_destroy=celt_0061_mode_destroy \
    -Dcelt_mode_info=celt_0061_mode_info \
    \
    -Dalg_quant=alg_quant_0061 \
    -Dalg_unquant=alg_unquant_0061 \
    -Dcelt_decoder_create_custom=celt_decoder_create_custom_0061 \
    -Dcelt_encoder_destroy=celt_encoder_destroy_0061 \
    -Dcelt_encoder_create=celt_encoder_create_0061 \
    -Dcelt_encode=celt_encode_0061 \
    -Dcelt_encode_float=celt_encode_float_0061 \
    -Dcelt_encoder_ctl=celt_encoder_ctl_0061 \
    -Dcelt_decode_float=celt_decode_float_0061 \
    -Dcelt_decoder_ctl=celt_decoder_ctl_0061 \
    -Dcompute_allocation=compute_allocation_0061 \
    -Dcompute_band_energies=compute_band_energies_0061 \
    -Ddenormalise_bands=denormalise_bands_0061 \
    -Dec_dec_init=ec_dec_init_0061 \
    -Dec_decode=ec_decode_0061 \
    -Dec_decode_bin=ec_decode_bin_0061 \
    -Dec_dec_update=ec_dec_update_0061 \
    -Dec_dec_uint=ec_dec_uint_0061 \
    -Dec_dec_bits=ec_dec_bits_0061 \
    -Dec_enc_init=ec_enc_init_0061 \
    -Dec_encode=ec_encode_0061 \
    -Dec_encode_bin=ec_encode_bin_0061 \
    -Dec_enc_uint=ec_enc_uint_0061 \
    -Dec_enc_bits=ec_enc_bits_0061 \
    -Dec_enc_done=ec_enc_done_0061 \
    -Dnormalise_bands=normalise_bands_0061 \
    -Drenormalise_vector=renormalise_vector_0061 \
    -Dquant_coarse_energy=quant_coarse_energy_0061 \
    -Dquant_fine_energy=quant_fine_energy_0061 \
    -Dquant_energy_finalise=quant_energy_finalise_0061 \
    -Dunquant_coarse_energy=unquant_coarse_energy_0061 \
    -Dunquant_energy_finalise=unquant_energy_finalise_0061 \
    -Dunquant_fine_energy=unquant_fine_energy_0061 \
    "
# same as the above but I don't know sh enough to normalize
CELT0110_RENAMES=" \
    -Dcelt_decode=celt_0110_decode \
    -Dcelt_decoder_create_custom=celt_0110_decoder_create_custom \
    -Dcelt_decoder_destroy=celt_0110_decoder_destroy \
    -Dcelt_mode_create=celt_0110_mode_create \
    -Dcelt_mode_destroy=celt_0110_mode_destroy \
    -Dcelt_mode_info=celt_0110_mode_info \
    \
    -Dalg_quant=alg_quant_0110 \
    -Dalg_unquant=alg_unquant_0110 \
    -Dcelt_encoder_destroy=celt_encoder_destroy_0110 \
    -Dcelt_encoder_create=celt_encoder_create_0110 \
    -Dcelt_encode=celt_encode_0110 \
    -Dcelt_encode_float=celt_encode_float_0110 \
    -Dcelt_encoder_ctl=celt_encoder_ctl_0110 \
    -Dcelt_decode_float=celt_decode_float_0110 \
    -Dcelt_decoder_ctl=celt_decoder_ctl_0110 \
    -Dcompute_allocation=compute_allocation_0110 \
    -Dcompute_band_energies=compute_band_energies_0110 \
    -Ddenormalise_bands=denormalise_bands_0110 \
    -Dec_dec_init=ec_dec_init_0110 \
    -Dec_decode=ec_decode_0110 \
    -Dec_decode_bin=ec_decode_bin_0110 \
    -Dec_dec_update=ec_dec_update_0110 \
    -Dec_dec_uint=ec_dec_uint_0110 \
    -Dec_dec_bits=ec_dec_bits_0110 \
    -Dec_enc_init=ec_enc_init_0110 \
    -Dec_encode=ec_encode_0110 \
    -Dec_encode_bin=ec_encode_bin_0110 \
    -Dec_enc_uint=ec_enc_uint_0110 \
    -Dec_enc_bits=ec_enc_bits_0110 \
    -Dec_enc_done=ec_enc_done_0110 \
    -Dnormalise_bands=normalise_bands_0110 \
    -Drenormalise_vector=renormalise_vector_0110 \
    -Dquant_coarse_energy=quant_coarse_energy_0110 \
    -Dquant_fine_energy=quant_fine_energy_0110 \
    -Dquant_energy_finalise=quant_energy_finalise_0110 \
    -Dunquant_coarse_energy=unquant_coarse_energy_0110 \
    -Dunquant_energy_finalise=unquant_energy_finalise_0110 \
    -Dunquant_fine_energy=unquant_fine_energy_0110 \
    "


git clone --depth 1 --branch v0.6.1 https://gitlab.xiph.org/xiph/celt.git celt-0061
cd celt-0061
./autogen.sh
./configure
make LDFLAGS="-no-undefined" AM_CFLAGS="$CELT0061_RENAMES"
mv ./libcelt/.libs/libcelt.a ./libcelt/.libs/libcelt-0061.a
cd ..

git clone --depth 1 --branch v0.11 https://gitlab.xiph.org/xiph/celt.git celt-0110
cd celt-0110
./autogen.sh
./configure
make LDFLAGS="-no-undefined" AM_CFLAGS="-DCUSTOM_MODES=1 $CELT0110_RENAMES"
mv ./libcelt/.libs/libcelt0.a ./libcelt/.libs/libcelt-0110.a

cd ..

cd ..

FLAGS+="VGM_CELT=2"
#INCS+=" -I../dependencies/celt-0061/libcelt/.libs/"
LIBS+=" -L../dependencies/celt-0061/libcelt/.libs/ -L../dependencies/celt-0110/libcelt/.libs/"


###############################################################################
# vgmstream
make vgmstream_cli $FLAGS EXTRA_CFLAGS=$INCS EXTRA_LDFLAGS=$LIBS
make vgmstream123 $FLAGS EXTRA_CFLAGS=$INCS EXTRA_LDFLAGS=$LIBS

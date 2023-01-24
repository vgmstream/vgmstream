if(NOT WIN32 AND USE_CELT)
	FetchDependency(CELT_0061
		DIR celt-0061
		FETCH_PRIORITY file git
		
		FILE_DOWNLOAD https://downloads.xiph.org/releases/celt/celt-0.6.1.tar.gz
		FILE_SUBDIR celt-0.6.1
		
		GIT_REPOSITORY https://gitlab.xiph.org/xiph/celt
		GIT_TAG v0.6.1
	)
	FetchDependency(CELT_0110
		DIR celt-0110
		FETCH_PRIORITY file git
		
		FILE_DOWNLOAD https://downloads.xiph.org/releases/celt/celt-0.11.0.tar.gz
		FILE_SUBDIR celt-0.11.0
		
		GIT_REPOSITORY https://gitlab.xiph.org/xiph/celt
		GIT_TAG v0.11
	)
	if(CELT_0061_PATH AND CELT_0110_PATH)
		set(CELT_0061_LINK_PATH ${CELT_0061_BIN}/libcelt/.libs/libcelt.a)
		set(CELT_0110_LINK_PATH ${CELT_0110_BIN}/libcelt/.libs/libcelt0.a)
		
		configure_file(
			${VGM_SOURCE_DIR}/ext_libs/extra/celt-0110/ecintrin.h
			${CELT_0110_PATH}/libcelt/ecintrin.h
		COPYONLY)
		
		set(CELT_CFLAGS
			alg_quant
			alg_unquant
			celt_decode
			celt_decoder_create
			celt_decoder_create_custom
			celt_decoder_destroy
			celt_mode_create
			celt_mode_destroy
			celt_mode_info
			celt_encoder_destroy
			celt_encoder_create
			celt_encode
			celt_encode_float
			celt_encoder_ctl
			celt_decode_float
			celt_decoder_ctl
			compute_allocation
			compute_band_energies
			denormalise_bands
			ec_dec_init
			ec_decode
			ec_decode_bin
			ec_dec_update
			ec_dec_uint
			ec_dec_bits
			ec_enc_init
			ec_encode
			ec_encode_bin
			ec_enc_uint
			ec_enc_bits
			ec_enc_done
			normalise_bands
			renormalise_vector
			quant_coarse_energy
			quant_fine_energy
			quant_energy_finalise
			unquant_coarse_energy
			unquant_energy_finalise
			unquant_fine_energy
			eMeans
			encode_pulses
			decode_pulses
			ec_laplace_encode
			ec_laplace_decode
			celt_decoder_get_size
			celt_decoder_init
			ec_dec_bit_logp
			ec_dec_icdf
			ec_enc_bit_logp
			ec_enc_icdf
			ec_enc_shrink
			_celt_lpc
			_celt_autocorr
			amp2Log2
			stereo_itheta
			anti_collapse
			spreading_decision
			haar1
			quant_all_bands
			log2_frac
			icwrs
			get_required_bits
			ec_ilog
			mdct_forward
			mdct_backward
			mdct_init
			mdct_clear
			clt_mdct_init
			clt_mdct_clear
			compute_pulse_cache
		)
		
		foreach(ver 0061 0110)
			foreach(source ${CELT_CFLAGS})
				list(APPEND CELT_${ver}_CFLAGS "-D${source}=${source}_${ver}")
			endforeach()
			list(APPEND CELT_${ver}_CFLAGS "-fPIC")
			
			set(CELT_${ver}_CONF
				--enable-static
				--disable-shared
				--disable-oggtest
				CC="${CMAKE_C_COMPILER}"
				AR="${CMAKE_AR}"
				CFLAGS="${CELT_${ver}_CFLAGS}"
			)
			if(ver STREQUAL "0110")
				list(APPEND CELT_${ver}_CONF --enable-custom-modes)
			endif()
			
			if(NOT EXISTS ${CELT_${ver}_PATH}/configure)
				add_custom_target(CELT_${ver}_AUTOGEN
					COMMAND ./autogen.sh
					BYPRODUCTS ${CELT_${ver}_PATH}/configure
					WORKING_DIRECTORY ${CELT_${ver}_PATH}
				)
			endif()
			
			file(MAKE_DIRECTORY ${CELT_${ver}_BIN})
			add_custom_target(CELT_${ver}_CONFIGURE
				COMMAND "${CELT_${ver}_PATH}/configure" ${CELT_${ver}_CONF}
				DEPENDS ${CELT_${ver}_PATH}/configure
				BYPRODUCTS ${CELT_${ver}_BIN}/Makefile
				WORKING_DIRECTORY ${CELT_${ver}_BIN}
			)
			add_custom_target(CELT_${ver}_MAKE
				COMMAND make SUBDIRS=libcelt DIST_SUBDIRS=libcelt
				DEPENDS ${CELT_${ver}_BIN}/Makefile
				BYPRODUCTS ${CELT_${ver}_LINK_PATH} ${CELT_${ver}_BIN}
				WORKING_DIRECTORY ${CELT_${ver}_BIN}
			)
			
			add_library(celt${ver} STATIC IMPORTED)
			if(NOT EXISTS ${CELT_${ver}_LINK_PATH})
				add_dependencies(celt${ver} CELT_${ver}_MAKE)
			endif()
			set_target_properties(celt${ver} PROPERTIES
				IMPORTED_LOCATION ${CELT_${ver}_LINK_PATH}
			)
		endforeach()
	else()
		set(USE_CELT OFF)
	endif()
endif()
if(USE_CELT)
	if(CELT_0061_SOURCE STREQUAL "${CELT_0110_SOURCE}")
		set(CELT_SOURCE ${CELT_0061_SOURCE})
	else()
		set(CELT_SOURCE "v0.6.1: ${CELT_0061_SOURCE}, v0.11.0: ${CELT_0110_SOURCE}")
	endif()
else()
	unset(CELT_SOURCE)
endif()

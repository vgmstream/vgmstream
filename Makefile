###############################
# vgmstream makefile
###############################

### defs
# currently aimed to WIN32 builds but vgmstream_cli should work for others (or use autotools instead)
export TARGET_OS = $(OS)
 

### tools
RMF = rm -f

ifeq ($(TARGET_OS),Windows_NT)
  SHELL = sh
  CC = gcc
  AR = ar
  STRIP = strip
  WINDRES = windres
  DLLTOOL = dlltool

  # same thing, the above should be available
  #CC = i686-w64-mingw32-gcc
  #AR = i686-w64-mingw32-ar
  #STRIP = i686-w64-mingw32-strip
  #WINDRES = i686-w64-mingw32-windres
  #DLLTOOL = i686-w64-mingw32-dlltool

else
  SHELL = /bin/sh
  CC = gcc
  AR = ar
  STRIP = strip
  WINDRES =
  DLLTOOL =

  # (old crosscompile, not used anymore?)
  #CC = i586-mingw32msvc-gcc
  #AR = i586-mingw32msvc-ar
  #STRIP = i586-mingw32msvc-strip
  #WINDRES = i586-mingw32msvc-windres
  #DLLTOOL = i586-mingw32msvc-dlltool

endif

export RMF SHELL CC AR STRIP WINDRES DLLTOOL


### targets

buildrelease: clean bin

buildfullrelease: clean sourceball bin

sourceball:
	rm -rf vgmstream-`./version.sh`
	git checkout-index -f -a --prefix=vgmstream-`./version.sh`/
#	git archive --format zip --output vgmstream-`./version.sh`.zip master
	echo "#!/bin/sh" > vgmstream-`./version.sh`/version.sh
	echo "echo \"`./version.sh`\"" >> vgmstream-`./version.sh`/version.sh
	tar cvzf "vgmstream-`./version.sh`.tar.gz" vgmstream-`./version.sh`/*
	rm -rf vgmstream-`./version.sh`

bin mingwbin: vgmstream_cli winamp xmplay
	zip -FS -j "vgmstream-`./version.sh`-test.zip" COPYING README.md cli/test.exe winamp/in_vgmstream.dll xmplay/xmp-vgmstream.dll ext_libs/*.dll

vgmstream_cli mingw_test:
	$(MAKE) -C cli vgmstream_cli

vgmstream123:
	$(MAKE) -C cli vgmstream123

winamp mingw_winamp:
	$(MAKE) -C winamp in_vgmstream

xmplay mingw_xmplay:
	$(MAKE) -C xmplay xmp_vgmstream

clean:
	$(RMF) vgmstream-*.zip
	$(MAKE) -C src clean
	$(MAKE) -C cli clean
	$(MAKE) -C winamp clean
	$(MAKE) -C xmplay clean
	$(MAKE) -C ext_libs clean

.PHONY: clean buildfullrelease buildrelease sourceball bin vgmstream_cli winamp xmplay mingwbin mingw_test mingw_winamp mingw_xmplay

#deprecated: buildfullrelease sourceball mingwbin mingw_test mingw_winamp mingw_xmplay

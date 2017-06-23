.PHONY: buildfullrelease buildrelease mingw_test mingw_winamp mingw_xmplay sourceball mingwbin

buildfullrelease: clean sourceball mingwbin

buildrelease: clean mingwbin

sourceball:
	rm -rf vgmstream-`./version.sh`
	git checkout-index -f -a --prefix=vgmstream-`./version.sh`/
#	git archive --format zip --output vgmstream-`./version.sh`.zip master
	echo "#!/bin/sh" > vgmstream-`./version.sh`/version.sh
	echo "echo \"`./version.sh`\"" >> vgmstream-`./version.sh`/version.sh
	tar cvzf "vgmstream-`./version.sh`.tar.gz" vgmstream-`./version.sh`/*
	rm -rf vgmstream-`./version.sh`

mingwbin: mingw_test mingw_winamp mingw_xmplay
	zip -FS -j "vgmstream-`./version.sh`-test.zip" COPYING readme.txt test/test.exe winamp/in_vgmstream.dll xmplay/xmp-vgmstream.dll ext_libs/*.dll

mingw_test:
	$(MAKE) -C test -f Makefile.mingw test.exe

mingw_winamp:
	$(MAKE) -C winamp in_vgmstream.dll

mingw_xmplay:
	$(MAKE) -C xmplay xmp-vgmstream.dll

clean:
	rm -f vgmstream-*.zip
	$(MAKE) -C src clean
	$(MAKE) -C test clean
	$(MAKE) -C test -f Makefile.mingw clean
	$(MAKE) -C winamp clean
	$(MAKE) -C xmplay clean
	$(MAKE) -C ext_libs -f Makefile.mingw clean

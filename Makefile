.PHONY: buildfullrelease buildrelease mingw_test mingw_winamp sourceball mingwbin

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

mingwbin: mingw_test mingw_winamp
	zip -FS -j "vgmstream-`./version.sh`-test.zip" COPYING readme.txt test/test.exe winamp/in_vgmstream.dll ext_libs/*.dll

mingw_test:
	$(MAKE) -C test -f Makefile.mingw test.exe

mingw_winamp:
	$(MAKE) -C winamp in_vgmstream.dll

clean:
	rm -rf vgmstream-*
	$(MAKE) -C test clean
	$(MAKE) -C test -f Makefile.mingw clean
	$(MAKE) -C winamp clean
	$(MAKE) -C src clean
	$(MAKE) -C ext_libs -f Makefile.mingw clean

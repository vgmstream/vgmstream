.PHONY: buildrelease mingw_test mingw_winamp sourceball mingwbin

buildrelease: clean sourceball mingwbin

sourceball:
	rm -rf vgmstream.export
	svn export . vgmstream.export
	tar cvzf "vgmstream-`./version.sh`.tar.gz" vgmstream.export/*
	rm -rf vgmstream.export

mingwbin: mingw_test mingw_winamp
	zip -j "vgmstream-`./version.sh`-test.zip" readme.txt LICENSE test/test.exe winamp/in_vgmstream.dll 

mingw_test:
	$(MAKE) -C test -f Makefile.mingw test.exe

mingw_winamp:
	$(MAKE) -C winamp in_vgmstream.dll

clean:
	rm -f vgmstream-*.tar.gz
	rm -f vgmstream-*-test.zip
	$(MAKE) -C test clean
	$(MAKE) -C test -f Makefile.mingw clean
	$(MAKE) -C winamp clean
	$(MAKE) -C src clean

.PHONY: buildrelease mingw_test mingw_winamp

buildrelease: clean vgmstream.tar.gz vgmstream-test.zip

vgmstream.tar.gz:
	tar cvzf vgmstream.tar.gz readme.txt LICENSE Makefile src test winamp

vgmstream-test.zip: mingw_test mingw_winamp
	zip -j vgmstream-test.zip readme.txt LICENSE test/test.exe winamp/in_vgmstream.dll 

mingw_test:
	$(MAKE) -C test -f Makefile.mingw test.exe

mingw_winamp:
	$(MAKE) -C winamp in_vgmstream.dll

clean:
	rm -f vgmstream.tar.gz
	rm -f vgmstream-test.zip
	$(MAKE) -C test clean
	$(MAKE) -C test -f Makefile.mingw clean
	$(MAKE) -C winamp clean
	$(MAKE) -C src clean

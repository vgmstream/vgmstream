#
# external libs
#

# don't barf in case it's target "clean"
#ifneq ($(TARGET_OS),Windows_NT)
#$(error option must be built with TARGET_OS = Windows_NT)
#endif

# generate linker libs for Mingw's GCC (usually named *.dll.a on Windows), using:
# 	dlltool [-D (lib).dll] -d (lib).def -l (lib).dll.a
# -D is optional, but needed if DLL was renamed (like libcelts)

# patterns match ($@ = target, $< = dependency, for others substitute)
%.dll.a:
	$(DLLTOOL) -D $(DLL_DIR)/$(patsubst %.dll.a,%.dll,$@) -d $(patsubst %.dll.a,%.def,$@) -l $(DLL_DIR)/$@

clean:
	$(RMF) $(DLL_DIR)/*.dll.a

.PHONY: clean

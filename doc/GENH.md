# GENH FORMAT

GENH is a generic binary header with fixed values, to make unsupported files playable. This binary header is appended to the beginning of a file and file is renamed to .genh, so vgmstream will read the GENH header and play the file with the new info.

GENH as been mostly superseded by TXTH, as it can do anything that GENH does and more, plus it's cleaner and much simpler to create, so TXTH is the recommended way to make vgmstream play unsupported files.

If you still want to create files with GENH headers, the easiest way is to use VGMToolBox's GENH creator, that provides a simple Windows interface.

For programmers looking for a formal definition the place to check would be vgmstream's parser, located in `genh.c` (particularly `parse_genh`), as new features or fixes may be added anytime.

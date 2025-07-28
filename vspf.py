# Visual Studio project fixer
#
# VS updates automatically project files every time a new source file is added, but they are
# a pain to maintain if aren't using VS. CL compiler supports wildcards in .vcxprojs, but VS
# doesn't and won't update the file list (and probably causes other issues). Meanwhile CMake
# creates ugly stuff with full paths that can't be added to SCC, meaning new/old users need
# to call cmake every time stuff changes around. So for now, this tool updates the expected
# file list so MS is happy and I don't have to babysit this crap.
#
# Reads/writes lines as to keep original structure (python's XML functions re-create the document).
# It's brittle though, and only works with the known structure used here.

import os, glob

TEST_OUTPUT = False
BASE_SRC_PATH = '.'

class ProjectFixer:

    def __init__(self, prj_pathname):
        self.prj_pathname = prj_pathname
        self.prj_path = os.path.dirname(prj_pathname)
        #self.prj_name = os.path.basename(prj_pathname)
        self.in_lines = []
        self.out_lines = []
        self.is_filters = False

        if prj_pathname.lower().endswith('.vcxproj'):
            self.is_filters = False
        elif prj_pathname.lower().endswith('.vcxproj.filters'):
            self.is_filters = True
        else:
            raise ValueError("unsupported file")

    def is_changed(self):
        # no change in src means output files should be the exact same as input 
        return self.in_lines != self.out_lines


    def read(self):
        with open (self.prj_pathname, 'r', encoding='utf-8-sig') as f:
            lines = f.readlines()
            self.in_lines = [line.strip('\r\n') for line in lines]

    def add(self, text):
            self.out_lines.append(text)

    def get_files(self, ext):
        files = glob.glob(self.prj_path + '/**/*.' + ext, recursive=True)

        items = []
        for file in files:
            basefile = file[len(self.prj_path) + 1 : ].replace('/', '\\')
            path = ''
            if '\\' in basefile:
                pos = basefile.rindex('\\')
                path = basefile[0 : pos + 1]
            basename = os.path.basename(basefile)
            if basename.startswith('.'): #hidden .c
                continue

            items.append( (basefile, path) )
        return items


    def write_section(self, is_includes):
        if is_includes:
            ext = 'h'
            tpls = [
                '    <ClInclude Include="%s">',
                '      <Filter>%sHeader Files</Filter>',
                '    </ClInclude>',
                '    <ClInclude Include="%s" />',
            ]
        else:
            ext = 'c'
            tpls = [
                '    <ClCompile Include="%s">',
                '      <Filter>%sSource Files</Filter>',
                '    </ClCompile>',
                '    <ClCompile Include="%s" />',
            ]
        self.write_section_internal(ext, tpls)

    def write_section_internal(self, ext, tpls):
        files = self.get_files(ext)
        for basefile, path in files:
            if self.is_filters:
                self.add(tpls[0] % (basefile))
                self.add(tpls[1] % (path))
                self.add(tpls[2])
            else:
                self.add(tpls[3] % (basefile))


    # - VS projects are organized in sections 
    # - writes lines if not target section
    # - when target section found (includes or compiles), redo section's files again
    def process(self):

        lines_itr = iter(self.in_lines)
        for line in lines_itr:
            self.add(line)

            if '<ItemGroup' not in line:
                continue

            # section start
            next_line = next(lines_itr)

            is_includes = '<ClInclude' in next_line
            is_compiles = '<ClCompile' in next_line

            if is_includes or is_compiles:
                # include/compile section
                self.write_section(is_includes)

                # consume rest of files until section end
                for subline in lines_itr:
                    if '</ItemGroup' in subline:
                        self.add(subline)
                        break
            else:
                # other type
                self.add(next_line)

        #print("done", len(self.out_lines))

    def write(self):
        if not self.is_changed():
            print("no changes detected for %s" % (self.prj_pathname))
            return

        out_name = self.prj_pathname
        if TEST_OUTPUT:
            out_name += '.test'
        print("writting " + out_name)


        with open(out_name, 'w', newline='\r\n', encoding='utf-8-sig') as f:
            #f.write(codecs.BOM_UTF8)
            f.write('\n'.join(self.out_lines))

    def start(self):
        self.read()
        self.process()
        self.write()


def main():
    types = [
        './src/*.vcxproj',
        './src/*.vcxproj.filters',
        #'./cli/*.vcxproj',
        #'./cli/*.vcxproj.filters',
        './winamp/*.vcxproj',
        './winamp/*.vcxproj.filters',
        './xmplay/*.vcxproj',
        './xmplay/*.vcxproj.filters',
    ]

    for type in types:
        files = glob.glob(type, recursive=True)
        for file in files:
            print('fixing ' + file)
            ProjectFixer(file).start()

main()

#ProjectFixer('libvgmstream.vcxproj').start()
#ProjectFixer('libvgmstream.vcxproj.filters').start()

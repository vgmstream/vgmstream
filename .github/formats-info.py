# Generates formats-info.md based on src/meta code, to update FORMATS.md.
# A bit crummy and all but whatevs.

import glob, os, re

#TODO maybe sort by meta order list in vgmstream.c
#TODO fix ??? descriptions
#TODO fix some "ext + ext" exts
#TODO improve some ubi formats
#TODO detect subfile/init_x calls that aren't VGMSTREAM * init_
#TODO detect MPEG_x, OPUS_x subtypes
#TODO ignore some check extensions
#TODO include common idstrings (first is_id32be() / is_id32le / get_id32be / get_id32be) 
#     > after first valid init?

META_FILES = '../**/meta/*.c'
FORMAT_FILES = '../**/formats.c'
IS_PRINT_INFO = False #false = write

#VGMSTREAM * init_vgmstream_9tav(STREAMFILE *sf) #allow static since it's used in some cases
RE_INIT = re.compile(r"VGMSTREAM[ ]*\*[ ]*init_vgmstream_([A-Za-z0-9_]+)\(.+\)")
RE_INIT_STATIC = re.compile(r"static VGMSTREAM[ ]*\*[ ]*init_vgmstream_([A-Za-z0-9_]+)\(.+\)")

RE_CODING = re.compile(r"coding_([A-Z0-9][A-Za-z0-9_]+)")

RE_META = re.compile(r"meta_([A-Z0-9][A-Za-z0-9_]+)")

#sf_dat = open_streamfile_by_ext(sf, "dat");
RE_COMPANION_EXT = re.compile(r"open_streamfile_by_ext\(.+,[ ]*\"(.+)\"[ ]*\)")

# a = init_...
RE_SUBFILES = re.compile(r"^(?!VGMSTREAM).*init_vgmstream_([A-Za-z0-9_]+)[(;]")
RE_COMPANION_FILE = re.compile(r"open_streamfile_by_filename\(.+,[ ]*(.+)?\)")

# if (!check_extensions(sf,"..."))
# if (check_extensions(sf,"...")) { ... } else goto fail;
RE_NEW_EXTCHECK = re.compile(r"\([ ]*[!]*check_extensions\([ ]*.+?,[ ]*\"(.+)\"[ ]*\)")
#if (strcasecmp("bg00",filename_extension(filename))) goto fail;
RE_OLD_EXTCHECK = re.compile(r"if[ ]*\(strcasecmp\([ ]*\"(.+)\"[ ]*,filename_extension")

# formats.c extract
RE_FORMATS_META = re.compile(r"{meta_([A-Z0-9][A-Za-z0-9_]+)[ ]*,[ ]* \"(.+)\"[ ]*}")

FILES_SKIP = [
    'txth.c','txtp.c','genh.c', 
    'silence.c', 'mp4_faac.c', 'deblock_streamfile.c', 
    'ps_headerless.c', 'zwdsp.c', 'ngc_bh2pcm.c',
]

EXT_RENAMES = {'...': '(any)', '': '(extensionless)'}
INIT_IGNORES = ['default', 'main']
META_IGNORES = ['meta_type', 'meta_t']
CODING_IGNORES = ['coding_type', 'coding_t', 'FFmpeg', 'SILENCE']
SUBFILES_IGNORES = ['subkey', 'silence', 'silence_container']


# various codecs that 
INIT_CODECS = {
    re.compile(r'init_ffmpeg_aac\('): 'AAC',
    re.compile(r'init_ffmpeg_xwma\('): 'XWMA',
    re.compile(r'init_ffmpeg_atrac3plus_.+\('): 'ATRAC3PLUS',
    re.compile(r'init_ffmpeg_atrac3_.+\('): 'ATRAC3',
    re.compile(r'init_ffmpeg_xma_.+\('): 'XMA',
    re.compile(r'init_ffmpeg_xma1_.+\('): 'XMA1',
    re.compile(r'init_ffmpeg_xma2_.+\('): 'XMA2',
    re.compile(r'init_mpeg_custom\('): 'MPEG',
    re.compile(r'init_ffmpeg_mp4_.+\('): 'MP4/AAC',
    re.compile(r'init_ffmpeg_offset\('): 'FFmpeg(various)',
    re.compile(r'init_ffmpeg_header_offset\('): 'FFmpeg(various)',
    re.compile(r'init_ffmpeg_header_offset_subsong\('): 'FFmpeg(various)',
    re.compile(r'init_ffmpeg_.+?_opus.*\('): 'Opus',
    re.compile(r'init_vgmstream_ogg_vorbis_config\('): 'OGG',
}

class MetaInit:
    def __init__(self, name):
        self.name = name
        self.subfiles = []
        self.exts = []
        self.companions = []
        #self.metas = []
    
    def is_used(self):
        return self.subfiles or self.exts or self.companions

class MetaFile:
    def __init__(self, file):
        self.fileinfo = os.path.basename(file)
        # divide each file into sub-metas since some .c have a bunch of them
        self.curr = MetaInit('default') #shouldn't be used but...
        self.inits = [self.curr]
        self.metas = []
        self.codings = []

    def add_inits(self, elems):
        if len(elems) > 1:
            raise ValueError("multiinit?")

        for elem in elems:
            self.curr = MetaInit(elem)
            self.inits.append(self.curr)

    def _add(self, new_items, items, renames={}):
        if not new_items:
            return

        for new_item in new_items:
            if new_item in renames:
                new_item = renames[new_item]
            if new_item not in items:
                items.append(new_item)

    #def add_inits(self, elems):
    #    self._add(elems, self.inits)

    def add_codings(self, codings):
        self._add(codings, self.codings)

    def add_exts(self, exts, renames={}):
        self._add(exts, self.curr.exts, renames)

    def add_metas(self, metas):
        self._add(metas, self.metas)

    def add_companions(self, elems):
        self._add(elems, self.curr.companions)

    def add_subfiles(self, elems):
        self._add(elems, self.curr.subfiles)

class App:
    def __init__(self):
        self.infos = []
        self.desc_metas = {}

    def extract_extensions(self, line):
       
        exts = RE_NEW_EXTCHECK.findall(line)
        if not exts:
            exts = RE_OLD_EXTCHECK.findall(line)
        if not exts:
            return
        exts_str = ','.join(exts)
        return exts_str.split(',')

    def extract_regex(self, line, regex, ignores=[]):
        items = []
        results = regex.findall(line)
        for result in results:
            if result in ignores:
                continue
            items.append(result)
        return items
        
    def extract_codecs(self, line):
        for regex, codec in INIT_CODECS.items():
            if regex.search(line):
                return [codec]

    def parse_files(self):
        infos = self.infos

        files = glob.glob(META_FILES, recursive=True)
        for file in files:
            info = MetaFile(file)
            with open(file, 'r', encoding='utf-8') as f:
                internals = []
                for line in f:
                    # use to ignore subcalls to internal functions
                    items = self.extract_regex(line, RE_INIT_STATIC)
                    if items:
                        internals += items

                    items = self.extract_regex(line, RE_INIT)
                    info.add_inits(items)

                    items = self.extract_extensions(line)
                    info.add_exts(items, EXT_RENAMES)

                    items = self.extract_regex(line, RE_META, META_IGNORES)
                    info.add_metas(items)

                    items = self.extract_regex(line, RE_CODING, CODING_IGNORES)
                    info.add_codings(items)

                    items = self.extract_regex(line, RE_COMPANION_EXT)
                    info.add_companions(items)

                    items = self.extract_regex(line, RE_COMPANION_FILE)
                    if items:
                        info.add_companions(['(external)'])

                    items = self.extract_regex(line, RE_SUBFILES, SUBFILES_IGNORES)
                    if not any(x in internals for x in items):
                        info.add_subfiles(items)

                    items = self.extract_codecs(line)
                    info.add_codings(items)

            infos.append(info)

    def parse_formats(self):
        desc_metas = self.desc_metas
        files = glob.glob(FORMAT_FILES, recursive=True)
        for file in files:
            with open(file, 'r', encoding='utf-8') as f:
                for line in f:
                    items = self.extract_regex(line, RE_FORMATS_META)
                    if not items or len(items[0]) != 2:
                        continue
                    meta, desc = items[0]
                    desc_metas[meta] = desc
            

    def print(self):
        desc_metas = self.desc_metas
        infos = self.infos
        
        lines = []
        for info in infos:
            #info.sort()
            if info.fileinfo in FILES_SKIP:
                continue

            fileinfo = '- **%s**' % (info.fileinfo)
            #if info.metas:
            #    metas = ' '.join(info.metas)
            #    fileinfo += " (%s)" % (metas)
            lines.append(fileinfo)


            if info.metas:
                for meta in info.metas:
                    desc = desc_metas.get(meta, '???')
                    metainfo = "%s [*%s*]" % (desc, meta)
                    lines.append('  - ' + metainfo)
            else:
                lines.append('  - (container)')

            for init in info.inits:
                if not init.is_used():
                    continue
                if INIT_IGNORES and init.name in INIT_IGNORES:
                    continue

                initinfo = '*%s*' % (init.name)

                infoexts = ''
                if init.exts:
                    infoexts = '.' + ' .'.join(init.exts)
                if init.companions:
                    if not init.exts:
                        infoexts = '(base)'
                    infoexts += ' + .' + ' .'.join(init.companions)
                if infoexts:
                    #lines.append(infoexts)
                    initinfo += ": `%s`" % infoexts
                lines.append('  - ' + initinfo)

                if init.subfiles:
                    subfiles = 'Subfiles: *%s*' % ' '.join(init.subfiles)
                    lines.append('    - ' + subfiles)

            if info.codings:
                codings = 'Codecs: ' + ' '.join(info.codings)
                lines.append('  - ' + codings)

            #lines.append('')

        if IS_PRINT_INFO:
            print('\n'.join(lines))
        else:
            with open('formats-info.md', 'w', encoding='utf-8') as f:
                f.write('\n'.join(lines))

    def process(self):
        self.parse_files()
        self.parse_formats()
        self.print()


App().process()

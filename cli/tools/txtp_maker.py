#!/usr/bin/env python3
from __future__ import division
import argparse, subprocess, zlib, os, re, sys, fnmatch, logging as log

#******************************************************************************
# TXTP MAKER
#
# Creates .txtp from lists of files, mainly one .txtp per subsong
#******************************************************************************

PATH_LIMIT = 240

class Cli(object):
    def _parse(self):
        description = (
            "Makes TXTP from files in folders"
        )
        epilog = (
            "examples:\n"
            "  %(prog)s *.fsb \n"
            "  - make .txtp for subsongs in current dir\n\n"
            "  %(prog)s bgm/*.fsb \n"
            "  - make .txtp for subsongs in bgm subdir\n\n"
            "  %(prog)s * -r -fss 1\n"
            "  - make .txtp for all files in any subdirs with at least 1 subsong\n"
            "    (ignores formats without subsongs)\n\n"
            "  %(prog)s bgm.fsb -in -fcm 2 -fms 5.0\n"
            "  - make .txtp for subsongs with at least 2 channels and 5 seconds\n\n"
            "  %(prog)s *.scd -r -fd -l 2\n"
            "  - make .txtp for all .scd in subdirs, ignoring dupes, one .txtp per 2ch\n\n"
            "  %(prog)s *.sm1 -fne .+STREAM[.]SS[0-9]$\n"
            "  - make .txtp for all .sm1 excluding subsongs ending with 'STREAM.SS0..9'\n\n"
            "  %(prog)s samples.bnk -fni ^bgm.?\n"
            "  - make .txtp for in .bnk including only subsong names that start with 'bgm'\n\n"
            "  %(prog)s *.fsb -n \"{fn}<__{ss}>< [{in}]>\" -z 4 -o\n"
            "  - make .txtp for all fsb, adding subsongs and stream name if they exist\n\n"
            "  %(prog)s bgm.awb -or -os \"[%%%%s]\"\n"
            "  - make .txtp and rename repeated names with a custom [a/b/c...] suffix per repeated file\n\n"
            "  %(prog)s bgm.awb -or -os \"[alt%%%%i]\" -os2\n"
            "  - make .txtp and rename repeated names with a custom suffix [alt 1/2/3...], not including first\n\n"
        )

        p = argparse.ArgumentParser(description=description, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
        p.add_argument('files', help="Files to get (wildcards work)", nargs='+')
        p.add_argument('-r',  dest='recursive', help="Create .txtp in base folder from data in subfolders", action='store_true')
        p.add_argument('-c',  dest='cli', help="Set path to CLI (default: auto)")
        p.add_argument('-d',  dest='subdir', help="Set subdir inside .txtp (default: auto)")
        p.add_argument('-n',  dest='base_name', help=("Define (name).txtp, that can be formatted using:\n"
                                                      "- {filename}|{fn}=filename without extension\n"
                                                      "- {subsong}|{ss}=subsong number)\n"
                                                      "- {internal-name}|{in}=internal stream name\n"
                                                      "- {if}=internal name or filename if not found\n"
                                                      "* may be inside <...> for conditional text\n"))
        p.add_argument('-z',  dest='zero_fill', help="Zero-fill subsong number (default: auto per subsongs)", type=int)
        p.add_argument('-ie', dest='no_internal_ext', help="Remove internal name's extension if any", action='store_true')
        p.add_argument('-m',  dest='mini_txtp', help="Create mini-txtp", action='store_true')
        p.add_argument('-s',  dest='subsong_start', help="Start subsong", type=int)
        p.add_argument('-S',  dest='subsong_end', help="End subsong", type=int)
        p.add_argument('-o',  dest='overwrite', help="Overwrite existing .txtp\n(beware when using with internal names alone)", action='store_true')
        p.add_argument('-oi', dest='overwrite_ignore', help="Ignore repeated rather than overwritting .txtp\n", action='store_true')
        p.add_argument('-or', dest='overwrite_rename', help="Rename rather than overwriting", action='store_true')
        p.add_argument('-os', dest='overwrite_suffix', help="Rename with a suffix")
        p.add_argument('-os2', dest='overwrite_suffix_2nd', help="Rename with a suffix not including first", action='store_true')
        p.add_argument('-l',  dest='layers', help="Create .txtp per subsong layers, every N channels", type=int)
        p.add_argument('-fd', dest='test_dupes', help="Skip .txtp that point to duplicate streams (slower)", action='store_true')
        p.add_argument('-fcm', dest='min_channels', help="Filter by min channels", type=int)
        p.add_argument('-fcM', dest='max_channels', help="Filter by max channels", type=int)
        p.add_argument('-frm', dest='min_sample_rate', help="Filter by min sample rate", type=int)
        p.add_argument('-frM', dest='max_sample_rate', help="Filter by max sample rate", type=int)
        p.add_argument('-fsm', dest='min_seconds', help="Filter by min seconds (N.N)", type=float)
        p.add_argument('-fsM', dest='max_seconds', help="Filter by max seconds (N.N)", type=float)
        p.add_argument('-fss', dest='min_subsongs', help="Filter min subsongs\n(1 filters formats incapable of subsongs)", type=int)
        p.add_argument('-fni', dest='include_regex', help="Filter by REGEX including matches of subsong name")
        p.add_argument('-fne', dest='exclude_regex', help="Filter by REGEX excluding matches of subsong name")
        p.add_argument('-nsc',dest='no_semicolon', help="Remove semicolon names (for songs with multinames)", action='store_true')
        p.add_argument('-v', dest='log_level', help="Verbose log level (off|debug|info, default: info)", default='info')
        args = p.parse_args()

        # defauls to rename (easier to use with drag-and-drop)
        if not all([args.overwrite, args.overwrite_ignore, args.overwrite_rename]):
            args.overwrite_rename = True
        return args

    def start(self):
        args = self._parse()
        if not args.files:
            return
        Logger(args).setup_cli()
        App(args).start()

#******************************************************************************

class _GuiLogHandler(log.Handler):
    def __init__(self, txt):
        log.Handler.__init__(self)
        self._txt = txt

    def emit(self, message):
        msg = self.format(message)
        self._txt.config(state='normal')
        self._txt.insert('end', msg + '\n')
        self._txt.config(state='disabled')

class Logger(object):
    def __init__(self, cfg):
        levels = {
            'info': log.INFO,
            'debug': log.DEBUG,
        }
        self.level = levels.get(cfg.log_level, log.ERROR)

    def setup_cli(self):
        log.basicConfig(level=self.level, format='%(message)s')

    def setup_gui(self, txt):
        log.basicConfig(level=self.level, format='%(message)s', handlers=[_GuiLogHandler(txt)])

#******************************************************************************

class Cr32Helper(object):

    def __init__(self, cfg):
        self.cfg = cfg
        self.crc32_map = {}
        self.last_dupe = False

    def get_crc32(self, filename):
        buf_size = 0x8000
        with open(filename, 'rb') as file:
            buf = file.read(buf_size)
            crc32 = 0
            while len(buf) > 0:
                crc32 = zlib.crc32(buf, crc32)
                buf = file.read(buf_size)
        return crc32 & 0xFFFFFFFF 

    def update(self, filename):
        self.last_dupe = False
        if self.cfg.test_dupes == 0:
            return
        if not os.path.exists(filename):
            return

        crc32_str = format(self.get_crc32(filename),'08x')
        if (crc32_str in self.crc32_map):
            self.last_dupe = True
            return
        self.crc32_map[crc32_str] = True

        return

    def is_last_dupe(self):
        return self.last_dupe

#******************************************************************************

class TxtpInfo(object):
    def __init__(self, output_b):
        self.output = str(output_b).replace("\\r","").replace("\\n","\n")
        self.channels = self._get_value("channels: ")
        self.sample_rate = self._get_value("sample rate: ")
        self.num_samples = self._get_value("stream total samples: ")
        self.stream_count = self._get_value("stream count: ")
        self.stream_index = self._get_value("stream index: ")
        self.stream_name = self._get_text("stream name: ")
        self.encoding = self._get_text("encoding: ")

        # in case vgmstream returns error, but output code wasn't EXIT_FAILURE
        if self.channels <= 0 or self.sample_rate <= 0:
            raise ValueError('Incorrect vgmstream command')

        self.stream_seconds = self.num_samples / self.sample_rate

    def _get_string(self, str, full=False):
        find_pos = self.output.find(str)
        if (find_pos == -1):
            return None
        cut_pos = find_pos + len(str)
        str_cut = self.output[cut_pos:]
        if full:
            return str_cut.split("\n")[0].strip()
        else:
            return str_cut.split()[0].strip()

    def _get_text(self, str):
        text = self._get_string(str, full=True)
        # stream names in CLI is printed as UTF-8 using '\xNN', so detect and transform
        if text and '\\' in text:
            return text.encode('ascii').decode('unicode-escape').encode('iso-8859-1').decode('utf-8')
        return text

    def _get_value(self, str):
        res = self._get_string(str)
        if not res:
           return 0
        return int(res)

# Saves .txtp (usually 1 but may do N) to make from CLI outputs
class TxtpMaker(object):

    def __init__(self, cfg):
        self.cfg = cfg

        self.rename_map = {}
        self._items = []
        return

    def __str__(self):
        return str(self.__dict__)

    def parse(self, output_b):
        self.info = TxtpInfo(output_b)
        self.ignorable = self._is_ignorable(self.cfg)

    def reset(self):
        self._items = []

    def is_ignorable(self):
        return self.ignorable

    def _is_ignorable(self, cfg):
        if cfg.min_channels and self.info.channels < cfg.min_channels:
            return True
        if cfg.max_channels and self.info.channels > cfg.max_channels:
            return True
        if cfg.min_sample_rate and self.info.sample_rate < cfg.min_sample_rate:
            return True
        if cfg.max_sample_rate and self.info.sample_rate > cfg.max_sample_rate:
            return True
        if cfg.min_seconds and self.info.stream_seconds < cfg.min_seconds:
            return True
        if cfg.max_seconds and self.info.stream_seconds > cfg.max_seconds:
            return True
        if cfg.min_subsongs and self.info.stream_count < cfg.min_subsongs:
            return True

        if cfg.exclude_regex and self.info.stream_name:
            p = re.compile(cfg.exclude_regex)
            if p.match(self.info.stream_name) is not None:
                return True

        if cfg.include_regex and self.info.stream_name:
            p = re.compile(cfg.include_regex)
            if p.match(self.info.stream_name) is None:
                return True

        if self.info.encoding.lower() == 'silence':
            return True

        return False

    def _get_stream_mask(self, layer):
        if layer + self.cfg.layers > self.info.channels:
            loops = self.info.channels - self.cfg.layers
        else:
            loops = self.cfg.layers + 1

        mask = '#C'
        for ch in range(1, loops):
            mask += str(layer + ch) + ','
        return mask[:-1]

    def _clean_stream_name(self):
        if not self.info.stream_name:
            return None

        txt = self.info.stream_name
        # remove paths #todo maybe config/replace?
        pos = txt.rfind('\\')
        if pos >= 0:
            txt = txt[pos+1:]
        pos = txt.rfind('/')
        if pos >= 0:
            txt = txt[pos+1:]

        # remove bad chars
        badchars = ['%', '*', '?', ':', '\"', '|', '<', '>']
        for badchar in badchars:
            txt = txt.replace(badchar, '_')

        if not self.cfg.no_internal_ext:
            pos = txt.rfind(".")
            if pos >= 0:
                txt = txt[:pos]

        if self.cfg.no_semicolon:
            pos = txt.find(";")
            if pos >= 0:
                txt = txt[:pos].strip()

        return txt

    def _add(self, outname, line):
        self._items.append( (outname, line) )
        return

    def _write(self, outname, line):
        outname += '.txtp'

        if len(outname) > PATH_LIMIT:
            outname = outname[0:PATH_LIMIT] + '[...].txtp'

        cfg = self.cfg
        exists = os.path.exists(outname)
        if exists and (cfg.overwrite_rename or cfg.overwrite_suffix):
            must_rename = True
            if must_rename:
                if outname in self.rename_map:
                    rename_count = self.rename_map[outname]
                else:
                    rename_count = 0
                self.rename_map[outname] = rename_count + 1
                outname = outname.replace(".txtp", "_%08i.txtp" % (rename_count))

        # decide action after (possible) renames above
        if os.path.exists(outname):
            if cfg.overwrite_ignore:
                log.debug("ignored: " + outname)
                return

            if not cfg.overwrite:
                raise ValueError('TXTP exists in path: ' + outname)

        with open(outname,"w+", encoding='utf-8') as ftxtp:
            if line:
                ftxtp.write(line)

        log.debug("created: " + outname)
        return
        
    def include(self, filename_path, filename_clean):
        cfg = self.cfg
        total_done = 0

        if self.is_ignorable():
            return total_done

        # write plain (name).txtp when no subsongs
        if self.info.stream_count <= 1:
            index = None
        else:
            index = str(self.info.stream_index) #str to avoid falsy 0
            if cfg.zero_fill is None or cfg.zero_fill < 0:
                index = index.zfill(len(str(self.info.stream_count)))
            else:
                index = index.zfill(cfg.zero_fill)

        if cfg.mini_txtp:
            outname = filename_path
            if index:
                outname += "#" + index

            if cfg.layers and cfg.layers < self.info.channels:
                for layer in range(0, self.info.channels, cfg.layers):
                    mask = self._get_stream_mask(layer)
                    self._add(outname + mask, '')
                    total_done += 1
            else:
                self._add(outname, '')
                total_done += 1

        else:
            filename_base = os.path.basename(filename_path)
            pos = filename_base.rfind(".") #remove ext
            if pos > 1:
                filename_base = filename_base[:pos]

            outname = ''
            if cfg.base_name:
                stream_name = self._clean_stream_name()
                internal_filename = stream_name
                if not internal_filename:
                    internal_filename = filename_base

                replaces = {
                    'fn': filename_base,
                    'filename': filename_base,
                    'ss': index,
                    'subsong': index,
                    'in': stream_name,
                    'internal-name': stream_name,
                    'if': internal_filename,
                }

                pattern1 = re.compile(r"<(.+?)>")
                pattern2 = re.compile(r"{(.+?)}")
                txt = cfg.base_name

                # print optional info like "<text__{cmd}__>" only if value in {cmd} exists
                optionals = pattern1.findall(txt)
                for optional in optionals:
                    has_values = False
                    cmds = pattern2.findall(optional)
                    for cmd in cmds:
                        if cmd in replaces and replaces[cmd] is not None:
                            has_values = True
                            break
                    if has_values: #leave text there (cmds will be replaced later)
                        txt = txt.replace('<%s>' % optional, optional, 1)
                    else:
                        txt = txt.replace('<%s>' % optional, '', 1)

                # replace "{cmd}" if cmd exists with its value (non-existent values use '')
                cmds = pattern2.findall(txt)
                for cmd in cmds:
                    if cmd in replaces:
                        value = replaces[cmd]
                        if value is None:
                           value = ''
                        txt = txt.replace('{%s}' % cmd, value, 1)
                outname = "%s" % (txt)

            # no name set, or empty results above            
            if not outname:
                outname = "%s" % (filename_base)
                if index:
                    outname += "_" + index

            line = ''
            if cfg.subdir:
                line += cfg.subdir
            line += filename_clean
            if index:
                line += "#" + index

            if cfg.layers and cfg.layers < self.info.channels:
                done = 0
                for layer in range(0, self.info.channels, cfg.layers):
                    sub = '_' + chr(ord('a') + done)
                    done += 1
                    mask = self._get_stream_mask(layer)
                    self._add(outname + sub, line + mask)
                    total_done += 1
            else:
                self._add(outname, line)
                total_done += 1
        return total_done

    def _rename(self):
        cfg = self.cfg
        suffix = cfg.overwrite_suffix

        repeated = {}
        for outname, _ in self._items:
            if outname not in repeated:
                repeated[outname] = 0
            else:
                repeated[outname] += 1

        done = {}
        for i in range(len(self._items)):
            item = self._items[i]
            outname = item[0]

            if repeated.get(outname) <= 0:
                continue

            if outname not in done:
                done[outname] = 0
            done_count = done[outname]
            done[outname] += 1

            if suffix:
                repl_value = None
                repl_regex = None

                if '%s' in suffix:
                    if done_count >= 0 and done_count < 28:
                        repl_value = chr(done_count + 97)
                    else:
                        repl_value = str(done_count)
                    repl_regex = r"\%s"

                elif '%' in suffix:
                    repl_regex = r"\%[0-9]*i"
                    repl_value = done_count

                new_suffix = suffix
                if repl_value is not None:
                    if cfg.overwrite_suffix_2nd and repl_regex and done_count == 0:
                        new_suffix = re.sub(repl_regex, "", suffix)
                    else:
                        new_suffix = suffix % (repl_value)

            self._items[i] = (outname + new_suffix, item[1])
        return

    def write(self):
        if self.cfg.overwrite_suffix:
            self._rename()

        for outname, line in self._items:
            self._write(outname, line)

    def has_more_subsongs(self, target_subsong):
        return target_subsong < self.info.stream_count

#******************************************************************************

class App(object):
    def __init__(self, args):
        self.cfg = args
        self.crc32 = Cr32Helper(args)

    # check CLI in path (can be called, not just file exists)
    def _test_cli(self):
        clis = []
        if self.cfg.cli:
            clis.append(self.cfg.cli)
        else:
            clis.append('vgmstream-cli')
            clis.append('test.exe') #for old CLIs

        for cli in clis:
            try:
                with open(os.devnull, 'wb') as DEVNULL: #subprocess.STDOUT #py3 only
                    cmd = "%s" % (cli)
                    subprocess.check_call(cmd, stdout=DEVNULL, stderr=DEVNULL)
                self.cfg.cli = cli
                return True #exists and returns ok
            except subprocess.CalledProcessError as e:
                self.cfg.cli = cli
                return True #exists but returns strerr (ran with no args)
            except Exception as e:
                continue #doesn't exist

        #none found
        return False

    def _make_cmd(self, filename_in, filename_out, target_subsong):
        if self.cfg.test_dupes:
            cmd = "%s -s %s -i -o \"%s\" \"%s\"" % (self.cfg.cli, target_subsong, filename_out, filename_in)
        else:
            cmd = "%s -s %s -m -i -O \"%s\"" % (self.cfg.cli, target_subsong, filename_in)
        return cmd

    def _find_files(self, dir, pattern):
        if os.path.isfile(pattern):
            return [pattern]
        if os.path.isdir(pattern):
            dir = pattern
            pattern = None

        for dirsep in '/\\':
            if dirsep in pattern:
                index = pattern.rfind(dirsep)
                if index >= 0:
                    dir = pattern[0:index]
                    pattern = pattern[index+1:]

        files = []
        for root, dirnames, filenames in os.walk(dir):
            # manually test name as is too, as filter wouldn't handle stuff like "bgm[us].wav"
            for filename in filenames:
                if filename == pattern or fnmatch.fnmatch(filename, pattern):
                    files.append(os.path.join(root, filename))

            if not self.cfg.recursive:
                break

        return files

    def start(self):
        if not self._test_cli():
            log.error("ERROR: CLI not found")
            return

        filenames_in = []
        for filename in self.cfg.files:
            filenames_in += self._find_files('.', filename)

        maker = TxtpMaker(self.cfg)

        total_created = 0
        total_dupes = 0
        total_errors = 0

        for filename_in in filenames_in:
            filename_in_clean = filename_in.replace("\\", "/")
            if filename_in_clean.startswith("./"):
                filename_in_clean = filename_in_clean[2:]

            filename_in_base = os.path.basename(filename_in)

            #skip starting dot for extensionless files
            if filename_in.startswith(".\\"):
                filename_in = filename_in[2:]

            filename_out = ".temp." + filename_in_base + ".wav"
            created = 0
            dupes = 0
            errors = 0

            subsong_start = self.cfg.subsong_start
            subsong_end = self.cfg.subsong_end
            if not subsong_start:
                subsong_start = 1
            target_subsong = subsong_start

            # subsongs should treat repeat names separately? pass flag?
            #maker.reset(rename_map)

            while True:
                try:
                    # main call to vgmstream
                    cmd = self._make_cmd(filename_in, filename_out, target_subsong)
                    log.debug("calling: %s", cmd)
                    output_b = subprocess.check_output(cmd, shell=False) #stderr=subprocess.STDOUT

                    # basic parse of vgmstream info
                    maker.parse(output_b)

                except (subprocess.CalledProcessError, ValueError) as e:
                    log.debug("ignoring CLI error in %s #%s: %s", filename_in, target_subsong, str(e))
                    errors += 1
                    break

                if target_subsong == subsong_start:
                    log.debug("processing %s...", filename_in_clean)

                if not maker.is_ignorable():
                    self.crc32.update(filename_out)

                if not self.crc32.is_last_dupe():
                    created += maker.include(filename_in_base, filename_in_clean)
                else:
                    dupes += 1
                    log.debug("dupe subsong %s", target_subsong)

                if not maker.has_more_subsongs(target_subsong):
                    break
                if subsong_end and target_subsong >= subsong_end:
                    break
                target_subsong += 1

                if target_subsong % 200 == 0:
                    log.info("%s/%s subsongs... (%s dupes, %s errors)", target_subsong, maker.info.stream_count, dupes, errors)

            if os.path.exists(filename_out):
                os.remove(filename_out)

            total_created += created
            total_dupes += dupes
            total_errors += errors

        maker.write()

        log.info("done! (%s done, %s dupes, %s errors)", total_created, total_dupes, total_errors)


if __name__ == "__main__":
    Cli().start()

    #if len(sys.argv) > 1:
    #    Cli().start()
    #else:
    #    Gui().start()

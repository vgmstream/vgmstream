#!/usr/bin/env python3

# ########################################################################### #
# TXTP MAKER
# ########################################################################### #

from __future__ import division
import subprocess
import zlib
import os.path
import os
import re
import sys
import fnmatch

def print_usage(appname):
    print("Usage: {} (filename) [options]".format(appname)+"\n"
          "\n"
          "Creates (filename)_(subsong).txtp for every subsong in (filename).\n"
          " (filename) can be a * or *.ext wildcard too (works with dupe filters).\n"
          "Works with files with no subsongs (unless filtered) too.\n"
          "\n"
          "Use -h to print [options]. Examples:\n"
          "\n"
          "{} bgm.fsb -in -fcm 2 -fms 5.0 ".format(appname)+"\n"
          "    make TXTP for subsongs with at least 2 channels and 5 seconds\n"
          "{} *.scd -r -fd -l 2".format(appname)+"\n"
          "   all .scd in subdirs, ignoring dupes and making per 2ch layers\n"
          "{} *.sm1 -fne .+STREAM[.]SS[0-9]$ ".format(appname)+"\n"
          "    all .sm1 excluding those subsong names that ends with 'STREAM.SS0..9'\n"
          "{} samples.bnk -fni ^bgm.? ".format(appname)+"\n"
          "    in .bnk including only subsong names that start with 'bgm'\n"
          "{} * -r -fss 1".format(appname)+"\n"
          "   all files in subdirs with at least 1 subsong (ignoring formats without them)\n"
          )

def print_help(appname):
    print("Options:\n"
          " -r: find recursive (writes files to current dir, with dir in TXTP)\n"
          " -c (name): set path to CLI (default: test.exe)\n"
          " -n (name): use (name).txtp, that can be formatted using:\n"
          "   {filename}, {subsong}, {internal-name}\n"
          "   ex. -n BGM_{subsong}, -n {subsong}__{internal-name} "
          " -z N: zero-fill subsong number (default: auto fill up to total subsongs)\n"
          " -d (dir): add dir in TXTP (if the file will reside in a subdir)\n"
          " -m: create mini-txtp\n"
          " -o: overwrite existing .txtp (beware when using with internal names)\n"
          " -O: rename rather than overwriting\n"
          " -in: name TXTP using the subsong's internal name if found\n"
          " -ie: remove internal name's extension\n"
          " -ii: add subsong number when using internal name\n"
          " -l N: create multiple TXTP per subsong layers, every N channels\n"
          " -fd: filter duplicates (slower)\n"
          " -fcm N: filter min channels\n"
          " -fcM N: filter by max channels\n"
          " -frm N: filter by min sample rate\n"
          " -frM N: filter by max sample rate\n"
          " -fsm N.N: filter by min seconds\n"
          " -fsM N.N: filter by max seconds\n"
          " -fss N: filter min subsongs (1 filters formats incapable of subsongs)\n"
          " -fni (regex): filter by subsong name, include files that match\n"
          " -fne (regex): filter by subsong name, exclude files that match\n"
          " -v (name): verbose level (off|trace|debug|info, default: info)\n"
          " -h N: show this help\n"
          )

# ########################################################################### #

def find_files(dir, pattern, recursive):
    files = []
    for root, dirnames, filenames in os.walk(dir):
        for filename in fnmatch.filter(filenames, pattern):
            files.append(os.path.join(root, filename))

        if not recursive:
            break
            
    return files

def make_cmd(cfg, fname_in, fname_out, target_subsong):
    if (cfg.test_dupes):
        cmd = "{} -s {} -i -o \"{}\" \"{}\"".format(cfg.cli, target_subsong, fname_out, fname_in)
    else:
        cmd = "{} -s {} -m -i -o \"{}\" \"{}\"".format(cfg.cli, target_subsong, fname_out, fname_in)
    return cmd

class LogHelper(object):

    def __init__(self, cfg):
        self.cfg = cfg

    def trace(self, msg):
        v = self.cfg.verbose
        if v == "trace":
            print(msg)

    def debug(self, msg):
        v = self.cfg.verbose
        if v == "trace" or v == "debug":
            print(msg)

    def info(self, msg):
        v = self.cfg.verbose
        if v == "trace" or v == "debug" or v == "info":
            print(msg)

class ConfigHelper(object):
    show_help = False
    cli = "test.exe"

    recursive = False
    base_name = ''
    zero_fill = -1
    subdir = ''
    mini_txtp = False
    overwrite = False
    overwrite_rename = False
    rename_map = {}
    layers = 0

    use_internal_name = False
    use_internal_ext = False
    use_internal_index = False

    test_dupes = False
    min_channels = 0
    max_channels = 0
    min_sample_rate = 0
    max_sample_rate = 0
    min_seconds = 0.0
    max_seconds = 0.0
    min_subsongs = 0
    include_regex = ""
    exclude_regex = ""

    verbose = "info"

    argv_len = 0
    index = 0


    def read_bool(self, command, default):
        if self.index > self.argv_len - 1:
            return default
        if self.argv[self.index] == command:
            val = True
            self.index += 1
            return val
        return default
    
    def read_value(self, command, default):
        if self.index > self.argv_len - 2:
            return default
        if self.argv[self.index] == command:
            val = self.argv[self.index+1]
            self.index += 2
            return val
        return default

    def read_string(self, command, default):
        return str(self.read_value(command, default))

    def read_int(self, command, default):
        return int(self.read_value(command, default))

    def read_float(self, command, default):
        return float(self.read_value(command, default))

    #todo improve this poop
    def __init__(self, argv):
        self.index = 2 #after file
        self.argv = argv 
        self.argv_len = len(argv)

        if argv[1] == '-h':
            self.show_help = True
        
        prev_index = self.index
        while self.index < len(self.argv):
            self.show_help = self.read_bool('-h', self.show_help)
            self.cli = self.read_string('-c', self.cli)
            self.recursive = self.read_bool('-r', self.recursive)
            self.base_name = self.read_string('-n', self.base_name)
            self.zero_fill = self.read_int('-z', self.zero_fill)
            self.subdir = self.read_string('-d', self.subdir)

            self.test_dupes = self.read_bool('-fd', self.test_dupes)
            self.min_channels = self.read_int('-fcm', self.min_channels)
            self.max_channels = self.read_int('-fcM', self.max_channels)
            self.min_sample_rate = self.read_int('-frm', self.min_sample_rate)
            self.max_sample_rate = self.read_int('-frM', self.max_sample_rate)
            self.min_seconds = self.read_float('-fsm', self.min_seconds)
            self.max_seconds = self.read_float('-fsM', self.max_seconds)
            self.min_subsongs = self.read_int('-fss', self.min_subsongs)
            self.include_regex = self.read_string('-fni', self.include_regex)
            self.exclude_regex = self.read_string('-fne', self.exclude_regex)

            self.mini_txtp = self.read_bool('-m', self.mini_txtp)
            self.overwrite = self.read_bool('-o', self.overwrite)
            self.overwrite_rename = self.read_bool('-O', self.overwrite_rename)
            self.layers = self.read_int('-l', self.layers)

            self.use_internal_name = self.read_bool('-in', self.use_internal_name)
            self.use_internal_ext = self.read_bool('-ie', self.use_internal_ext)
            self.use_internal_index = self.read_bool('-ii', self.use_internal_index)

            self.verbose = self.read_string('-v', self.verbose)

            if prev_index == self.index:
                self.index += 1
            prev_index = self.index

        if (self.subdir != '') and not (self.subdir.endswith('/') or self.subdir.endswith('\\')):
            self.subdir += '/'

    def __str__(self):
        return str(self.__dict__)


class Cr32Helper(object):
    crc32_map = {}
    dupe = False
    cfg = None
    
    def __init__(self, cfg):
        self.cfg = cfg

    def get_crc32(self, fname):
        buf_size = 0x8000
        with open(fname, 'rb') as file:
            buf = file.read(buf_size)
            crc32 = 0
            while len(buf) > 0:
                crc32 = zlib.crc32(buf, crc32)
                buf = file.read(buf_size)
        return crc32 & 0xFFFFFFFF 

    def update(self, fname):
        cfg = self.cfg

        self.dupe = False
        if cfg.test_dupes == 0:
            return
        if not os.path.exists(fname):
            return

        crc32_str = format(self.get_crc32(fname),'08x')
        if (crc32_str in self.crc32_map):
            self.dupe = True
            return
        self.crc32_map[crc32_str] = True

        return

    def is_dupe(self):
        return self.dupe


class TxtpMaker(object):
    channels = 0
    sample_rate = 0
    num_samples = 0
    stream_count = 0
    stream_index = 0
    stream_name = ''
    stream_seconds = 0

    def __init__(self, cfg, output_b, log):
        self.cfg = cfg
        self.log = log

        self.output = str(output_b).replace("\\r","").replace("\\n","\n")
        self.channels = self.get_value("channels: ")
        self.sample_rate = self.get_value("sample rate: ")
        self.num_samples = self.get_value("stream total samples: ")
        self.stream_count = self.get_value("stream count: ")
        self.stream_index = self.get_value("stream index: ")
        self.stream_name = self.get_string("stream name: ")

        if self.channels == 0:
            raise ValueError('Incorrect command result')

        self.stream_seconds = self.num_samples / self.sample_rate

    def __str__(self):
        return str(self.__dict__)

    def get_string(self, str):
        find_pos = self.output.find(str)
        if (find_pos == -1):
            return ''
        cut_pos = find_pos + len(str)
        str_cut = self.output[cut_pos:]
        return str_cut.split()[0]

    def get_value(self, str):
        res = self.get_string(str)
        if (res == ''):
           return 0;
        return int(res)

    def is_ignorable(self):
        cfg = self.cfg

        if (self.channels < cfg.min_channels):
            return True;
        if (cfg.max_channels > 0 and self.channels > cfg.max_channels):
            return True;
        if (self.sample_rate < cfg.min_sample_rate):
            return True;
        if (cfg.max_sample_rate > 0 and self.sample_rate > cfg.max_sample_rate):
            return True;
        if (self.stream_seconds < cfg.min_seconds):
            return True;
        if (cfg.max_seconds > 0 and self.stream_seconds > cfg.max_seconds):
            return True;
        if (self.stream_count < cfg.min_subsongs):
            return True;
        if (cfg.exclude_regex != "" and self.stream_name != ""):
            p = re.compile(cfg.exclude_regex)
            if (p.match(self.stream_name) != None):
                return True
        if (cfg.include_regex != "" and self.stream_name != ""):
            p = re.compile(cfg.include_regex)
            if (p.match(self.stream_name) == None):
                return True

        return False

    def get_stream_mask(self, layer):
        cfg = self.cfg

        mask = '#C'

        loops = cfg.layers + 1
        if layer + cfg.layers > self.channels:
            loops = self.channels - cfg.layers
        for ch in range(1,loops):
            mask += str(layer+ch) + ','

        mask = mask[:-1]
        return mask

    def get_stream_name(self):
        cfg = self.cfg

        if not cfg.use_internal_name:
            return ''
        txt = self.stream_name

        # remove paths #todo maybe config/replace?
        pos = txt.rfind("\\")
        if (pos != -1):
            txt = txt[pos+1:]
        pos = txt.rfind("/")
        if (pos != -1):
            txt = txt[pos+1:]
        # remove bad chars
        txt = txt.replace("%", "_")
        txt = txt.replace("*", "_")
        txt = txt.replace("?", "_")
        txt = txt.replace(":", "_")
        txt = txt.replace("\"", "_")
        txt = txt.replace("|", "_")
        txt = txt.replace("<", "_")
        txt = txt.replace(">", "_")
    
        if not cfg.use_internal_ext:
            pos = txt.rfind(".")
            if (pos != -1):
                txt = txt[:pos]
        return txt
        
    def write(self, outname, line):
        cfg = self.cfg

        outname += '.txtp'

        if cfg.overwrite_rename and os.path.exists(outname):
            if outname in cfg.rename_map:
                rename_count = cfg.rename_map[outname]
            else:
                rename_count = 0
            cfg.rename_map[outname] = rename_count + 1
            outname = outname.replace(".txtp", "_{}.txtp".format(rename_count))

        if not cfg.overwrite and os.path.exists(outname):
            raise ValueError('TXTP exists in path: ' + outname)
        ftxtp = open(outname,"w+")
        if line != '':
            ftxtp.write(line)
        ftxtp.close()

        self.log.debug("created: " + outname)
        return

    def make(self, fname_path, fname_clean):
        cfg = self.cfg
        total_done = 0

        if self.is_ignorable():
            return total_done

        # write plain (name).txtp when no subsongs
        if self.stream_count <= 1:
            index = ""
        else:
            index = str(self.stream_index)
            if cfg.zero_fill < 0:
                index = index.zfill(len(str(self.stream_count)))
            else:
                index = index.zfill(cfg.zero_fill)

        if cfg.mini_txtp:
            outname = fname_path
            if index != "":
                outname += "#" + index

            if cfg.layers > 0 and cfg.layers < self.channels:
                for layer in range(0, self.channels, cfg.layers):
                    mask = self.get_stream_mask(layer)
                    self.write(outname + mask, '')
                    total_done += 1
            else:
                self.write(outname, '')
                total_done += 1

        else:
            stream_name = self.get_stream_name()
            if stream_name != '':
                outname = stream_name
                if cfg.use_internal_index:
                    outname += "_{}".format(index)
            else:
                if cfg.base_name != '':
                    fname_base = os.path.basename(fname_path)
                    pos = fname_base.rfind(".") #remove ext
                    if (pos != -1 and pos > 1):
                        fname_base = fname_base[:pos]
                        
                    internal_name = self.stream_name
                
                    txt = cfg.base_name
                    txt = txt.replace("{filename}",fname_base)
                    txt = txt.replace("{subsong}",index)
                    txt = txt.replace("{internal-name}",internal_name)

                    outname = "{}".format(txt)

                else:
                    txt = fname_path
                    pos = txt.rfind(".") #remove ext
                    if (pos != -1 and pos > 1):
                        txt = txt[:pos]

                    outname = "{}".format(txt)
                    if index != "":
                        outname += "_" + index

            line = ''
            if cfg.subdir != '':
                line += cfg.subdir
            line += fname_clean
            if index != "":
                line += "#" + index

            if cfg.layers > 0 and cfg.layers < self.channels:
                done = 0
                for layer in range(0, self.channels, cfg.layers):
                    sub = chr(ord('a') + done)
                    done += 1
                    mask = self.get_stream_mask(layer)
                    self.write(outname + sub, line + mask)
                    total_done += 1
            else:
                self.write(outname, line)
                total_done += 1
        return total_done

    def has_more_subsongs(self, target_subsong):
        return target_subsong < self.stream_count

# ########################################################################### #

def main():
    appname = os.path.basename(sys.argv[0])
    if (len(sys.argv) <= 1):
        print_usage(appname)
        return

    cfg = ConfigHelper(sys.argv)
    crc32 = Cr32Helper(cfg)
    log = LogHelper(cfg)

    if cfg.show_help:
        print_help(appname)
        return

    fname = sys.argv[1]
    fnames_in = find_files('.', fname, cfg.recursive)

    total_created = 0
    total_dupes = 0
    total_errors = 0
    for fname_in in fnames_in:
        fname_in_clean = fname_in.replace("\\", "/")
        if fname_in_clean.startswith("./"):
            fname_in_clean = fname_in_clean[2:]
           
        fname_in_base = os.path.basename(fname_in)
        
        if fname_in.startswith(".\\"): #skip starting dot for extensionless files
            fname_in = fname_in[2:]
        
        fname_out = ".temp." + fname_in_base + ".wav"
        created = 0
        dupes = 0
        errors = 0
        target_subsong = 1
        while 1:

            try:
                cmd = make_cmd(cfg, fname_in, fname_out, target_subsong)
                log.trace("calling: " + cmd)
                output_b = subprocess.check_output(cmd, shell=False) #stderr=subprocess.STDOUT
            except subprocess.CalledProcessError as e:
                log.debug("ignoring CLI error in " + fname_in + "#"+str(target_subsong)+": " + e.output)
                errors += 1
                break

            if target_subsong == 1:
                log.debug("processing {}...".format(fname_in_clean))

            maker = TxtpMaker(cfg, output_b, log)

            if not maker.is_ignorable():
                crc32.update(fname_out)

            if not crc32.is_dupe():
                created += maker.make(fname_in_base, fname_in_clean)
            else:
                dupes += 1
                log.debug("Dupe subsong {}".format(target_subsong))

            if not maker.has_more_subsongs(target_subsong):
                break
            target_subsong += 1

            if target_subsong % 200 == 0:
                log.info("{}/{} subsongs... ".format(target_subsong, maker.stream_count) + 
                          "({} dupes, {} errors)".format(dupes, errors)
                          )

        if os.path.exists(fname_out):
            os.remove(fname_out)

        total_created += created
        total_dupes += dupes
        total_errors += errors


    log.info("done! ({} done, {} dupes, {} errors)".format(total_created, total_dupes, total_errors))
    
if __name__ == "__main__":
    main()

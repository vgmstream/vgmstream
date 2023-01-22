# VRTS - VGMSTREAM REGRESSION TESTING SCRIPT
#
# Searches for files in a directory (or optionally subdirs) and compares
# the output of two CLI versions, both wav and stdout, for regression
# testing. This creates and deletes temp files, trying to process all
# extensions found unless specified (except a few).

# TODO reject some .wav but not all (detect created by v)
# TODO capture stdout and enable fuzzy depending on codec
# TODO fix -l option, add decode reset option
import os, argparse, time, datetime, glob, subprocess, struct


# don't try to decode common stuff
IGNORED_EXTENSIONS = ['.exe', '.dll', '.zip', '.7z', '.rar', '.bat', '.sh', '.txt', '.lnk', '.wav', '.py', '.md', '.idb']
#TODO others
FUZZY_CODECS = ['ffmpeg', 'vorbis', 'mpeg', 'speex', 'celt']

DEFAULT_CLI_NEW = 'vgmstream-cli'
DEFAULT_CLI_OLD = 'vgmstream-cli_old'

# result codes, where >= 0: ok (acceptable), <0: ko (not good)
RESULT_SAME = 0             # no diffs
RESULT_FUZZY = 1            # no duffs allowing +-N
RESULT_NONE = 2             # neither exists
RESULT_DIFFS = -3           # different
RESULT_SIZES = -4           # different sizes
RESULT_MISSING_NEW = -5     # new does not exist 
RESULT_MISSING_OLD = 6      # old does not exist 

###############################################################################

def parse_args():
    description = (
        "Compares new vs old vgmstream CLI output, for regression testing"
    )
    epilog = (
        "examples:\n"
        "%(prog)s *.ogg -r -n\n"
        "- checks for differences in ogg of this and subfolders\n"
        "%(prog)s *.adx -nd\n"
        "- checks for differences in adx and doesn't delete wav output\n"
        "%(prog)s -p\n"
        "- compares performance performance of all files\n"
    )

    ap = argparse.ArgumentParser(description=description, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("files", help="files to match", nargs='*', default=["*.*"])
    ap.add_argument("-r","--recursive", help="search files in subfolders", action='store_true')
    ap.add_argument("-z","--fuzzy", help="fuzzy threshold of +-N PCM16LE", type=int, default=1)
    ap.add_argument("-nd","--no-delete", help="don't delete output", action='store_true')
    ap.add_argument("-rd","--result-diffs", help="only report full diffs", action='store_true')
    ap.add_argument("-rz","--result-fuzzy", help="only report full and fuzzy diffs", action='store_true')
    ap.add_argument("-p","--performance-both", help="compare decode performance", action='store_true')
    ap.add_argument("-pn","--performance-new", help="test performance of new CLI", action='store_true')
    ap.add_argument("-po","--performance-old", help="test performance of old CLI", action='store_true')
    ap.add_argument("-pw","--performance-write", help="compare decode+write performance", action='store_true')
    ap.add_argument("-pr","--performance-repeat", help="repeat decoding files N times\n(more files makes easier to see performance changes)", type=int, default=0)
    ap.add_argument("-l","--looping", help="compare looping files (slower)", action='store_true')
    ap.add_argument("-cn","--cli-new", help="sets name of new CLI (can be a path)")
    ap.add_argument("-co","--cli-old", help="sets name of old CLI (can be a path)")

    args = ap.parse_args()
    
    # derived defaults to simplify
    args.performance = args.performance_both or args.performance_new or args.performance_old or args.performance_write
    args.compare = not args.performance
    if args.performance_both:
        args.performance_new = True
        args.performance_old = True

    return args

###############################################################################

S16_UNPACK = struct.Struct('<h').unpack_from

# Compares 2 files and returns if contents are the same.
# If fuzzy is set detects +- PCM changes (slower).
class VrtsComparator:
    CHUNK_HEADER = 0x50
    CHUNK_SIZE = 0x00100000

    def __init__(self, path1, path2, fuzzy_max=0):
        self._path1 = path1
        self._path2 = path2
        self._fuzzy_max = fuzzy_max
        self._offset = 0
        self.fuzzy_count = 0
        self.fuzzy_diff = 0
        self.fuzzy_offset = 0

    def _compare_fuzzy(self, b1, b2):
        len1 = len(b1)
        len2 = len(b2)
        if len1 != len2:
            return RESULT_SAME

        # compares PCM16LE bytes allowing +-N diffs between PCM bytes
        # useful when comparing output from floats, that can change slightly due to compiler optimizations
        max = self._fuzzy_max
        pos = 0
        while pos < len1:
            # slower than struct unpack
            #pcm1 = b1[pos+0] | (b1[pos+1] << 8)
            #if pcm1 & 0x8000:
            #    pcm1 -= 0x10000
            #pcm2 = b2[pos+0] | (b2[pos+1] << 8)
            #if pcm2 & 0x8000:
            #    pcm2 -= 0x10000
            
            pcm1, = S16_UNPACK(b1, pos)
            pcm2, = S16_UNPACK(b2, pos)

            if not (pcm1 >= pcm2 - max and pcm1 <= pcm2 + max):
                #print("%i vs %i +- %i at %x" % (pcm1, pcm2, max, self._offset + pos))
                self.fuzzy_diff = pcm1 - pcm2
                self.fuzzy_offset = self._offset + pos
                return RESULT_DIFFS

            pos += 2

        self.fuzzy_count = 1
        return 0

    def _compare_bytes(self, b1, b2):
        # even though python is much slower than C this test is reasonably fast
        if b1 == b2:
            return RESULT_SAME

        # different: fuzzy check if same
        if self._fuzzy_max:
            return self._compare_fuzzy(b1, b2)

        return RESULT_DIFFS

    def _compare_files(self, f1, f2):

        # header not part of fuzzyness (no need to get exact with sizes)
        if self._fuzzy_max:
            b1 = f1.read(self.CHUNK_HEADER)
            b2 = f2.read(self.CHUNK_HEADER)
            cmp = self._compare_bytes(b1, b2)
            if cmp < 0:
                return cmp
            self._offset += self.CHUNK_HEADER

        while True:
            b1 = f1.read(self.CHUNK_SIZE)
            b2 = f2.read(self.CHUNK_SIZE)
            if not b1 or not b2:
                break

            cmp = self._compare_bytes(b1, b2)
            if cmp < 0:
                return cmp
            self._offset += self.CHUNK_SIZE

        return 0
    

    def compare(self):
        try:
            f1_len = os.path.getsize(self._path1)
        except FileNotFoundError:
            f1_len = -1
        try:
            f2_len = os.path.getsize(self._path2)
        except FileNotFoundError:
            f2_len = -1

        if f1_len < 0 and f2_len < 0:
            return RESULT_NONE

        if f1_len < 0:
            return RESULT_MISSING_NEW
        if f2_len < 0:
            return RESULT_MISSING_OLD

        if f1_len != f2_len:
            return RESULT_SIZES

        with open(self._path1, 'rb') as f1, open(self._path2, 'rb') as f2:
            cmp = self._compare_files(f1, f2)
            if cmp < 0:
                return cmp

        if self.fuzzy_count > 0:
            return RESULT_FUZZY
        return RESULT_SAME


###############################################################################

# prints colored text in CLI
#   https://pkg.go.dev/github.com/whitedevops/colors
#   https://stackoverflow.com/questions/287871/
class VrtsPrinter:
    RESET = '\033[0m'
    BOLD = '\033[1m'

    LIGHT_RED = '\033[91m'
    LIGHT_GREEN = '\033[92m'
    LIGHT_YELLOW = '\033[93m'
    LIGHT_BLUE = '\033[94m'
    LIGHT_CYAN = '\033[96m'
    WHITE = '\033[97m'
    LIGHT_GRAY = "\033[37m"
    DARK_GRAY = "\033[90m"
    
    COLOR_RESULT = {
        RESULT_SAME: WHITE,
        RESULT_FUZZY: LIGHT_CYAN,
        RESULT_NONE: LIGHT_YELLOW,
        RESULT_DIFFS: LIGHT_RED,
        RESULT_SIZES: LIGHT_RED,
        RESULT_MISSING_NEW: LIGHT_RED,
        RESULT_MISSING_OLD: LIGHT_YELLOW,
    }
    TEXT_RESULT = {
        RESULT_SAME: 'same',
        RESULT_FUZZY: 'fuzzy same',
        RESULT_NONE: 'neither works',
        RESULT_DIFFS: 'diffs',
        RESULT_SIZES: 'wrong sizes',
        RESULT_MISSING_NEW: 'missing new',
        RESULT_MISSING_OLD: 'missing old',
    }

    def __init__(self):
        try:
            os.system('color') #win only?
        except:
            pass

    def _print(self, msg, color=None):
        if color:
            print("%s%s%s" % (color, msg, self.RESET))
        else:
            print(msg)

           
    def result(self, msg, code, fuzzy_diff, fuzzy_offset):
        text = self.TEXT_RESULT.get(code)
        color = self.COLOR_RESULT.get(code)
        if not text:
            text = code
        msg = "%s: %s" % (msg, text)
        if fuzzy_diff != 0:
            msg += " (%s @0x%x)" % (fuzzy_diff, fuzzy_offset)
        self._print(msg, color)


    def info(self, msg):
        msg = "%s (%s)" % (msg, self._get_date())
        self._print(msg, self.DARK_GRAY)
        pass

    def _get_date(self):
        return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")

###############################################################################

class VrtsFiles:
    def __init__(self, args):
        self._args = args
        self.filenames = []

    def prepare(self):
        for fpattern in self._args.files:
        
            recursive = self._args.recursive
            if recursive:
                fpattern = '**/' + fpattern

            files = glob.glob(fpattern, recursive=recursive)
            for file in files:
                if not os.path.isfile(file):
                    continue

                # ignores non useful files
                _, ext = os.path.splitext(file)
                if ext.lower() in IGNORED_EXTENSIONS:
                    continue

                self.filenames.append(file)

                # same file N times
                if self._args.performance and self._args.performance_repeat:
                    for i in range(self._args.performance_repeat):
                        self.filenames.append(file)


# calling subprocess with python:
# - os.system(command)
#   - not recommended by docs (less flexible and spawns a new process?)
# - subprocess.call
#   - wait till complete and returns code
# - subprocess.check_call
#   - wait till complete and raise CalledProcessError on nonzero return code
# - subprocess.check_output
#   - call without wait, raise CalledProcessError on nonzero return code
# - subprocess.run
#   - recommended but python 3.5+ 
#    (check=True: raise exceptions like check_*, capture_output: return STDOUT/STDERR)
class VrtsProcess:

    def call(self, args, stdout=False):
        try:
            #with open(os.devnull, 'wb') as DEVNULL: #python2
            #    res = subprocess.check_call(args, stdout=DEVNULL, stderr=DEVNULL)

            res = subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) #capture_output=stdout, 
            #print("result:", res.returncode)
            #print("result:", res.strout, res.strerr)

            if stdout:
                return res.stdout

            return True #exists and returns ok

        except subprocess.CalledProcessError as e:
            #print("call error: ", e) #, e.stderr: disable DEVNULL
            return False #non-zero, exists but returns strerr (ex. ran with no args)

        except FileNotFoundError as e:
            #print("file error: ", e)
            return None #doesn't exists/etc


class VrtsApp:

    def __init__(self, args):
        self._args = args
        self._files = VrtsFiles(args)
        self._prc = VrtsProcess()
        self._p = VrtsPrinter()
        self._cli_new = None
        self._cli_old = None

    def _find_cli(self, arg_cli, default_cli):

        if arg_cli and os.path.isdir(arg_cli):
            cli = os.path.join(arg_cli, default_cli)

        elif arg_cli: #is file
            cli = arg_cli

        else:
            cli = default_cli

        args = [cli] #plain call to see if program is in PATH
        res = self._prc.call(args)
        if res is not None:
            return cli

        return None

    # detects CLI location:
    # - defaults to (cli) [new] + (cli)_old [old] assumed to be in PATH
    # - can be passed a dir or file for old/new
    # - old is optional in performance mode
    def _detect_cli(self):
        cli = self._find_cli(self._args.cli_new, DEFAULT_CLI_NEW)
        if cli:
            self._cli_new = cli

        cli = self._find_cli(self._args.cli_old, DEFAULT_CLI_OLD)
        if cli:
            self._cli_old = cli

        if not self._cli_new and (self._args.compare or self._args.performance_new):
            raise ValueError("new CLI not found")
        if not self._cli_old and (self._args.compare or self._args.performance_old):
            raise ValueError("old CLI not found")

    def _get_performance_args(self, cli):
        args = [cli, '-O'] #flag to not write files
        if self._args.looping:
            args.append('-i')
        args.extend(self._files.filenames)
        return args

    def _performance(self):
        flag_looping = ''
        if self._args.looping:
            flag_looping = '-i'

        # pases all files at once, as it's faster than 1 by 1 (that has to init program every time)
        if self._performance_new:
            self._p.info("testing new performance")
            ts_st = time.time()

            args = self._get_performance_args(self._cli_new)
            res = self._prc.call(args)

            ts_ed = time.time()
            self._p.info("done: elapsed %ss" % (ts_ed - ts_st))

        if self._performance_old:
            self._p.info("testing old performance")
            ts_st = time.time()

            args = self._get_performance_args(self._cli_old)
            res = self._prc.call(args)

            ts_ed = time.time()
            self._p.info("done: elapsed %ss (%s)" % (ts_ed - ts_st))

        #if self._performance_both:
        #   ...


    # returns max fuzzy count, except for non-fuzzable files (that use int math)
    def _get_fuzzy_count(self, stdout):
        fuzzy = self._args.fuzzy
        if self._args.fuzzy <= 0:
            return 0

        if not stdout:
            return fuzzy

        try:
            pos = stdout.index(b'encoding:')
            codec_line = stdout[0:].split('\n', 1)[0]
            for fuzzy_codec in FUZZY_CODECS:
                if fuzzy_codec in codec_line:
                    return fuzzy
        except Exception as e:
            pass

        return 0 #non-fuzable

    def _get_compare_args(self, cli, outwav, filename):
        args = [cli, '-o', outwav] #flag to not write files
        if self._args.looping:
            args.append('-i')
        args.append(filename)
        return args

    def _compare(self):
        ts_st = time.time()
        self._p.info("comparing files")

        flag_looping = ''
        if self._args.looping:
            flag_looping = '-i'

        total_ok = 0
        total_ko = 0
        for filename in self._files.filenames:
            filename_newwav = filename + ".new.wav"
            filename_oldwav = filename + ".old.wav"

            # main decode (ignores errors, comparator already checks them)
            args = self._get_compare_args(self._cli_new, filename_newwav, filename)
            stdout = self._prc.call(args, stdout=True)

            args = self._get_compare_args(self._cli_old, filename_oldwav, filename)
            self._prc.call(args, stdout=False)

            # test results
            fuzzy = self._get_fuzzy_count(stdout)
            cmp = VrtsComparator(filename_newwav, filename_oldwav, fuzzy)
            code = cmp.compare()

            self._p.result(filename, code, cmp.fuzzy_diff, cmp.fuzzy_offset)

            if code < 0:
                total_ko += 1
            else:
                total_ok += 1

            # post cleanup
            if not self._args.no_delete:
                try:
                    os.remove(filename_newwav) 
                except:
                    pass
                try:
                    os.remove(filename_oldwav) 
                except:
                    pass

        ts_ed = time.time()
        self._p.info("done: ok=%s, ko=%s, elapsed %ss" % (total_ok, total_ko, ts_ed - ts_st))


    def start(self):
        self._detect_cli()
        self._files.prepare()
        if self._args.performance:
            self._performance()
        else:
            self._compare()


def main():
    args = parse_args()
    if not args:
        return

    try:
        VrtsApp(args).start()
    except ValueError as e:
        print(e)

if __name__ == "__main__":
    main()

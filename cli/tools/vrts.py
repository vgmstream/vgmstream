# VRTS - VGMSTREAM REGRESSION TESTING SCRIPT
#
# Searches for files in a directory (or optionally subdirs) and compares
# the output of two CLI versions, both wav and stdout, for regression
# testing. This creates and deletes temp files, trying to process all
# extensions found unless specified (except a few).

# TODO reject some .wav but not all (detect created by v)
# TODO capture stdout and enable fuzzy depending on codec
# TODO fix -l option, add decode reset option
# TODO multiproc comparator singleton (faster with N files?)

import os, argparse, time, datetime, glob, subprocess, array
import multiprocessing
#import multiprocessing.dummy #fake provs with threads, but not much slower (maybe faster on windows?)

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
    ap.add_argument("-rd","--report-diffs", help="only report full diffs", action='store_true')
    ap.add_argument("-p","--performance-both", help="compare decode performance", action='store_true')
    ap.add_argument("-pn","--performance-new", help="test performance of new CLI", action='store_true')
    ap.add_argument("-po","--performance-old", help="test performance of old CLI", action='store_true')
    ap.add_argument("-pw","--performance-write", help="compare decode+write performance", action='store_true')
    ap.add_argument("-pr","--performance-repeat", help="repeat decoding files N times\n(more files makes easier to see performance changes)", type=int, default=0)
    ap.add_argument("-l","--looping", help="compare looping files (slower)", action='store_true')
    ap.add_argument("-cn","--cli-new", help="sets name of new CLI (can be a path)")
    ap.add_argument("-co","--cli-old", help="sets name of old CLI (can be a path)")
    ap.add_argument("-m","--multiprocesses", help="uses N multiprocesses to compare for performance\n(note that pypy w/ single process is faster than multiprocesses)", type=int, default=1)
    ap.add_argument("-d","--diffs", help="compares input files directly (won't decode)", action='store_true')
    ap.add_argument("-f","--flags", help="sets CLI flags (enclose in double quotes + space, like \" -l 3.0 -F\")")

    args = ap.parse_args()
    
    # derived defaults to simplify
    args.performance = args.performance_both or args.performance_new or args.performance_old or args.performance_write
    args.compare = not args.performance
    if args.performance_both:
        args.performance_new = True
        args.performance_old = True

    return args

###############################################################################

#S16_UNPACK = struct.Struct('<h').unpack_from

# Compares 2 files and returns if contents are the same. If fuzzy is set detects +- PCM changes (slower).
# Has an option to use multiprocesses, mainly noticeable with big (N-ch + 100MB) files.
class VrtsComparator:
    CHUNK_HEADER = 0x50
    CHUNK_SIZE = 0x00100000 * 10 #1MB * N
    END_SIGNAL = None

    def __init__(self, path1, path2, fuzzy_max=0, concurrency=1):
        self._path1 = path1
        self._path2 = path2
        self._fuzzy_max = fuzzy_max
        self._concurrency = concurrency
        self._offset = 0
        self.fuzzy_count = 0
        self.fuzzy_diff = 0
        self.fuzzy_offset = 0

    # compares PCM16LE bytes allowing +-N diffs between PCM bytes
    # useful when comparing output from floats, that can change slightly due to compiler optimizations
    def _test_fuzzy(self, b1, b2):
        len1 = len(b1)
        len2 = len(b2)
        if len1 != len2:
            return RESULT_SAME

        self.fuzzy_count = 0

        b1_array = array.array('h') #LE
        b1_array.frombytes(b1)
        b2_array = array.array('h') #LE
        b2_array.frombytes(b2)

        max = self._fuzzy_max
        for i in range(len(b1_array)):
            #pos = i * 2
            pcm1 = b1_array[i]
            pcm2 = b2_array[i]

            # slower than pre-loaded array
            #pcm1, = S16_UNPACK(b1, pos)
            #pcm2, = S16_UNPACK(b2, pos)

            # slower than struct unpack
            #pcm1 = b1[pos+0] | (b1[pos+1] << 8)
            #if pcm1 & 0x8000:
            #    pcm1 -= 0x10000
            #pcm2 = b2[pos+0] | (b2[pos+1] << 8)
            #if pcm2 & 0x8000:
            #    pcm2 -= 0x10000

            if not (pcm1 >= pcm2 - max and pcm1 <= pcm2 + max):
                #print("%i vs %i +- %i at %x" % (pcm1, pcm2, max, self._offset + pos))
                #self.fuzzy_diff = pcm1 - pcm2
                #self.fuzzy_offset = self._offset + pos
                return RESULT_DIFFS

        self.fuzzy_count = 1
        return 0

    def _test_bytes(self, b1, b2):
        # even though python is much slower than C this test is reasonably fast (internally implemented in C probably)
        if b1 == b2:
            return RESULT_SAME

        # different: fuzzy check if same
        if self._fuzzy_max:
            return self._test_fuzzy(b1, b2)

        return RESULT_DIFFS

    def _worker_multi(self, queue, result, fuzzies):
        while True:
            item = queue.get()

            if item == self.END_SIGNAL:
                # done
                break

            if result.value < 0:
                # consume queue but don't stop, must wait for end signal
                continue

            b1, b2 = item
            cmp = self._test_bytes(b1, b2)
            if cmp < 0:
                result.value = cmp
                # mark but don't stop, must wait for end signal
                continue

            if self.fuzzy_count:
                fuzzies.value += self.fuzzy_count
                continue

    def _compare_multi(self, f1, f2):
        concurrency = self._concurrency

        # reads chunks and passes them to validator workers in parallel
        result = multiprocessing.Value('i', 0) #new shared "i"nt 
        fuzzies = multiprocessing.Value('i', 0) #new shared "i"nt 
        queue = multiprocessing.Queue(maxsize=concurrency)

        # init all max procs that will validate
        # (maybe should use Pool but seems to have some issues with shared queues and values)
        procs = []
        for _ in range(concurrency):
            proc = multiprocessing.Process(target=self._worker_multi, args=(queue, result, fuzzies))
            proc.daemon = True #depends on main proc (if main stops proc also stops)
            proc.start()
            procs.append(proc)

        while True:
            # some worker has signaled end (will still wait for END_SIGNAL in queue)
            if result.value < 0:
                break

            b1 = f1.read(self.CHUNK_SIZE)
            b2 = f2.read(self.CHUNK_SIZE)
            if not b1 or not b2:
                break

            # pass chunks to queue
            try:
                queue.put((b1,b2)) #, timeout=15
            except Exception as e: #ctrl+C, etc
                print("queue error:", e)
                break


        # signal all processes to end using queue (safer overall than each stopping on its own)
        for _ in range(concurrency):
            try:
                queue.put(self.END_SIGNAL) #, timeout=15
            except Exception as e: #ctrl+C?
                print("queue error:", e)
                break

        # should be stopped by the above
        for proc in procs:
            try:
                proc.join()
                proc.close()
            except:
                pass

        try:
            queue.close()
        except:
            pass

        self.fuzzy_count = fuzzies.value
        if result.value < 0:
            return result.value
        return RESULT_SAME

    def _compare_single(self, f1, f2):
        while True:
            b1 = f1.read(self.CHUNK_SIZE)
            b2 = f2.read(self.CHUNK_SIZE)
            if not b1 or not b2:
                break

            cmp = self._test_bytes(b1, b2)
            if cmp < 0:
                return cmp
            self._offset += self.CHUNK_SIZE

        return 0

    def _compare_files(self, f1, f2):
        # header not part of fuzzyness (no need to get exact with sizes)
        if self._fuzzy_max:
            b1 = f1.read(self.CHUNK_HEADER)
            b2 = f2.read(self.CHUNK_HEADER)
            cmp = self._test_bytes(b1, b2)
            if cmp < 0:
                return cmp
            self._offset += self.CHUNK_HEADER

        if self._concurrency > 1:
            return self._compare_multi(f1, f2)
        else:
            return self._compare_single(f1, f2)
    

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
    DARK_GRAY = "\033[90m"
    MAGENTA = "\033[35m"
    LIGHT_MAGENTA = "\033[95m"
    
    COLOR_RESULT = {
        RESULT_SAME: WHITE,
        RESULT_FUZZY: LIGHT_CYAN,
        RESULT_NONE: LIGHT_YELLOW,
        RESULT_DIFFS: LIGHT_RED,
        RESULT_SIZES: LIGHT_MAGENTA,
        RESULT_MISSING_NEW: LIGHT_RED,
        RESULT_MISSING_OLD: WHITE,
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

    REPORTS_DIFFS = [
        #RESULT_NONE,
        RESULT_SIZES,
        RESULT_MISSING_NEW,
        RESULT_MISSING_OLD
    ]

    def __init__(self, args):
        self._args = args
        try:
            os.system('color') #win only?
        except:
            pass

    def _print(self, msg, color=None):
        if color:
            print("%s%s%s" % (color, msg, self.RESET))
        else:
            print(msg)


    def result(self, msg, code, fuzzy_diff=0, fuzzy_offset=0):
        text = self.TEXT_RESULT.get(code)
        color = self.COLOR_RESULT.get(code)
        if not text:
            text = code
        msg = "%s: %s" % (msg, text)
        if fuzzy_diff != 0:
            msg += " (%s @0x%x)" % (fuzzy_diff, fuzzy_offset)

        report = True
        if self._args.report_diffs and code not in self.REPORTS_DIFFS:
            report = False

        if report:
            self._print(msg, color)


    def info(self, msg):
        msg = "%s (%s)" % (msg, self._get_date())
        self._print(msg, self.DARK_GRAY)


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

                # ignores non useful files, except on diffs mode that uses them directly
                if not self._args.diffs:
                    _, ext = os.path.splitext(file)
                    if ext.lower() in IGNORED_EXTENSIONS:
                        continue

                self.filenames.append(file)

                # same file N times
                if self._args.performance and self._args.performance_repeat:
                    for _ in range(self._args.performance_repeat):
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
# * python2 needs to define DEVNULL like:
#       with open(os.devnull, 'wb') as DEVNULL: #python2
#           res = subprocess.check_call(args, stdout=DEVNULL, stderr=DEVNULL)

class VrtsProcess:
    # calls N parallel commands; returns True=ok, False=ko, None=wrong command
    def calls(self, args_list, stdout=False):
        max_procs = len(args_list)
        procs = [None] * max_procs
        outputs = [None] * max_procs

        # initial call (may result in error)
        for i, args in enumerate(args_list):
            try:
                procs[i] = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except Exception as e:
                #print("file error: ", e)
                outputs[i] = None #doesn't exists/etc

        # wait and get result
        for i, proc in enumerate(procs):
            if not proc:
                continue
            proc.wait()

            outputs[i] = True
            if proc.returncode != 0:
                outputs[i] = False #non-zero, exists but returns strerr (ex. ran with no args)
            #elif stdout:
            #    outputs[i] = proc.stdout
        return outputs

    # calls single command; returns True=ok, False=ko, None=wrong command
    def call(self, args, stdout=False):
        try:
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
        self._p = VrtsPrinter(args)
        self._cli_new = None
        self._cli_old = None
        self._temp_files = []

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
        if self._args.diffs:
            return
    
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

        if not self._args.looping:
            args.append('-i')
        if self._args.flags:
            flags = self._args.flags.split(" ")
            args.extend(flags)
        
        args.extend(self._files.filenames)
        return args

    def _performance(self):
        # pases all files at once, as it's faster than 1 by 1 (that has to init program every time)
        if self._args.performance_new:
            self._p.info("testing new performance")
            ts_st = time.time()

            args = self._get_performance_args(self._cli_new)
            res = self._prc.call(args)

            ts_ed = time.time()
            self._p.info("done: elapsed %ss" % (ts_ed - ts_st))

        if self._args.performance_old:
            self._p.info("testing old performance")
            ts_st = time.time()

            args = self._get_performance_args(self._cli_old)
            res = self._prc.call(args)

            ts_ed = time.time()
            self._p.info("done: elapsed %ss" % (ts_ed - ts_st))

        #if self._performance_both:
        #   handled above


    # returns max fuzzy count, except for non-fuzzable files (that use int math)
    def _get_fuzzy_count(self, stdout):
        fuzzy = self._args.fuzzy
        if self._args.fuzzy <= 0:
            return 0

        if not stdout or stdout == True:
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

        if not self._args.looping:
            args.append('-i')
        if self._args.flags:
            flags = self._args.flags.split(" ")
            args.extend(flags)

        args.append(filename)
        return args

    def _compare(self):
        ts_st = time.time()
        msg = 'comparing files'
        if self._args.flags:
            msg = "%s [%s]" % (msg, self._args.flags)
        self._p.info(msg)

        total_ok = 0
        total_ko = 0
        for filename in self._files.filenames:
            filename_newwav = filename + ".new.wav"
            filename_oldwav = filename + ".old.wav"
            self._temp_files = [filename_newwav, filename_oldwav]

            # main decode (ignores errors, comparator already checks them)
            args_new = self._get_compare_args(self._cli_new, filename_newwav, filename)
            args_old = self._get_compare_args(self._cli_old, filename_oldwav, filename)

            # call 2 parallel decodes (much faster)
            stdouts = self._prc.calls([args_new, args_old], stdout=True)
            stdout = stdouts[0]
            #stdout = self._prc.call(args_new, stdout=True)
            #self._prc.call(args_old, stdout=False)

            # test results
            fuzzy = self._get_fuzzy_count(stdout)
            cmp = VrtsComparator(filename_newwav, filename_oldwav, fuzzy_max=fuzzy, concurrency=self._args.multiprocesses)
            code = cmp.compare()

            self._p.result(filename, code) #, cmp.fuzzy_diff, cmp.fuzzy_offset

            if code < 0:
                total_ko += 1
            else:
                total_ok += 1
            self.file_cleanup()

        ts_ed = time.time()
        self._p.info("done: ok=%s, ko=%s, elapsed %ss" % (total_ok, total_ko, ts_ed - ts_st))

    def file_cleanup(self):
        if self._args.no_delete:
            return

        for temp_file in self._temp_files:
            try:
                os.remove(temp_file)
            except:
                pass
        self._temp_files = []

    def _diffs(self):
        
        files = self._files.filenames
        print(files)
        for i in range(len(files) - 1):
            curr = files[i]
            next = files[i+1]
            print(curr, next)
            
            fuzzy = self._args.fuzzy
            cmp = VrtsComparator(curr, next, fuzzy_max=fuzzy, concurrency=self._args.multiprocesses)
            code = cmp.compare()
            
            self._p.result(curr, code)

    def start(self):
        self._detect_cli()
        self._files.prepare()
        
        if self._args.diffs:
            self._diffs()
        elif self._args.performance:
            self._performance()
        else:
            self._compare()


def main():
    args = parse_args()
    if not args:
        return

    # doesn't seem to be a default way to detect program shutdown on windows
    #try:
    #    import win32api
    #    win32api.SetConsoleCtrlHandler(func, True)
    #except ImportError:
    #    pass
    #import signal
    #signal.signal(signal.SIGBREAK, signal.default_int_handler) #
    #signal.signal(signal.SIGINT, signal.default_int_handler) #
    #signal.signal(signal.SIGTERM, signal.default_int_handler) #

    app = VrtsApp(args)
    try:
        app.start()
    except KeyboardInterrupt as e:
        app.file_cleanup()
    except ValueError as e:
        app.file_cleanup()
        print(e)

if __name__ == "__main__":
    main()

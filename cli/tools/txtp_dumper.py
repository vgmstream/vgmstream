#!/usr/bin/env python3
import os, glob, argparse

#******************************************************************************
# TXTP DUMPER
#
# Creates .txtp from a text list, mainly .m3u with virtual txtp to actual .txtp
# example:
# 
# # %TITLE Stage 1
# bgm.awb#1 .txtp
# >> creates bgm.awb#1 .txtp
#
# data/bgm.fsb #2 : bgm_field.txtp
# >> creates bgm_field.txtp full txtp, with data/bgm.fsb #2 inside
#
#******************************************************************************

class Cli(object):
    def _parse(self):
        description = (
            "Makes TXTP from list"
        )
        epilog = (
            "examples:\n"
            "  %(prog)s !tags.m3u\n"
            "  - make .txtp per line in !tags.m3u\n\n"
            "  %(prog)s !tags.m3u -m \n"
            "  - make full txtp rather than mini-txtp\n    (may overwrite files when using subsongs)\n\n"
            "  %(prog)s lines.txt -s sound \n"
            "  - make .txtp per line, setting a subdir\n\n"
            "text file example:\n"
            " # creates a mini txtp as-is\n"
            " bgm.fsb #1 .txtp\n"
            " # creates a txtp using the name after ':' with the body before it\n"
            " bgm.fsb #2 : bgm_field.txtp\n"
        )

        p = argparse.ArgumentParser(description=description, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
        p.add_argument('files', help="Files to get (wildcards work)", nargs='+')
        p.add_argument('-s',  dest='subdir', help="Create .txtp pointing to subdir")
        p.add_argument('-m',  dest='maxitxtp', help="Create regular txtp rather than mini-txtp", action='store_true')
        p.add_argument('-f',  dest='force', help="Make .txtp even if line isn't a .txtp", action='store_true')
        p.add_argument('-o',  dest='output', help="Output dir (default: current)")
        #p.add_argument('-n',  dest='normalize', help="Normalize .txtp formatting")
        return p.parse_args()

    def start(self):
        args = self._parse()
        if not args.files:
            return
        App(args).start()

#******************************************************************************

class App(object):
    def __init__(self, args):
        self.args = args

    def start(self):
        print("TXTP dumper start")
        filenames = []
        for filename in self.args.files:
            filenames += glob.glob(filename)

        for filename in filenames:
            path = self.args.output or '.'
            if self.args.output:
                try:
                    os.makedirs(self.args.output)
                except OSError:
                    pass

            count = 0
            with open(filename,'r', encoding='utf-8-sig') as fi:
                for line in fi:
                    line = line.strip()
                    line = line.rstrip()
                    if line.startswith('#'):
                        continue

                    if not line.endswith('.txtp'):
                        if self.args.force:
                            line += '.txtp'
                        else:
                            continue

                    line.replace('\\', '/')


                    subdir = self.args.subdir
                    if ':' in line:
                        index = line.find(':') #internal txtp : txtp name

                        text = line[0:index].strip()
                        name = line[index+1:].strip()

                    elif self.args.maxitxtp or subdir:
                        index = line.find('.') #first extension

                        if line[index:].startswith('.txtp'): #???
                            continue

                        name = line[0:index] + '.txtp'

                        text = line.replace('.txtp', '').strip()
                        if subdir:
                            subdir.replace('\\', '/')
                            if not subdir.endswith('/'):
                                subdir = subdir + '/'
                            text = subdir + text
                    else:
                        # should be a mini-txtp, but if name isn't "file.ext.txtp" and just "file.txtp",
                        # probably means proper txtp exists and should't be created (when generating from !tags.m3u)
                        name = line
                        text = ''

                        basename = os.path.basename(name)
                        subname, _ = os.path.splitext(basename)
                        _, subext = os.path.splitext(subname)
                        if not subext:
                            print("ignored pre-txtp: %s" % (basename))
                            continue

                    outpath = os.path.join(path, name)

                    with open(outpath, 'w') as fo:
                        if text:
                            fo.write(text)
                        pass
                    count += 1

                if not count:
                    print("%s: no .txtp found" % (filename))
                else:
                    print("%s: total %i .txtp" % (filename, count))

if __name__ == "__main__":
    Cli().start()

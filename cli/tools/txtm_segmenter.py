# !/usr/bin/python

import os
import sys
import glob
import argparse
import re


def parse():
    description = (
        "creates segmented .txtp from a list of files obtained using wildcards"
    )
    epilog = (
        "examples:\n"
        "%(prog)s bgm_*.ogg\n"
        "- get all files that start with bgm_ and end with .ogg\n"
        "%(prog)s bgm_??.* -n bgm_main.txtp -cls 2\n"
        "- get all files that start with bgm_ and end with 2 chars plus any extension\n"
        "%(prog)s files/bgm_*_all.ogg -s\n"
        "- create single .txtp per every bgm_(something)_all.ogg inside files dir\n"
        "%(prog)s **/*.ogg -l\n"
        "- find all .ogg in all subdirs but list only\n"
        "%(prog)s files/*.ogg -f .+(a|all)[.]ogg$\n"
        "- find all .ogg in files except those that end with 'a.ogg' or 'all.ogg'\n"
        "%(prog)s files/*.ogg -f .+(00[01])[.]ogg$\n"
        "- find all .ogg in files that end with '0.ogg' or '1.ogg'\n"
    )

    parser = argparse.ArgumentParser(description=description, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("files", help="files to match")
    parser.add_argument("-n","--name", help="generated txtp name (adapts 'files' by default)")
    parser.add_argument("-f","--filter", help="filter matched files with regex and keep rest")
    parser.add_argument("-i","--include", help="include matched files with regex and ignore rest")
    parser.add_argument("-s","--single", help="generate single files per list match", action='store_true')
    parser.add_argument("-l","--list", help="list only results and don't write", action='store_true')
    parser.add_argument("-cls","--command-loop-start", help="sets loop start segment")
    parser.add_argument("-cle","--command-loop-end", help="sets loop end segment")
    parser.add_argument("-cv","--command-volume", help="sets volume")
    parser.add_argument("-c","--command", help="sets any command (free text)")

    return parser.parse_args()


def is_file_ok(args, glob_file):
    if not os.path.isfile(glob_file):
        return False

    if glob_file.endswith(".py"):
        return False

    if args.filter:
        filename_test = os.path.basename(glob_file)
        p = re.compile(args.filter)
        if p.match(filename_test) != None:
            return False

    if args.include:
        filename_test = os.path.basename(glob_file)
        p = re.compile(args.include)
        if p.match(filename_test) == None:
            return False

    return True

def get_txtp_name(args, segment):
    txtp_name = ''
    
    if   args.name:
        txtp_name = args.name

    elif args.single:
        txtp_name = os.path.splitext(os.path.basename(segment))[0]

    else:
        txtp_name = os.path.splitext(os.path.basename(args.files))[0]

        txtp_name = txtp_name.replace('*', '')
        txtp_name = txtp_name.replace('?', '')

        if txtp_name.endswith('_'):
            txtp_name = txtp_name[:-1]
        if txtp_name == '':
            txtp_name = 'bgm'

    if not txtp_name.endswith(".txtp"):
        txtp_name += ".txtp"
    return txtp_name

def main(): 
    args = parse()

    # get target files
    glob_files = glob.glob(args.files)

    # process matches and add to output list
    files = []
    segments = []
    for glob_file in glob_files:
        if not is_file_ok(args, glob_file):
            continue

        if args.single:
            name = get_txtp_name(args, glob_file)
            segments = [glob_file]
            files.append( (name,segments) )
            
        else:
            segments.append(glob_file)

    if not args.single:
        name = get_txtp_name(args, '')
        files.append( (name,segments) )

    if not files or not segments:
        print("no files found")
        exit()


    # list info
    for name, segments in files:
        print("file: " + name)
        for segment in segments:
            print("  " + segment)

    if args.list:
        exit()

    # write resulting files
    for name, segments in files:
        with open(name,"w+") as ftxtp:
            for segment in segments:
                ftxtp.write(segment + "\n")
            if args.command_loop_start:
                ftxtp.write("loop_start_segment = " + args.command_loop_start + "\n")
            if args.command_loop_end:
                ftxtp.write("loop_end_segment = " + args.command_loop_end + "\n")
            if args.command_volume:
                ftxtp.write("commands = #@volume " + args.command_volume + "\n")
            if args.command:
                ftxtp.write(args.command.replace("\\n", "\n") + "\n")


if __name__ == "__main__":
    main()

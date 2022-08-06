# !/usr/bin/python

import os, glob, argparse, re


def parse():
    description = (
        "creates segmented .txtp from a list of files obtained using wildcards"
    )
    epilog = (
        'examples:\n'
        '%(prog)s bgm_*.ogg\n'
        '- get all files that start with bgm_ and end with .ogg\n'
        '%(prog)s bgm_??.* -n bgm_main.txtp -cla\n'
        '- get all files that start with bgm_ and end with 2 chars plus any extension + loop\n'
        '%(prog)s files/bgm_*_all.ogg -s\n'
        '- create single .txtp per every bgm_(something)_all.ogg inside files dir\n'
        '%(prog)s **/*.ogg -l\n'
        '- list results only\n'
        '%(prog)s files/*.ogg -fi .+(00[01])[.]ogg$\n'
        '- find all .ogg in files including those that end with 0.ogg or 1.ogg\n'
        '%(prog)s files/*.ogg -fe .+(a|all)[.]ogg$\n'
        '- find all .ogg in files excluding those that end with a.ogg or all.ogg\n'
        '%(prog)s files/*.* -fi "(.+)(_intro|_loop)([.].+)$" -n "\\1_full" -cla\n'
        '- makes intro+loop .txtp named (first part without _intro/_loop)_full.txtp + loops\n'
        '%(prog)s files/*.* -fe "(.+)(_intro|_loop)([.].+)$" -s\n'
        '- makes single .txtp for files that don\'t have intro+loop pairs\n'
    )

    parser = argparse.ArgumentParser(description=description, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("files", help="files to match")
    parser.add_argument("-n","--name", help="generated txtp name (auto from 'files' by default)\nMay use regex groups like '\\1.ogg' when used with filter-include")
    parser.add_argument("-fi","--filter-include", help="include files matched with regex and ignore rest")
    parser.add_argument("-fe","--filter-exclude", help="exclude files matched with regex and keep rest")
    parser.add_argument("-s","--single", help="generate single files per list match", action='store_true')
    parser.add_argument("-l","--list", help="list only results and don't write .txtp", action='store_true')
    parser.add_argument("-cla","--command-loop-auto", help="sets auto-loop (last segment)", action='store_true')
    parser.add_argument("-clf","--command-loop-force", help="sets auto-loop (last segment) even with 1 segment", action='store_true')
    parser.add_argument("-cls","--command-loop-start", help="sets loop start segment")
    parser.add_argument("-cle","--command-loop-end", help="sets loop end segment")
    parser.add_argument("-cv","--command-volume", help="sets volume")
    parser.add_argument("-c","--command", help="sets any command (free text)")
    parser.add_argument("-ci","--command-inline", help="sets any inline command (free text)")

    return parser.parse_args()


def is_file_ok(args, file):
    if not os.path.isfile(file):
        return False

    if file.endswith(".py"):
        return False

    file_test = os.path.basename(file)
    if args.filter_exclude:
        if args.p_exclude.match(file_test) != None:
            return False

    if args.filter_include:
        if args.p_include.match(file_test) == None:
            return False

    return True

def get_txtp_name(args, file):
    txtp_name = ''

    if   args.filter_include and args.name and '\\' in args.name:
        file_test = os.path.basename(file)
        txtp_name = args.p_include.sub(args.name, file_test)

    elif   args.name:
        txtp_name = args.name

    elif args.single:
        txtp_name = os.path.splitext(os.path.basename(file))[0]

    else:
        txtp_name = os.path.splitext(os.path.basename(args.files))[0]

        txtp_name = txtp_name.replace('*', '')
        txtp_name = txtp_name.replace('?', '')

        if txtp_name.endswith('_'):
            txtp_name = txtp_name[:-1]

    if not txtp_name:
        txtp_name = 'bgm'

    if not txtp_name.endswith(".txtp"):
        txtp_name += ".txtp"

    return txtp_name

def main(): 
    args = parse()

    if args.filter_include:
        args.p_include = re.compile(args.filter_include, re.IGNORECASE)
    if args.filter_exclude:
        args.p_exclude = re.compile(args.filter_exclude, re.IGNORECASE)

    # get target files
    files = glob.glob(args.files)

    # process matches and add to output list
    txtps = {}
    for file in files:
        if not is_file_ok(args, file):
            continue

        name = get_txtp_name(args, file)
        if not name in txtps:
            txtps[name] = []
        txtps[name].append(file)


    if not txtps:
        print("no files found")
        exit()


    # list info
    for name, segments in txtps.items():
        print("file: " + name)
        for segment in segments:
            print("  " + segment)

    if args.list:
        exit()

    # write resulting files
    for name, segments in txtps.items():
        len_segments = len(segments)
        with open(name,"w+") as ftxtp:
            for i, segment in enumerate(segments):
                command_inline = ''
                if args.command_inline:
                    command_inline = args.command_inline.replace("\\n", "\n")
            
                if args.command_loop_force and len_segments == 1:
                    txtp_line = "%s #e%s\n" % (segment, command_inline)
                else:
                    txtp_line = "%s%s\n" % (segment, command_inline)
                ftxtp.write(txtp_line)

            if args.command_loop_auto or args.command_loop_force and len_segments > 1:
                ftxtp.write("loop_mode = auto\n")
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

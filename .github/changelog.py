# generates changelog
# NOTE: this is just a quick fix, feel free to handle releases more cleanly
import subprocess, urllib.request, json, datetime

USE_GIT = True
GIT_MAX_MERGES = 10
JSON_MAX_MERGES = 10
JSON_LOCAL = True
MAKE_SHORT_CHANGELOG = False

def convert_git(stdout):
    lines = stdout.split('\n')

    # split into groups of commit/author/etc/text 
    groups = []
    curr = -1
    for idx, line in enumerate(lines):
        if line.startswith('commit '):
            if curr >= 0:
                groups.append(lines[curr:idx])
            curr = idx

    groups.append(lines[curr:idx])

    # assumed to use --small
    items = []
    for group in groups:
        item = {}

        # not consistent (may include 'merge' msgs)
        for idx, line in enumerate(group):
            lw = line.lower()
            #if lw.startswith('author'):
            #    item['author'] = line[7:].strip()
            if lw.startswith('date'):
                item['date'] = line[5:].strip().replace('"','')
            if not line:
                item['message'] = '\n'.join(group[idx:])
                break
        items.append(item)
    return items

# get lastest commits, examples:
#   git log --max count 5 --merges
#   git --no-pager log --after="2020-02-01" --format=medium
def load_git():
    if not USE_GIT:
        raise ValueError("git disabled")

    args = ['git', 'describe', '--tags', '--abbrev=0']
    proc = subprocess.run(args, capture_output=True)
    if proc.returncode != 0:
        raise ValueError("git exception")
    latest_tag = proc.stdout.decode('utf-8').strip()

    # no apparent portable way to get a utc timezone in date=format
    #args = ['git','--no-pager','log', '--merges', '--date=format:"%Y-%m-%d %H:%M:%S"',  '--max-count', str(GIT_MAX_MERGES) ]
    args = ['git','--no-pager','log', '--merges', '--date=format:"%Y-%m-%d %H:%M:%S %z"', '%s..HEAD' % (latest_tag)]
    proc = subprocess.run(args, capture_output=True)
    if proc.returncode != 0:
        raise ValueError("git exception")
    stdout = proc.stdout.decode('utf-8')
    return convert_git(stdout)


def convert_json(data):
    merges = 0
    items = []
    for data_elem in data:
        commit = data_elem['commit']

        # use "merge" messages that (usually) have useful, formatted info
        message = commit['message']
        if not message.lower().strip().startswith('merge'):
            continue

        # request
        date = commit['author']['date'].replace('T',' ').replace('Z','')

        item = {
            'date': date,
            'message': message,
        }
        items.append(item)

        merges += 1
        if merges > JSON_MAX_MERGES:
            break

    return items

def load_json():
    # see https://docs.github.com/en/rest/commits/commits
    # for reference (needs to be logged in to get artifacts = useless)
    # https://api.github.com/repos/OWNER/REPO/actions/artifacts
    # https://api.github.com/repos/OWNER/REPO/actions/artifacts/ARTIFACT_ID
    # https://api.github.com/repos/OWNER/REPO/actions/workflows/release.yml/runs
    if JSON_LOCAL:
        with open('commits.json', 'r', encoding='utf-8') as f:
            data = json.load(f)
    else:
        contents = urllib.request.urlopen("https://api.github.com/repos/vgmstream/vgmstream/commits?per_page=100").read()
        if len(contents) > 10000000: #?
            raise ValueError("bad call")
        data = json.loads(contents)
    return convert_json(data)


def convert_items(items, lines, short_log):
    for item in items:
        message = item['message']
        date = item['date']

        msg_from = None
        ignore = False
        subs = []
        msg_lines = iter([msg.strip() for msg in message.split('\n')])
        for msg in msg_lines:
            if msg.lower().startswith('merge'):
                if ' into ' in msg: #Merge branch ... into ...
                    ignore = True
                    break
                try:
                    pos = msg.index(' from ')
                    msg_from = msg[pos + 6:].strip()
                except:
                    pass
                continue
            if not msg: #always first?
                continue

            if not msg.startswith('-'):
                msg = '- %s' % (msg)
            if msg.startswith('* '):
                msg = '- %s' % (msg[2:])
            subs.append(msg)

        if ignore:
            continue

        if not subs:
            subs.append('- (not described)')

        if short_log:
            lines.extend(subs)
        else:
            header = "#### %s" % (date.replace('T',' ').replace('Z',''))
            if msg_from:
                header += ' (%s)' % (msg_from)
            lines.append(header)
            lines.extend(subs)
            lines.append('')
            


def write(lines):
    with open('changelog.txt', 'w', encoding="utf-8") as f:
        f.write('\n'.join(lines))

def get_lines(short_log=False):
    if short_log:
        lines = []
    else:
        curr_date = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S %z")
        lines = [
            '### CHANGELOG',
            '(latest changes from previous release, generated on %s)' % (curr_date),
            '',
        ]

    try:
        try:
            items = load_git()
        except Exception as e:
            print("error when generating git, using json:", e)
            items = load_json()
        convert_items(items, lines, short_log)
        
    except Exception as e:
        print("err", e)
        lines.append("(couldn't generate changelog)")
    return lines

def main(short_changelog=False):
    lines = get_lines(short_changelog)
    write(lines)
    return lines

if __name__ == "__main__":
    main(MAKE_SHORT_CHANGELOG)

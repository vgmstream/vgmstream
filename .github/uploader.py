# uploads artifacts to nightly releases
# NOTE: this is just a quick fix, feel free to handle releases more cleanly
import urllib.request, json, argparse, glob, subprocess, os

# to handle nightly releases:
# - create repo (could be in main repo but needs a tag, that is associated with some commit)
# - create release w/ tag, this is were uploads go 
# - upload assets manually or via API below, needs access token
# - assets have a fixed link based on tag = good
#
# To generate access tokens:
# - go to user settings > developer settings
#
# * there are github actions that automate this that could be used, this is based from manual tests
# * also "import github" to use Github(token) that comes with many helpers
#   gh = Github(token)
#   gh_repo = gh.get_repo(repo)
#   gh_release = gh_repo.get_release(tag)
#   assets = gh_release.get_assets()
#   gh_release.update_release(...)
#   gh_release.upload_asset(...)
#
# API: https://docs.github.com/en/rest/releases/releases?apiVersion=2022-11-28
#      https://docs.github.com/en/rest/releases/assets?apiVersion=2022-11-28
#
# Github's API has some limits but hopefully not reached by current token

RELEASE_TAG = 'nightly'
# gives info about release (public)
URL_RELEASE = 'https://api.github.com/repos/vgmstream/vgmstream-releases/releases/tags/nightly'
# allows deleting a single asset
URL_DELETE = 'https://api.github.com/repos/vgmstream/vgmstream-releases/releases/assets/%s'
# allows uploading a single asset
URL_UPLOAD = 'https://uploads.github.com/repos/vgmstream/vgmstream-releases/releases/%s/assets?name=%s'
# change release info
URL_UPDATE = 'https://api.github.com/repos/vgmstream/vgmstream-releases/releases/%s'
# gives info about last vgmstream tag
URL_VGMSTREAM = 'https://api.github.com/repos/vgmstream/vgmstream/releases?per_page=1'

#------------------------------------------------------------------------------

def get_release():
    contents = urllib.request.urlopen(URL_RELEASE).read()
    data = json.loads(contents)
    return data

def get_vgmstream_tag():
    # TODO could use local git tag
    contents = urllib.request.urlopen(URL_VGMSTREAM).read()
    data = json.loads(contents)
    return data[0]['tag_name']

def update_release(release, token, debug, body):
    release_id = release['id']

    args = [
        'curl',
        '-X', 'PATCH',
        '-H', 'Accept: application/vnd.github+json',
        '-H', 'Authorization: Bearer %s' % (token),
        '-H', 'X-GitHub-Api-Version: 2022-11-28',
        URL_UPDATE % (release_id),
        '-d', '{"body":"%s"}' % (body),
    ]
    #-d '{"tag_name":"v1.0.0","target_commitish":"master","name":"v1.0.0","body":"...","draft":false,"prerelease":false}'

    print("* updating release text")
    if debug:
        print(' '.join(args))
    else:
        subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def delete_asset(release, token, debug, file):
    basename = os.path.basename(file)

    asset_id = None
    for asset in release['assets']:
        if asset['name'].lower() == basename.lower():
            asset_id = asset['id']
            break
    if not asset_id:
        print("asset id not found")
        return

    args = [
        'curl',
        '-X', 'DELETE',
        '-H', 'Accept: application/vnd.github+json',
        '-H', 'Authorization: Bearer %s' % (token),
        '-H', 'X-GitHub-Api-Version: 2022-11-28',
        URL_DELETE % (asset_id),
    ]

    print("* deleting old asset %s (%s)" % (file, asset_id))
    if debug:
        print(' '.join(args))
    else:
        subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

def upload_asset(release, token, debug, file):
    basename = os.path.basename(file)
    release_id = release['id']
    
    args = [
        'curl',
        '-X', 'POST',
        '-H', 'Accept: application/vnd.github+json',
        '-H', 'Authorization: Bearer %s' % (token),
        '-H', 'X-GitHub-Api-Version: 2022-11-28',
        '-H', 'Content-Type: application/octet-stream',
        URL_UPLOAD % (release_id, basename),
        '--data-binary', '@%s' % (file)
    ]

    print("* uploading asset %s" % (file))
    if debug:
        print(' '.join(args))
    else:
        subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

#------------------------------------------------------------------------------

def generate_changelog(release, token, debug):
    print("* generating changelog")
    try:
        import changelog
        # writes in work dir and gets lines
        lines = changelog.main() 

        current_tag = get_vgmstream_tag()
        body = [
            'Automated releases ([full diffs here](https://github.com/vgmstream/vgmstream/compare/%s...master)).' % (current_tag),
            '',
            '',
            '<details>'
            '<summary>Recent changes</summary>',
            '', #important, collapsable doesn't work otherwise
        ]
        body.extend(lines)
        body.extend([
            '</details>'
        ])
        body_text = '\\n'.join(body)
        body_text = body_text.replace('"', '\"')

        file = "changelog.txt"
        update_release(release, token, debug, body_text)
        delete_asset(release, token, debug, file)
        upload_asset(release, token, debug, file)

        #with open("body.json",'w') as f:
        #    f.write('{"body":"%s"}' % (body_text))

        #with open("changelog.txt",'rb') as f:
        #    print(f.read())

    except Exception as e:
        print("couldn't generate changelog", e)


def main(args):
    print("starting asset uploader")

    files = []
    for file_glob in args.files:
        files += glob.glob(file_glob)

    # allow for changelog only
    #if not files:
    #    raise ValueError("no files found")

    # shouldn't happen (points to non-existing files)
    if args.files and not files:
        raise ValueError("no files found, expected: %s" % (args.files))

    # this token usually only exists in env on merges, but allow passing for tests
    token = args.token
    if not token:
        token = os.environ.get('UPLOADER_GITHUB_TOKEN')
    if not token:
        print("token not defined")
        raise ValueError("token not defined")

    print("handling %s files" % (len(files)))
    try:
        release = get_release()
        for file in files:
            delete_asset(release, token, args.debug, file)
            upload_asset(release, token, args.debug, file)
    except Exception as e:
        print("error during process: %s" % (e))
        raise ValueError("could't upload")

    # this should be invoked separately so release doesn't change per artifact
    if args.changelog:
        generate_changelog(release, token, args.debug)

    print("done")

def parse_args():
    description = (
        "uploads artifacts to releases"
    )
    epilog = None

    ap = argparse.ArgumentParser(description=description, epilog=epilog, formatter_class=argparse.RawTextHelpFormatter)
    ap.add_argument("files", help="files to upload", nargs='*')
    ap.add_argument("-t","--token", help="security token")
    ap.add_argument("-c","--changelog", help="update changelog as well", action="store_true")
    ap.add_argument("-x","--debug", help="no actions", action="store_true")

    args = ap.parse_args()
    return args

if __name__ == "__main__":
    args = parse_args()
    main(args)

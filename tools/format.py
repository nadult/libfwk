#!/usr/bin/env python3

import argparse, os, re, subprocess, shutil


def find_clang_format_cmd():
    names = ['clang-format-17', 'clang-format']
    for name in names:
        if shutil.which(name) is not None:
            return name
    print("clang-format is missing")
    exit(1)


def verify_clang_format(cmd):
    result = subprocess.run([cmd, '--version'], stdout=subprocess.PIPE)
    tokens = result.stdout.decode("utf-8").split()
    while len(tokens) > 0 and tokens[0] != 'clang-format':
        tokens.pop(0)
    if (
        result.returncode != 0
        or len(tokens) < 3
        or tokens[0] != 'clang-format'
        or tokens[1] != 'version'
    ):
        print('error while checking clang-format version')
        print("version string: " + str(tokens))
        exit(1)
    version = tokens[2].split('.', 2)
    print(f"clang-format version: {version[0]}.{version[1]}.{version[2]}")

    if int(version[0]) < 17:
        print("clang-format is too old; At least version 17 is required")
        exit(1)


def find_cpp_files():
    out = []
    regex = re.compile('(.*\.h$)|(.*\.cpp$)')
    for root_dir in ['include', 'src']:
        for root, dirs, files in os.walk(root_dir):
            for file in files:
                if regex.match(file):
                    out.append(os.path.join(root, file))
    return out


def format_cpp(cf_cmd, check: bool):
    print("Checking C++ code formatting..." if check else "Formatting C++ code...")
    full_command = [cf_cmd, '-i']
    if check:
        full_command += ['--dry-run', '-Werror']
    full_command += find_cpp_files()
    result = subprocess.run(full_command)

    if check:
        if result.returncode != 0:
            exit(1)
        print("All OK")


def main():
    parser = argparse.ArgumentParser(
        prog='libfwk-format',
        description='Tool for code formatting and format verification',
    )
    parser.add_argument('-c', '--check', action='store_true')
    args = parser.parse_args()

    libfwk_path = os.path.join(__file__, '..')

    clang_format_cmd = find_clang_format_cmd()
    verify_clang_format(clang_format_cmd)

    format_cpp(clang_format_cmd, args.check)


if __name__ == "__main__":
    main()

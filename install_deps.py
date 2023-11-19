#!/usr/bin/env python3

import subprocess, os, re, json, shutil, argparse, zipfile, urllib.request
from dataclasses import dataclass


@dataclass
class PackageInfo:
    name: str
    version: str
    revision_id: str
    package_id: str
    settings: dict
    options: dict

    def pattern(self):
        return f"{self.name}/{self.version}:{self.package_id}"


@dataclass
class PackageQuery:
    name: str
    version: str
    query: str = ""

    def pattern(self):
        return f"{self.name}/{self.version}"


def parse_packages_json(query: str, json_text: str) -> list[PackageInfo]:
    local_cache = json.loads(json_text)['Local Cache']
    if 'error' in local_cache:
        return []
    revs = local_cache[query.pattern()]['revisions']
    out = []
    for rev_id in revs.keys():
        packages = revs[rev_id]["packages"]
        for package_id in packages.keys():
            info = packages[package_id]["info"]
            settings = info.get("settings", {})
            options = info.get("options", {})
            out.append(
                PackageInfo(
                    query.name, query.version, rev_id, package_id, settings, options
                )
            )
    return out


def list_packages(query: PackageQuery) -> list[PackageInfo]:
    command = ['conan', 'list', '-c', '-f', 'json', f"{query.pattern()}:*"]
    if query.query:
        command += ['-p', query.query]
    # print("List command: " + str(command))

    result = subprocess.run(command, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Error while getting package info: {query.pattern()}")
        exit(1)
    return parse_packages_json(query, result.stdout.decode("utf-8"))


def download_packages(query: PackageQuery) -> list[PackageInfo]:
    command = ['conan', 'download', '-r', 'conancenter', query.pattern(), '-f', 'json']
    if query.query:
        command += ['-p', query.query]
    # print("Download command: " + str(command))

    result = subprocess.run(command, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Error while downloading dependency: {query.pattern()}")
        exit(1)
    return parse_packages_json(query, result.stdout.decode("utf-8"))


def get_best_package(packages: list[PackageInfo]) -> PackageInfo:
    assert packages
    best = packages[0]
    for i in range(1, len(packages)):
        cur = packages[i]
        if cur.settings["compiler.version"] > best.settings["compiler.version"]:
            best = cur
    return best


def get_package_path(info: PackageInfo):
    command = ['conan', 'cache', 'path', info.pattern()]
    result = subprocess.run(command, stdout=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Error while retrieving package path for: {info.pattern()}")
        exit(1)
    return result.stdout.decode("utf-8").split()[0]


def copy_subdirs(package_name: str, dst_dir: str, src_dir: str, subdirs: list[str]):
    for subdir in subdirs:
        dst_subdir = os.path.join(dst_dir, subdir)
        src_subdir = os.path.join(src_dir, subdir)
        if package_name == 'freetype' and subdir == 'include':
            src_subdir = os.path.join(src_subdir, 'freetype2')
        os.makedirs(dst_subdir, exist_ok=True)
        if os.path.isdir(src_subdir):
            shutil.copytree(src_subdir, dst_subdir, dirs_exist_ok=True)


windows_query = "os=Windows and arch=x86_64 and options.shared=False"

package_queries = [
    PackageQuery("bzip2", "1.0.8", windows_query),
    PackageQuery("brotli", "1.0.9", windows_query),
    PackageQuery("libpng", "1.6.39", windows_query),
    PackageQuery("freetype", "2.11.1", windows_query),
    PackageQuery("ogg", "1.3.5", windows_query),
    PackageQuery("openal", "1.21.1", windows_query),
    PackageQuery("sdl", "2.26.1", windows_query),
    PackageQuery("vorbis", "1.3.7", windows_query),
    PackageQuery("zlib", "1.3", windows_query),
    PackageQuery("vulkan-headers", "1.3.236.0"),
    PackageQuery("vulkan-loader", "1.3.236.0", "os=Windows and arch=x86_64"),
    # TODO: shaderc is broken :(
    # PackageQuery("shaderc", "2021.1", "os=Windows"),
]


def get_shaderc(install_path: str):
    print('Downloading shaderc package...')
    file_name = 'shaderc-2021-01-windows.zip'
    url = f'https://github.com/nadult/libfwk/releases/download/v0.1/{file_name}'
    if os.path.isfile(file_name):
        os.remove(file_name)
    urllib.request.urlretrieve(url, file_name)
    print(f"Extracting shaderc...")
    zip_file = zipfile.ZipFile(file_name, 'r')
    zip_file.extractall(install_path)
    zip_file.close()
    os.remove(file_name)


def main():
    parser = argparse.ArgumentParser(
        prog='libfwk-install-deps',
        description='Installs all the necessary dependencies for building libfwk on Windows',
    )
    parser.add_argument('install_path')
    args = parser.parse_args()

    if os.name != "nt":
        print(
            "Currently the only purpose of install_deps.py is to install dependencies on Windows"
        )
        exit(1)

    install_path = args.install_path
    install_path = os.path.join(install_path, 'x86_64')
    print(f"Installing packages at: {install_path}")
    os.makedirs(install_path, exist_ok=True)
    get_shaderc(install_path)

    # TODO: multithreading
    for query in package_queries:
        packages = list_packages(query)
        if not packages:
            print(f"Downloading: {query.pattern()}")
            packages = download_packages(query)
        if not packages:
            print(f"Didn't download any matching packages for: {query.pattern()}")
            exit(1)

        package = get_best_package(packages)
        package_path = get_package_path(package)

        print(f"Installing {package.pattern()} from: {package_path}")
        copy_subdirs(
            package.name, install_path, package_path, ['include', 'lib', 'bin']
        )


if __name__ == "__main__":
    main()

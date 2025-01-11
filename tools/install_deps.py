#!/usr/bin/env python

import subprocess, os, json, shutil, argparse
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
    local_cache = json.loads(json_text)["Local Cache"]
    if "error" in local_cache:
        return []
    revs = local_cache[query.pattern()]["revisions"]
    out = []
    for rev_id in revs.keys():
        packages = revs[rev_id]["packages"]
        for package_id in packages.keys():
            info = packages[package_id]["info"]
            settings = info.get("settings", {})
            options = info.get("options", {})
            out.append(
                PackageInfo(query.name, query.version, rev_id, package_id, settings, options)
            )
    return out


def check_conan_version():
    conan_version = None
    min_version = 2

    try:
        result = subprocess.run(["conan", "--version"], stdout=subprocess.PIPE)
        tokens = result.stdout.decode("utf-8").split()
        if len(tokens) >= 3 and tokens[0] == "Conan" and tokens[1] == "version":
            conan_version = [int(x) for x in tokens[2].split(".")]
    except Exception as e:
        print(f"Exception while checking conan version: {e}")
        pass

    if not conan_version:
        raise Exception(f"Conan not found. At least version {min_version} is required.")
    if len(conan_version) < 3 or conan_version[0] < 2:
        raise Exception(
            f"Invalid conan version: {conan_version} At least version {min_version} is required."
        )


def list_packages(query: PackageQuery) -> list[PackageInfo]:
    command = ["conan", "list", "-c", "-f", "json", f"{query.pattern()}:*"]
    if query.query:
        command += ["-p", query.query]
    # print("List command: " + str(command))

    result = subprocess.run(command, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Error while getting package info: {query.pattern()}")
        exit(1)
    return parse_packages_json(query, result.stdout.decode("utf-8"))


def download_packages(query: PackageQuery) -> list[PackageInfo]:
    command = ["conan", "download", "-r", "conancenter", query.pattern(), "-f", "json"]
    if query.query:
        command += ["-p", query.query]
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
    command = ["conan", "cache", "path", info.pattern()]
    result = subprocess.run(command, stdout=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Error while retrieving package path for: {info.pattern()}")
        exit(1)
    return result.stdout.decode("utf-8").split()[0]


def copy_subdirs(package_name: str, dst_dir: str, src_dir: str, subdirs: list[str]):
    for subdir in subdirs:
        dst_subdir = os.path.join(dst_dir, subdir)
        src_subdir = os.path.join(src_dir, subdir)
        if package_name == "freetype" and subdir == "include":
            src_subdir = os.path.join(src_subdir, "freetype2")
        os.makedirs(dst_subdir, exist_ok=True)
        if os.path.isdir(src_subdir):
            shutil.copytree(src_subdir, dst_subdir, dirs_exist_ok=True)


def parse_package_queries(json_path):
    queries = []
    with open(json_path, "r") as file:
        json_data = json.load(file)
        for package in json_data["packages"]:
            name = package["name"]
            version = package["version"]
            query = package.get("query", "")
            assert isinstance(name, str)
            assert isinstance(version, str)
            assert isinstance(query, str)
            queries.append(PackageQuery(name, version, query))
    return queries


def parse_arguments():
    parser = argparse.ArgumentParser(
        prog="install_deps",
        description="Installs all the necessary dependencies for building libfwk on Windows",
    )
    parser.add_argument(
        "--deps-file",
        type=str,
        help=(
            "Path to the file with a list of packages to install. "
            "If not specified then dependencies.json in current directory will be used."
        ),
    )
    parser.add_argument(
        "--install_dir",
        type=str,
        help=(
            "Target directory where libraries will be installed. "
            "By default the will be installed in windows/libraries/ inside libfwk's directory."
        ),
    )
    return parser.parse_args()


def libfwk_path():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    args = parse_arguments()
    if os.name != "nt":
        print("Currently the only purpose of install_deps.py is to install dependencies on Windows")
        exit(1)

    check_conan_version()

    deps_file_path = args.deps_file
    if deps_file_path is None:
        deps_file_path = os.path.join(os.getcwd(), "dependencies.json")
    assert os.path.isfile(deps_file_path)
    package_queries = parse_package_queries(deps_file_path)
    package_names = [package.name for package in package_queries]
    print(f"Packages to install: {package_names}")

    install_dir = args.install_dir
    if install_dir is None:
        install_dir = os.path.join(libfwk_path(), "windows", "libraries")
    print(f"Installing into: {install_dir}")
    os.makedirs(install_dir, exist_ok=True)

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
        copy_subdirs(package.name, install_dir, package_path, ["include", "lib", "bin"])


if __name__ == "__main__":
    main()

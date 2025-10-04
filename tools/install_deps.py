#!/usr/bin/env python

import argparse, hashlib, json, shutil, os, re, ssl, subprocess
from dataclasses import dataclass, field
from urllib.parse import urlparse
from typing import Optional

# TODO: better naming, especially differentiation between packages which can be downloaded
# from potentially unsafe sources (like libfwk github) from those which can be built locally
# or downloaded from conan center.

# TODO: building sdl3 & assimp
# TODO: building missing packages on ubuntu
# TODO: ubuntu & windows package building in github action (runnable on demand)


def copy_subdirs(package_name: str, dst_dir: str, src_dir: str, subdirs: list[str]):
    for subdir in subdirs:
        dst_subdir = os.path.join(dst_dir, subdir)
        src_subdir = os.path.join(src_dir, subdir)
        if package_name == "freetype" and subdir == "include":
            src_subdir = os.path.join(src_subdir, "freetype2")
        os.makedirs(dst_subdir, exist_ok=True)
        if os.path.isdir(src_subdir):
            shutil.copytree(src_subdir, dst_subdir, dirs_exist_ok=True)


# Computes first 64 bits (16 hex digits) of sha256 checksum
def compute_sha256_64(file_path: str) -> str:
    sha256 = hashlib.sha256()
    with open(file_path, "rb") as f:
        for chunk in iter(lambda: f.read(8192), b""):
            sha256.update(chunk)
    return sha256.hexdigest()[:16]


# Downloads a file from a given URL to a given destination path.
# Function does not verify SSL certificates, so the results must be verified in some way by the caller.
def download_file_unsafe(url: str, dest_path: str):
    import urllib.request
    context = ssl._create_unverified_context()
    with urllib.request.urlopen(url, context=context) as response, open(
        dest_path, "wb"
    ) as out_file:
        shutil.copyfileobj(response, out_file)


def libfwk_path():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# =================================== dependencies.json parsing ===================================


@dataclass
class PackageCache:
    type: str
    platform: str
    url: str
    # Tuples: package_name:version:hash
    packages: list[str]


@dataclass
class ConanRecipes:
    queries: dict[str, str] = field(default_factory=dict)
    # Triples: package_name:version:query_name
    packages: list[str] = field(default_factory=list)


@dataclass
class DependenciesJson:
    dependencies: list[str] = field(default_factory=list)
    package_caches: list[PackageCache] = field(default_factory=list)
    conan_recipes: ConanRecipes = field(default_factory=ConanRecipes)
    # Tuples: package_name:version
    custom_recipes: list[str] = field(default_factory=list)


# Checks if strings in a list match a given regex pattern fully
def _validate_strings(title: str, strings: list[str], pattern: str):
    matcher = re.compile(rf"^{pattern}$")
    invalid = [s for s in strings if not matcher.fullmatch(s)]
    if invalid:
        raise ValueError(f"Invalid {title}: {invalid}. Must match pattern: {pattern}")


def _validate_string_options(title: str, string: str, options: list[str]):
    if not isinstance(string, str):
        raise TypeError(f"{title} must be a string")
    if string not in options:
        raise ValueError(f"Invalid {title}: {string}. Must be one of: {options}")


def _validate_url(url: str):
    parsed = urlparse(url)
    if parsed.scheme not in ("http", "https") or not parsed.netloc:
        raise ValueError(f"Invalid URL: {url}")


def parse_dependencies_json(json_data: dict) -> DependenciesJson:
    package_source_types = ["github_release", "custom"]
    package_source_platforms = ["windows", "linux"]

    package_name_pattern = r"[a-z][a-z0-9-]+"
    query_name_pattern = r"[a-z][a-z0-9-]+"
    version_pattern = r"\d+(\.\d+)*(-\d+)?"
    sha256_64_pattern = r"[a-f0-9]{16}"

    package_name_version_pattern = rf"{package_name_pattern}:{version_pattern}"
    package_name_version_hash_pattern = (
        rf"{package_name_pattern}:{version_pattern}:{sha256_64_pattern}"
    )
    package_name_version_query_pattern = (
        rf"{package_name_pattern}:{version_pattern}:{query_name_pattern}"
    )

    # Parsing dependencies
    dependencies = json_data.get("dependencies", [])
    if not isinstance(dependencies, list) or not all(isinstance(d, str) for d in dependencies):
        raise TypeError("dependencies must be a list of strings")
    _validate_strings("dependencies", dependencies, package_name_pattern)

    # Parsing package caches
    package_caches = []
    for cache in json_data.get("package-caches", []):
        package_cache = PackageCache(
            type=cache["type"],
            platform=cache["platform"],
            url=cache.get("url", ""),
            packages=cache.get("packages", []),
        )
        _validate_string_options("package-cache.type", package_cache.type, package_source_types)
        _validate_string_options(
            "package-cache.platform", package_cache.platform, package_source_platforms
        )
        _validate_url(package_cache.url)
        _validate_strings(
            "package-cache.packages", package_cache.packages, package_name_version_hash_pattern
        )
        package_caches.append(package_cache)

    # Parsing conan recipes
    conan_recipes = ConanRecipes()
    if "conan-recipes" in json_data:
        conan_recipes_data = json_data.get("conan-recipes", {})
        conan_recipes.queries = conan_recipes_data.get("queries", {})
        conan_recipes.packages = conan_recipes_data.get("packages", [])
        if not isinstance(conan_recipes.queries, dict) or not all(
            isinstance(k, str) and isinstance(v, str) for k, v in conan_recipes.queries.items()
        ):
            raise TypeError("conan-recipes.queries must be a dictionary of strings")
        if not isinstance(conan_recipes.packages, list) or not all(
            isinstance(p, str) for p in conan_recipes.packages
        ):
            raise TypeError("conan-recipes.packages must be a list of strings")

        _validate_strings(
            "conan-recipes.queries keys",
            list(conan_recipes.queries.keys()),
            query_name_pattern,
        )
        _validate_strings(
            "conan-recipes.packages", conan_recipes.packages, package_name_version_query_pattern
        )

    # Parsing custom recipes
    custom_recipes = json_data.get("custom-recipes", [])
    if not isinstance(custom_recipes, list) or not all(isinstance(p, str) for p in custom_recipes):
        raise TypeError("custom-recipes must be a list of strings")
    _validate_strings("custom-recipes", custom_recipes, package_name_version_pattern)

    return DependenciesJson(dependencies, package_caches, conan_recipes, custom_recipes)


# =================================== Conan related functions =====================================


@dataclass
class ConanPackageInfo:
    name: str
    version: str
    revision_id: str
    package_id: str
    settings: dict
    options: dict

    def pattern(self):
        return f"{self.name}/{self.version}:{self.package_id}"


# TODO: rename to ConanRecipeQuery?
@dataclass
class ConanPackageQuery:
    name: str
    version: str
    query: str = ""

    def pattern(self):
        return f"{self.name}/{self.version}"


def conan_parse_packages_json(query: str, json_text: str) -> list[ConanPackageInfo]:
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
                ConanPackageInfo(query.name, query.version, rev_id, package_id, settings, options)
            )
    return out


def conan_check_version():
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


def conan_list_packages(query: ConanPackageQuery) -> list[ConanPackageInfo]:
    command = ["conan", "list", "-c", "-f", "json", f"{query.pattern()}:*"]
    if query.query:
        command += ["-p", query.query]
    # print("List command: " + str(command))

    result = subprocess.run(command, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Error while getting package info: {query.pattern()}")
        exit(1)
    return conan_parse_packages_json(query, result.stdout.decode("utf-8"))


def conan_download_packages(query: ConanPackageQuery) -> list[ConanPackageInfo]:
    command = ["conan", "download", "-r", "conancenter", query.pattern(), "-f", "json"]
    if query.query:
        command += ["-p", query.query]
    print("Download command: " + str(command))

    result = subprocess.run(command, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Error while downloading dependency: {query.pattern()}")
        exit(1)
    return conan_parse_packages_json(query, result.stdout.decode("utf-8"))


def conan_get_best_package(packages: list[ConanPackageInfo]) -> ConanPackageInfo:
    assert packages
    best = packages[0]
    for i in range(1, len(packages)):
        cur = packages[i]
        if cur.settings["compiler.version"] > best.settings["compiler.version"]:
            best = cur
    return best


def conan_get_package_path(info: ConanPackageInfo):
    command = ["conan", "cache", "path", info.pattern()]
    result = subprocess.run(command, stdout=subprocess.PIPE)
    if result.returncode != 0:
        print(f"Error while retrieving package path for: {info.pattern()}")
        exit(1)
    return result.stdout.decode("utf-8").split()[0]


# ========================================= Main script ===========================================

# TODO: update commands and help


def parse_arguments():
    parser = argparse.ArgumentParser(
        prog="install_deps",
        description="Installs dependencies needed for building libfwk or "
        "libfwk-based projects on Windows.",
    )

    parser.add_argument(
        "--deps-file",
        type=str,
        help="Path to the file with a list of packages to install. "
        "If not specified then dependencies.json in current directory will be used.",
    )
    parser.add_argument(
        "--target-dir",
        type=str,
        help="Target directory where libraries will be installed. "
        "By default it is 'windows/libraries/' inside libfwk's directory.",
    )
    parser.add_argument(
        "--package-dir",
        type=str,
        help="Directory where downloaded packages will be stored. "
        "By default it is 'windows/packages/' inside libfwk's directory.",
    )

    # TODO: change these to subcommands
    # TODO: build is not a good name
    parser.add_argument(
        "--download",
        action="store_true",
        help="Download prebuilt packages from storages.",
    )

    parser.add_argument(
        "--build",
        action="store_true",
        help="Don't download prebuilt packages from storage, instead get them from "
        "conan or build them locally.",
    )

    parser.add_argument(
        "--clean",
        action="store_true",
        help="If specified then the target directory will be cleaned before "
        "installing new libraries.",
    )

    parser.add_argument("--package", action="store_true", help="Build downloadable packages.")

    parser.add_argument(
        "--list-downloadable",
        action="store_true",
        help="Lists packages which can be downloaded.",
    )

    parser.add_argument(
        "--list-buildable",
        action="store_true",
        help="Lists packages which can be built .",
    )

    parser.add_argument(
        "packages",
        nargs="*",
        metavar="PACKAGE",
        help="Packages to install. If omitted, installs all dependencies from dependencies.json.",
    )

    return parser.parse_args()


def get_cached_package_names(deps: DependenciesJson) -> list[str]:
    names = []
    for cache in deps.package_caches:
        for pkg in cache.packages:
            names.append(pkg.split(":")[0])
    return list(set(names))


def get_buildable_package_names(deps: DependenciesJson) -> list[str]:
    names = []
    for pkg in deps.conan_recipes.packages:
        names.append(pkg.split(":")[0])
    for pkg in deps.custom_recipes:
        names.append(pkg.split(":")[0])
    return list(set(names))


def find_conan_recipe(deps_json: DependenciesJson, package_name: str) -> Optional[ConanPackageQuery]:
    conan_recipes = deps_json.conan_recipes
    for pkg in conan_recipes.packages:
        name, version, query_name = pkg.split(":")
        if name == package_name:
            query = conan_recipes.queries.get(query_name)
            assert query is not None
            return ConanPackageQuery(package_name, version, query)
    return None


# Returns package version from a given recipe or None
def find_custom_recipe(deps_json: DependenciesJson, package_name: str) -> Optional[str]:
    for pkg in deps_json.custom_recipes:
        name, version = pkg.split(":")
        if name == package_name:
            return version
    return None


# Returns a tuple (version, full_url, hash)
def find_package_in_caches(deps: DependenciesJson, package_name: str) -> Optional[tuple[str, str]]:
    for cache in deps.package_caches:
        for pkg in cache.packages:
            name, version, hash = pkg.split(":")
            if name == package_name:
                return (version, cache.url + f"/{name}_{version}.zip", hash)
    return None


def build_package(deps: DependenciesJson, package_name: str, target_dir: str):
    conan_recipe = find_conan_recipe(deps, package_name)
    if conan_recipe:
        packages = conan_list_packages(conan_recipe)
        if not packages:
            print(f"Downloading: {conan_recipe.pattern()}")
            packages = conan_download_packages(conan_recipe)
        if not packages:
            print(f"Didn't download any matching packages for: {conan_recipe.pattern()}")
            exit(1)

        package = conan_get_best_package(packages)
        package_path = conan_get_package_path(package)

        print(f"Installing {package.pattern()} from: {package_path}")
        copy_subdirs(package.name, target_dir, package_path, ["include", "lib", "bin"])
        return package.version

    custom_version = find_custom_recipe(deps, package_name)
    if custom_version:
        print("TODO: custom build not implemented yet")
        return custom_version

    assert False, f"Don't know how to build package: '{package_name}'"


def download_package(deps: DependenciesJson, package_name: str, target_dir: str, package_dir: str):
    cached_package = find_package_in_caches(deps, package_name)
    if not cached_package:
        print(f"Don't know how to download package: {package_name}")
        exit(1)

    (version, url, hash) = cached_package
    print(f"Downloading package: {package_name}")
    print(f"  Version: {version}")
    print(f"  URL: {url}")

    package_file_name = f"{package_name}_{version}.zip"
    package_file_path = os.path.join(package_dir, package_file_name)
    if os.path.isfile(package_file_path) and compute_sha256_64(package_file_path) == hash:
        print(f"Package already downloaded: {package_file_path}")
    else:
        download_file_unsafe(url, package_file_path)
        downloaded_hash = compute_sha256_64(package_file_path)
        if downloaded_hash != hash:
            print(f"Error: downloaded file has invalid hash: {package_file_path}")
            print(f"File hash: {downloaded_hash} expected: {hash}")
            exit(1)

    shutil.unpack_archive(package_file_path, target_dir)


# Komendy do zaimplementowania:
# - zbudowanie / zainstalowanie  wybranego/wybranych pakietów albo wszystkich wymaganych pakietów
# - zrobienie cleana w docelowym katalogu


def load_deps_json(deps_file_path: Optional[str]) -> DependenciesJson:
    if deps_file_path is None:
        deps_file_path = os.path.join(os.getcwd(), "dependencies.json")
    assert os.path.isfile(deps_file_path)
    with open(deps_file_path, "r") as file:
        return parse_dependencies_json(json.load(file))


def clean_target_dir(target_dir: str):
    print(f"Cleaning target directory: {target_dir}")
    valid_subdirs = ["include", "lib", "bin", "share"]
    if os.path.isdir(target_dir):
        for subdir in os.listdir(target_dir):
            if subdir in valid_subdirs:
                shutil.rmtree(os.path.join(target_dir, subdir))
            else:
                print(f"Warning: unexpected subdirectory in target dir: {subdir}")


def zip_directory(target_dir: str, file_name_base: Optional[str] = None) -> Optional[str]:
    assert os.path.isdir(target_dir), f"Cannot zip directory: {target_dir}"

    file_name_base = file_name_base or os.path.basename(target_dir)
    zip_path_base = os.path.join(os.path.dirname(target_dir), file_name_base)
    zip_path = zip_path_base + ".zip"
    if os.path.exists(zip_path):
        os.remove(zip_path)
    print(f"Creating archive: {zip_path}")
    shutil.make_archive(zip_path_base, "zip", root_dir=target_dir, base_dir=".")
    return zip_path


def prepare_target_dir(dir: str, clean: bool, verbose: bool = True):
    assert not os.path.isfile(dir)
    if clean and os.path.isdir(dir):
        if verbose:
            print(f"Cleaning target directory: {dir}")
        shutil.rmtree(dir)
    os.makedirs(dir, exist_ok=True)


def main():
    args = parse_arguments()
    if os.name != "nt":
        print("Currently the only purpose of install_deps.py is to install dependencies on Windows")
        exit(1)

    deps_json = load_deps_json(args.deps_file)
    downloadable_packages = get_cached_package_names(deps_json)
    buildable_packages = get_buildable_package_names(deps_json)
    available_packages = buildable_packages if args.build else downloadable_packages

    if args.list_downloadable:
        print(f"Downloadable packages: {downloadable_packages}")
        return
    if args.list_buildable:
        print(f"Buildable packages: {buildable_packages}")
        return

    selected_deps = args.packages
    if not selected_deps:
        selected_deps = deps_json.dependencies

    # TODO: this is probably not needed
    unavailable_packages = [dep for dep in selected_deps if dep not in available_packages]
    if unavailable_packages:
        print(f"The following requested packages are not available: {unavailable_packages}")
        exit(1)

    # TODO: multithreading for downloading

    if args.download:
        default_target_dir = os.path.join(libfwk_path(), "windows", "libraries")
        default_package_dir = os.path.join(libfwk_path(), "windows", "packages")
        target_dir = os.path.abspath(args.target_dir or default_target_dir)
        package_dir = os.path.abspath(args.package_dir or default_package_dir)
        # TODO: shouldn't download command clean the target dir by default? especially when installing all packages
        prepare_target_dir(target_dir, args.clean)
        prepare_target_dir(package_dir, False)
        for dep in selected_deps:
            download_package(deps_json, dep, target_dir, package_dir)

    elif args.build:
        default_target_dir = os.path.join(libfwk_path(), "windows", "libraries")
        target_dir = os.path.abspath(args.target_dir or default_target_dir)
        prepare_target_dir(target_dir, args.clean)
        for dep in selected_deps:
            build_package(deps_json, dep, target_dir)

    elif args.package:
        target_dir = args.target_dir
        if target_dir is None:
            target_dir = os.path.abspath(os.path.join("build", "packages"))
        download_strings = []
        for dep in selected_deps:
            package_target_dir = os.path.join(target_dir, dep)
            prepare_target_dir(package_target_dir, True)
            version = build_package(deps_json, dep, package_target_dir)
            file_name_base = f"{dep}_{version}"
            zip_path = zip_directory(package_target_dir, file_name_base)
            hash = compute_sha256_64(zip_path)
            download_strings.append(f"{dep}:{version}:{hash}")

        print("Built packages (name:version:hash):")
        for download_string in download_strings:
            print(f"  {download_string}")


if __name__ == "__main__":
    main()

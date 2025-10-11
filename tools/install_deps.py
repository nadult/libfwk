#!/usr/bin/env python

import argparse, hashlib, json, shutil, os, re, ssl, subprocess
from dataclasses import dataclass, field
from urllib.parse import urlparse
from typing import Optional

# TODO: better naming, especially differentiation between packages which can be downloaded
# from potentially unsafe sources (like libfwk github) from those which can be built locally
# or downloaded from conan center.

# TODO: Add option to build only specific targets
# TODO: better installed files filtering
# TODO: option to compile library in release mode with /MDd
# TODO: option to compile multiple versions of a given library (release, release-mdd, debug)
# TODO: renaming of static libraries to have consistent suffixes
# TODO: improve logging, maybe add some colors?
# TODO: rename windows/libraries to windows/dependencies?
# TODO: cleaning target directory vs cleaning build directory?
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


def zip_directory(target_dir: str, zip_path_base: Optional[str] = None) -> Optional[str]:
    assert os.path.isdir(target_dir), f"Cannot zip directory: {target_dir}"

    zip_path_base = zip_path_base or target_dir
    zip_path = zip_path_base + ".zip"
    if os.path.exists(zip_path):
        os.remove(zip_path)
    print(f"Creating archive: {zip_path}")
    shutil.make_archive(zip_path_base, "zip", root_dir=target_dir, base_dir=".")
    return zip_path


def libfwk_path():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def check_commands_available(commands: list[str]) -> bool:
    for command in commands:
        if shutil.which(command) is None:
            raise RuntimeError(
                f"Command missing: {command} please install it first before running this script"
            )


# =================================== dependencies.json parsing ===================================
# =================================================================================================

# TODO: move classes to different sections?


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
class CMakeRecipe:
    name: str
    version: str
    github_project: str
    source_branch: str
    source_sha1: str
    after_checkout_commands: list[str] = field(default_factory=list)
    cmake_options: dict[str, str] = field(default_factory=dict)
    install_subdirs: list[str] = field(default_factory=list)


@dataclass
class DependenciesJson:
    dependencies: list[str] = field(default_factory=list)
    package_caches: list[PackageCache] = field(default_factory=list)
    conan_recipes: ConanRecipes = field(default_factory=ConanRecipes)
    # Tuples: package_name:version
    cmake_recipes: list[str] = field(default_factory=list)


class _PatternValidator:
    def __init__(self, pattern: str):
        self.pattern = pattern
        self.matcher = re.compile(rf"^{pattern}$")

    # Checks if strings in a list match a given regex pattern fully
    def validate(self, title: str, strings: list[str]):
        invalid = [s for s in strings if not isinstance(s, str) or not self.matcher.fullmatch(s)]
        if invalid:
            raise ValueError(f"Invalid {title}: {invalid}. Must match pattern: {self.pattern}")


def _validate_options(title: str, value: str, options: list[str]):
    if not isinstance(value, str):
        raise TypeError(f"{title} must be a string")
    if value not in options:
        raise ValueError(f"Invalid {title}: {value}. Must be one of: {options}")


def _validate_url(url: str):
    parsed = urlparse(url)
    if parsed.scheme not in ("http", "https") or not parsed.netloc:
        raise ValueError(f"Invalid URL: {url}")


def _validate_list_type(title: str, value, expected_type):
    if not isinstance(value, list) or not all(isinstance(v, expected_type) for v in value):
        raise TypeError(f"{title} must be a list of {expected_type.__name__}")


def _validate_dict_type(title: str, value, expected_key_type, expected_value_type):
    if not isinstance(value, dict) or not all(
        isinstance(k, expected_key_type) and isinstance(v, expected_value_type)
        for k, v in value.items()
    ):
        raise TypeError(
            f"{title} must be a dictionary of {expected_key_type.__name__} to {expected_value_type.__name__}"
        )


def parse_dependencies_json(json_data: dict) -> DependenciesJson:
    package_source_types = ["github_release", "custom"]
    package_source_platforms = ["windows", "linux"]

    package_name_pattern = r"[a-z][a-z0-9-]+"
    query_name_pattern = r"[a-z][a-z0-9-]+"
    version_pattern = r"\d+(\.\d+)*(-\d+)?"
    sha256_64_pattern = r"[a-f0-9]{16}"

    branch_name_checker = _PatternValidator(r"[A-Za-z0-9_.-]+")
    sha1_checker = _PatternValidator(r"[a-f0-9]{40}")
    github_project_checker = _PatternValidator(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+")

    package_name_checker = _PatternValidator(package_name_pattern)
    query_name_checker = _PatternValidator(query_name_pattern)
    package_name_version_checker = _PatternValidator(rf"{package_name_pattern}:{version_pattern}")
    package_name_version_hash_checker = _PatternValidator(
        rf"{package_name_pattern}:{version_pattern}:{sha256_64_pattern}"
    )
    package_name_version_query_checker = _PatternValidator(
        rf"{package_name_pattern}:{version_pattern}:{query_name_pattern}"
    )

    # Parsing dependencies
    dependencies = json_data.get("dependencies", [])
    _validate_list_type("dependencies", dependencies, str)
    package_name_checker.validate("dependencies", dependencies)

    # Parsing package caches
    package_caches = []
    for cache in json_data.get("package-caches", []):
        package_cache = PackageCache(
            type=cache["type"],
            platform=cache["platform"],
            url=cache.get("url", ""),
            packages=cache.get("packages", []),
        )
        _validate_options("package-cache.type", package_cache.type, package_source_types)
        _validate_options(
            "package-cache.platform", package_cache.platform, package_source_platforms
        )
        _validate_url(package_cache.url)
        package_name_version_hash_checker.validate("package-cache.packages", package_cache.packages)
        package_caches.append(package_cache)

    # Parsing conan recipes
    conan_recipes = ConanRecipes()
    if "conan-recipes" in json_data:
        conan_recipes_data = json_data.get("conan-recipes", {})
        conan_recipes.queries = conan_recipes_data.get("queries", {})
        conan_recipes.packages = conan_recipes_data.get("packages", [])
        _validate_dict_type("conan-recipes.queries", conan_recipes.queries, str, str)
        _validate_list_type("conan-recipes.packages", conan_recipes.packages, str)
        query_name_checker.validate(
            "conan-recipes.queries keys", list(conan_recipes.queries.keys())
        )
        package_name_version_query_checker.validate(
            "conan-recipes.packages", conan_recipes.packages
        )

    # Parsing cmake recipes
    cmake_recipes = []
    for cmake_recipe in json_data.get("cmake-recipes", []):
        if not isinstance(cmake_recipe, dict):
            raise TypeError("cmake-recipes must be a list of dictionaries")
        name_version = cmake_recipe.get("package")
        package_name_version_checker.validate("cmake-recipes.package", [name_version])
        name, version = name_version.split(":")
        github_project = cmake_recipe.get("github-project")
        source_branch = cmake_recipe.get("source-branch")
        source_sha1 = cmake_recipe.get("source-sha1")

        after_checkout_commands = cmake_recipe.get("after-checkout-commands", [])
        _validate_list_type("cmake-recipes.after-checkout-commands", after_checkout_commands, str)

        cmake_options = cmake_recipe.get("cmake-options", {})
        _validate_dict_type("cmake-recipes.cmake-options", cmake_options, str, str)

        install_subdirs = cmake_recipe.get("install-subdirs", [])
        _validate_list_type("cmake-recipes.install-subdirs", install_subdirs, str)

        github_project_checker.validate("cmake-recipes.github-project", [github_project])
        branch_name_checker.validate("cmake-recipes.source-branch", [source_branch])
        sha1_checker.validate("cmake-recipes.source-sha1", [source_sha1])
        cmake_recipes.append(
            CMakeRecipe(
                name=name,
                version=version,
                github_project=github_project,
                source_branch=source_branch,
                source_sha1=source_sha1,
                after_checkout_commands=after_checkout_commands,
                cmake_options=cmake_options,
                install_subdirs=install_subdirs,
            )
        )

    return DependenciesJson(dependencies, package_caches, conan_recipes, cmake_recipes)


def load_deps_json(deps_file_path: Optional[str]) -> DependenciesJson:
    if deps_file_path is None:
        deps_file_path = os.path.join(os.getcwd(), "dependencies.json")
    assert os.path.isfile(deps_file_path)
    with open(deps_file_path, "r") as file:
        return parse_dependencies_json(json.load(file))


# =================================== Conan related functions =====================================
# =================================================================================================


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


# ====================================== Package caches ===========================================
# =================================================================================================


def get_cached_package_names(deps: DependenciesJson) -> list[str]:
    names = []
    for cache in deps.package_caches:
        for pkg in cache.packages:
            names.append(pkg.split(":")[0])
    return list(set(names))


# Returns a tuple (version, full_url, hash)
def find_package_in_caches(deps: DependenciesJson, package_name: str) -> Optional[tuple[str, str]]:
    for cache in deps.package_caches:
        for pkg in cache.packages:
            name, version, hash = pkg.split(":")
            if name == package_name:
                return (version, cache.url + f"/{name}_{version}.zip", hash)
    return None


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


# ====================================== Package building =========================================
# =================================================================================================


@dataclass
class BuildOptions:
    clean_build: bool = False
    skip_download_source: bool = False

    @staticmethod
    def parse(args: argparse.Namespace) -> "BuildOptions":
        self = BuildOptions()
        self.clean_build = args.clean_build
        self.skip_download_source = args.skip_download_source
        return self


def get_buildable_package_names(deps: DependenciesJson) -> list[str]:
    names = []
    for pkg in deps.conan_recipes.packages:
        names.append(pkg.split(":")[0])
    for recipe in deps.cmake_recipes:
        names.append(recipe.name)
    return list(set(names))


def find_conan_recipe(
    deps_json: DependenciesJson, package_name: str
) -> Optional[ConanPackageQuery]:
    conan_recipes = deps_json.conan_recipes
    for pkg in conan_recipes.packages:
        name, version, query_name = pkg.split(":")
        if name == package_name:
            query = conan_recipes.queries.get(query_name)
            assert query is not None
            return ConanPackageQuery(package_name, version, query)
    return None


def find_cmake_recipe(deps_json: DependenciesJson, package_name: str) -> Optional[CMakeRecipe]:
    for recipe in deps_json.cmake_recipes:
        if recipe.name == package_name:
            return recipe
    return None


def is_git_repository(dir: str) -> bool:
    return os.path.isdir(os.path.join(dir, ".git"))


def github_update_source(
    github_project: str, branch: str, sha1: str, dest_dir: str, skip_download: bool = False
):
    if not skip_download:
        repo_url = f"https://github.com/{github_project}.git"

        if is_git_repository(dest_dir):
            subprocess.run(["git", "fetch"], cwd=dest_dir, check=True)
            subprocess.run(["git", "checkout", branch], cwd=dest_dir, check=True)
            subprocess.run(["git", "pull", "origin", branch], cwd=dest_dir, check=True)
        else:
            git_clone_cmd = ["git", "clone", "--branch", branch, repo_url, dest_dir]
            subprocess.run(git_clone_cmd, check=True)

    current_sha1 = (
        subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=dest_dir, check=True, stdout=subprocess.PIPE
        )
        .stdout.decode("utf-8")
        .strip()
    )
    if current_sha1 != sha1:
        subprocess.run(["git", "checkout", sha1], cwd=dest_dir, check=True)


def configure_cmake_package(
    cmake_recipe: CMakeRecipe, source_dir: str, build_dir: str, install_dir: str
):
    os.makedirs(build_dir, exist_ok=True)
    cmake_cmd = ["cmake", "-S", source_dir, "-DCMAKE_INSTALL_PREFIX=" + install_dir]
    cmake_cmd += [f"-D{key}={value}" for key, value in cmake_recipe.cmake_options.items()]

    print("CMake command: " + str(cmake_cmd))
    subprocess.run(cmake_cmd, cwd=build_dir, check=True)


def build_cmake_package(
    cmake_recipe: CMakeRecipe, target_dir: str, build_dir: str, options: BuildOptions
) -> str:
    print(f"Building cmake package: {cmake_recipe.name} (version: {cmake_recipe.version})")

    src_dir = os.path.join(build_dir, "src")
    install_dir = os.path.join(build_dir, "install")
    build_dir = os.path.join(build_dir, "build")

    check_commands_available(["git", "cmake"])

    github_update_source(
        cmake_recipe.github_project,
        cmake_recipe.source_branch,
        cmake_recipe.source_sha1,
        src_dir,
        options.skip_download_source,
    )

    for cmd in cmake_recipe.after_checkout_commands:
        print(f"Running command: {cmd}")
        subprocess.run(cmd, cwd=src_dir, check=True, shell=True)

    prepare_target_dir(build_dir, options.clean_build, False)
    prepare_target_dir(install_dir, True, False)
    configure_cmake_package(cmake_recipe, src_dir, build_dir, install_dir)

    build_cmd = ["cmake", "--build", ".", "--config", "Release", "--target", "install"]
    print("Build command: " + str(build_cmd))
    subprocess.run(build_cmd, cwd=build_dir, check=True)

    install_subdirs = cmake_recipe.install_subdirs or ["include", "lib", "bin"]
    copy_subdirs(cmake_recipe.name, target_dir, install_dir, install_subdirs)

    return cmake_recipe.version


def build_package(
    deps: DependenciesJson,
    package_name: str,
    target_dir: str,
    build_dir: str,
    options: BuildOptions,
) -> str:
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

    cmake_recipe = find_cmake_recipe(deps, package_name)
    if cmake_recipe:
        version = build_cmake_package(cmake_recipe, target_dir, build_dir, options)
        return version

    assert False, f"Don't know how to build package: '{package_name}'"


# ========================================= Main script ===========================================
# =================================================================================================

# TODO: update commands and help


def parse_arguments() -> argparse.Namespace:
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
        "--clean",
        action="store_true",
        help="If specified then the target directory will be cleaned before "
        "installing new libraries. When downloading all packages this is done by default.",
    )

    sub_parsers = parser.add_subparsers(help="Available commands:", dest="command")

    download_parser = sub_parsers.add_parser(
        "download", help="Download prebuilt packages from caches."
    )
    download_parser.add_argument(
        "--package-dir",
        type=str,
        help="Directory where downloaded packages will be stored. "
        "By default it is 'windows/packages/' inside libfwk's directory.",
    )

    build_parser = sub_parsers.add_parser(
        "build",
        help="Build packages locally by using recipies or by downloading it from conan center.",
    )
    package_parser = sub_parsers.add_parser(
        "package", help="Build packages which can later be downloaded from caches."
    )

    sub_parsers.add_parser(
        "list",
        help="List packages which can be downloaded or built.",
    )

    for sub_parser in [build_parser, package_parser]:
        sub_parser.add_argument(
            "--clean-build",
            action="store_true",
            help="Build directory will be cleaned before configuring & building.",
        )

    build_parser.add_argument(
        "--skip-download-source",
        action="store_true",
        help="Skip downloading sources if they are already present in the source directory.",
    )

    for sub_parser, help_text in [
        (
            download_parser,
            "Selection of packages to download. If omitted all required dependencies will be downloaded.",
        ),
        (
            build_parser,
            "Selection of packages to build. If omitted all buildable packages will be built.",
        ),
        (
            package_parser,
            "Selection of packages to build and package. "
            "If omitted all buildable packages will be built and packaged.",
        ),
    ]:
        sub_parser.add_argument(
            "packages",
            nargs="*",
            metavar="PACKAGE",
            help=help_text,
        )

    return parser.parse_args()


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

    if args.command == "list":
        print(f"Downloadable packages: {downloadable_packages}")
        print(f"Buildable packages: {buildable_packages}")
        return

    selected_deps = args.packages
    if not selected_deps:
        selected_deps = deps_json.dependencies

    available_packages = downloadable_packages if args.command == "download" else buildable_packages
    unavailable_packages = [dep for dep in selected_deps if dep not in available_packages]
    if unavailable_packages:
        print(f"The following requested packages are not available: {unavailable_packages}")
        exit(1)

    # TODO: multithreading for downloading & unpacking?
    if args.command == "download":
        default_target_dir = os.path.join(libfwk_path(), "windows", "libraries")
        default_package_dir = os.path.join(libfwk_path(), "windows", "packages")
        target_dir = os.path.abspath(args.target_dir or default_target_dir)
        package_dir = os.path.abspath(args.package_dir or default_package_dir)

        if args.packages is None and args.clean is None:
            args.clean = True

        prepare_target_dir(target_dir, args.clean)
        prepare_target_dir(package_dir, False)
        for dep in selected_deps:
            download_package(deps_json, dep, target_dir, package_dir)

    elif args.command == "build":
        default_target_dir = os.path.join(libfwk_path(), "windows", "libraries")
        target_dir = os.path.abspath(args.target_dir or default_target_dir)
        build_dir = os.path.join(libfwk_path(), "build", "dependencies")
        build_options = BuildOptions.parse(args)

        prepare_target_dir(target_dir, args.clean)
        for dep in selected_deps:
            package_build_dir = os.path.join(build_dir, dep)
            build_package(deps_json, dep, target_dir, package_build_dir, build_options)

    elif args.command == "package":
        build_dir = os.path.join(libfwk_path(), "build", "dependencies")
        target_dir = args.target_dir or os.path.join(libfwk_path(), "windows", "packages")
        build_options = BuildOptions.parse(args)

        prepare_target_dir(target_dir, args.clean)
        download_strings = []
        for dep in selected_deps:
            package_build_dir = os.path.join(build_dir, dep)
            package_target_dir = os.path.join(package_build_dir, "install")
            version = build_package(deps_json, dep, target_dir, package_build_dir, build_options)
            zip_file_base = f"{dep}_{version}"
            zip_path = zip_directory(package_target_dir, os.path.join(target_dir, zip_file_base))
            hash = compute_sha256_64(zip_path)
            download_strings.append(f"{dep}:{version}:{hash}")

        print("Built packages (name:version:hash):")
        for download_string in download_strings:
            print(f"  {download_string}")


if __name__ == "__main__":
    main()

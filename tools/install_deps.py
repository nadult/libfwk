#!/usr/bin/env python

# Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
# This file is part of libfwk. See license.txt for details.

import enum
import argparse, hashlib, json, shutil, os, re, ssl, subprocess
from dataclasses import dataclass, field
import sys
from urllib.parse import urlparse
from typing import Optional

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from configure import (
    Generator,
    BuildPlatformInfo,
    generator_options,
    valid_generators,
    build_platform_info,
)

# TODO: better naming, especially differentiation between packages which can be downloaded
# from potentially unsafe sources (like libfwk github) from those which can be built locally
# or downloaded from conan center.

# TODO: iterator_debug_level in shaderc-combined
# TODO: merge install_deps.py into configure.py ?
# TODO: Fix shaderc-combined: it's too large... unity-builds for spirv-opt?
# TODO: better printing for conan command
# TODO: verify that libfwk is buildable with built dependencies? that would be costly?
# TODO: add option to clean build directories?
# TODO: rename windows/libraries to windows/dependencies?
# TODO: building missing packages on ubuntu
# TODO: ubuntu & windows package building in github action (runnable on demand)
# TODO: autoclear when package configuration changed? we could store keys and compare them

# =================================================================================================
# region                                  Common functions
# =================================================================================================


class Color(enum.Enum):
    DEFAULT = 0
    BLACK = 30
    RED = 31
    GREEN = 32
    YELLOW = 33
    BLUE = 34
    MAGENTA = 35
    CYAN = 36
    WHITE = 37
    HI_BLACK = 90
    HI_RED = 91
    HI_GREEN = 92
    HI_YELLOW = 93
    HI_BLUE = 94
    HI_MAGENTA = 95
    HI_CYAN = 96
    HI_WHITE = 97


class Style(enum.Enum):
    REGULAR = 0
    BOLD = 1
    UNDERLINE = 4


def ansi_code(color: Color, style: Style = Style.REGULAR) -> str:
    if color == Color.DEFAULT:
        return f"\033[{style.value}m"
    return f"\033[{style.value};{color.value}m"


def ansi_reset_code() -> str:
    return "\033[0m"


def stylize_text(text: str, color: Color, style: Style = Style.REGULAR) -> str:
    return f"{ansi_code(color, style)}{text}{ansi_reset_code()}"


def print_title(title: str, color: Color = Color.DEFAULT, style: Style = Style.REGULAR):
    title_width = len(title) + 2
    title = f" {ansi_code(color, style)}{title}{ansi_reset_code()} "
    console_width = min(shutil.get_terminal_size((80, 20)).columns, 100)
    left_width = (console_width - title_width) // 2
    right_width = console_width - title_width - left_width
    left_width = max(left_width, 3)
    right_width = max(right_width, 3)
    print(f"\n{'=' * left_width}{title}{'=' * right_width}\n")


def print_color(text: str, color: Color, style: Style = Style.REGULAR):
    print(stylize_text(text, color, style))


def print_step(text: str):
    print_color(f"\n(*) {text}", Color.DEFAULT, Style.BOLD)


def print_error(text: str):
    print_color(text, Color.RED, Style.BOLD)


def copy_subdirs(dst_dir: str, src_dir: str, subdirs: list[str]):
    for subdir in subdirs:
        dst_subdir = os.path.join(dst_dir, subdir)
        src_subdir = os.path.join(src_dir, subdir)
        os.makedirs(dst_subdir, exist_ok=True)
        if os.path.isdir(src_subdir):
            shutil.copytree(src_subdir, dst_subdir, dirs_exist_ok=True)


# File patterns are regexes matching relative paths. Additionally after colon they can have
# a new name for the file. In such case it shouldn't be a regex, but a simple path.
def install_files(dst_dir: str, src_dir: str, file_patterns_or_renames: list[str]):
    # Copy files from src_dir to dst_dir if they match any of the patterns (without fnmatch)
    os.makedirs(dst_dir, exist_ok=True)

    file_patterns = [p for p in file_patterns_or_renames if ":" not in p]
    compiled_patterns = [re.compile(p) for p in file_patterns]
    file_renames = [p.split(":") for p in file_patterns_or_renames if ":" in p]

    for root, _, files in os.walk(src_dir):
        for fname in files:
            src_path = os.path.join(root, fname)
            rel_path = os.path.relpath(src_path, src_dir).replace(os.sep, "/")
            if any(rgx.fullmatch(rel_path) for rgx in compiled_patterns):
                dst_path = os.path.join(dst_dir, *rel_path.split("/"))
                os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                shutil.copy2(src_path, dst_path)
            for old_name, new_name in file_renames:
                if rel_path == old_name:
                    dst_path = os.path.join(dst_dir, *new_name.split("/"))
                    os.makedirs(os.path.dirname(dst_path), exist_ok=True)
                    shutil.copy2(src_path, dst_path)


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
    print_step(f"Creating archive: {zip_path}")

    if os.path.exists(zip_path):
        os.remove(zip_path)
    shutil.make_archive(zip_path_base, "zip", root_dir=target_dir, base_dir=".")
    return zip_path


def libfwk_path():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def check_commands_available(commands: list[str]) -> bool:
    for command in commands:
        if shutil.which(command) is None:
            print_error(
                f"Command missing: {command} please install it first before running this script"
            )
            exit(1)


def find_subdirs(dir: str) -> list[str]:
    return [name for name in os.listdir(dir) if os.path.isdir(os.path.join(dir, name))]


def prepare_dir(dir: str, clean: bool, verbose: bool = True):
    assert not os.path.isfile(dir)
    if clean and os.path.isdir(dir):
        if verbose:
            print(f"Cleaning target directory: {dir}")
        shutil.rmtree(dir)
    os.makedirs(dir, exist_ok=True)


def prepare_target_dir(dir: str, clean: bool):
    if clean:
        for subdir in find_subdirs(dir):
            subdir_path = os.path.join(dir, subdir)
            shutil.rmtree(subdir)
    os.makedirs(dir, exist_ok=True)


def current_platform() -> str:
    if os.name == "nt":
        return "windows"
    elif os.name == "posix":
        return "linux"
    else:
        raise RuntimeError(f"Unsupported platform: {os.name}")


# =================================================================================================
# region                                 Parsing helpers
# =================================================================================================


class _PatternValidator:
    def __init__(self, pattern: str):
        self.pattern = pattern
        self.matcher = re.compile(rf"^{pattern}$")

    # Checks if strings in a list match a given regex pattern fully
    def validate(self, title: str, strings: list[str]):
        invalid = [s for s in strings if not isinstance(s, str) or not self.matcher.fullmatch(s)]
        if invalid:
            raise ValueError(f"Invalid {title}: {invalid}. Must match pattern: {self.pattern}")


def _make_pattern_validators():
    branch_name = r"[A-Za-z0-9_.-]+"
    github_project = r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+"
    package_name = r"[a-z][a-z0-9-]*"
    platform_name = r"(windows|linux)"
    build_type = r"(release|debug)"
    build_suffix = r"[a-z][a-z0-9-]*"
    query_name = r"[a-z][a-z0-9-]*"
    sha256_64 = r"[a-f0-9]{16}"
    sha1 = r"[a-f0-9]{40}"
    version = r"\d+(\.\d+)*(-\d+)?"

    return {
        "branch_name": _PatternValidator(branch_name),
        "platform_name": _PatternValidator(platform_name),
        "build_name": _PatternValidator(rf"({platform_name})?:{build_type}:({build_suffix})?"),
        "github_project": _PatternValidator(github_project),
        "package_name": _PatternValidator(package_name),
        "package_name_version": _PatternValidator(rf"{package_name}:{version}"),
        "package_name_version_hash": _PatternValidator(rf"{package_name}:{version}:{sha256_64}"),
        "package_name_version_query": _PatternValidator(rf"{package_name}:{version}:{query_name}"),
        "query_name": _PatternValidator(query_name),
        "sha1": _PatternValidator(sha1),
    }


_pattern_validators = _make_pattern_validators()


def _validate_pattern(validator_name: str, title: str, values: list[str]):
    validator = _pattern_validators.get(validator_name)
    assert validator
    validator.validate(title, values)


def _validate_options(title: str, value: str, options: list[str]):
    if not isinstance(value, str):
        raise TypeError(f"{title} must be a string")
    if value not in options:
        raise ValueError(f"Invalid {title}: {value}. Must be one of: {options}")


def _validate_url(title: str, url: str):
    parsed = urlparse(url)
    if parsed.scheme not in ("http", "https") or not parsed.netloc:
        raise ValueError(f"Invalid URL in {title}: {url}")


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


# =================================================================================================
# region                                   Common classes
# =================================================================================================


@dataclass
class PackageCache:
    type: str
    platform: str
    url: str
    # Tuples: package_name:version:hash
    packages: list[str]

    @staticmethod
    def parse(json: dict) -> "PackageCache":
        package_source_types = ["github_release", "custom"]
        package_source_platforms = ["windows", "linux"]

        type = json["type"]
        platform = json["platform"]
        url = json["url"]
        packages = json.get("packages", [])

        _validate_options("package-cache.type", type, package_source_types)
        _validate_options("package-cache.platform", platform, package_source_platforms)
        _validate_url("package-cache.url", url)
        _validate_pattern("package_name_version_hash", "package-cache.packages", packages)

        return PackageCache(type, platform, url, packages)

    def matches_platform(self, platform: str) -> bool:
        return self.platform == platform


@dataclass
class ConanRecipes:
    platform: Optional[str]
    queries: dict[str, str] = field(default_factory=dict)
    # Triples: package_name:version:query_name
    packages: list[str] = field(default_factory=list)

    @staticmethod
    def parse(json: dict) -> "ConanRecipes":
        if not isinstance(json, dict):
            raise TypeError("conan-recipe must be a dictionary")
        platform = json.get("platform")
        queries = json.get("queries", {})
        packages = json.get("packages", [])
        _validate_dict_type("conan-recipes.queries", queries, str, str)
        _validate_list_type("conan-recipes.packages", packages, str)
        _validate_pattern("query_name", "conan-recipes.queries keys", list(queries.keys()))
        _validate_pattern("package_name_version_query", "conan-recipes.packages", packages)
        if platform:
            _validate_pattern("platform_name", "conan-recipes.platform", [platform])
        return ConanRecipes(platform, queries, packages)

    def matches_platform(self, platform: str) -> bool:
        return not self.platform or platform == self.platform


@dataclass
class CMakeBuild:
    full_name: str = ""
    platform: Optional[str] = None
    build_type: str = ""
    build_suffix: Optional[str] = None
    cmake_options: dict[str, str] = field(default_factory=dict)
    install_files: list[str] = field(default_factory=list)

    @staticmethod
    def parse(json: dict, full_name: str) -> "CMakeBuild":
        full_name = full_name or ":release:"
        _validate_pattern("build_name", "cmake-build name", [full_name])
        platform, build_type, build_suffix = full_name.split(":")
        platform = platform or None
        build_suffix = build_suffix or None

        cmake_options = json.get("cmake-options", {})
        _validate_dict_type("cmake-build.cmake-options", cmake_options, str, str)
        install_files = json.get("install-files", [])
        _validate_list_type("cmake-build.install-files", install_files, str)
        return CMakeBuild(
            full_name, platform, build_type, build_suffix, cmake_options, install_files
        )

    def inherit_cmake_options(self, parent: "CMakeBuild"):
        for key, value in parent.cmake_options.items():
            if key not in self.cmake_options:
                self.cmake_options[key] = value


@dataclass
class CMakeRecipe:
    name: str
    version: str
    github_project: str
    source_branch: str
    source_sha1: str
    source_patches: list[str] = field(default_factory=list)
    after_checkout_commands: list[str] = field(default_factory=list)
    default_build: CMakeBuild = field(default_factory=CMakeBuild)
    builds: list[CMakeBuild] = field(default_factory=list)

    @staticmethod
    def parse(json: dict) -> "CMakeRecipe":
        if not isinstance(json, dict):
            raise TypeError("cmake-recipe must be a dictionary")
        name_version = json.get("package")
        _validate_pattern("package_name_version", "cmake-recipe.package", [name_version])
        name, version = name_version.split(":")
        github_project = json.get("github-project")
        _validate_pattern("github_project", "cmake-recipe.github-project", [github_project])
        source_branch = json.get("source-branch")
        _validate_pattern("branch_name", "cmake-recipe.source-branch", [source_branch])
        source_sha1 = json.get("source-sha1")
        _validate_pattern("sha1", "cmake-recipe.source-sha1", [source_sha1])
        source_patches = json.get("source-patches", [])
        _validate_list_type("cmake-recipe.source-patches", source_patches, str)
        after_checkout_commands = json.get("after-checkout-commands", [])
        _validate_list_type("cmake-recipe.after-checkout-commands", after_checkout_commands, str)

        default_build = CMakeBuild.parse(json, "")
        json_builds = json.get("builds", {})
        _validate_dict_type("cmake-recipe.builds", json_builds, str, object)
        builds = []
        for build_name, json_build in json_builds.items():
            build = CMakeBuild.parse(json_build, build_name)
            if not build.install_files:
                raise ValueError(f"Build '{build_name}' has no install files")
            build.inherit_cmake_options(default_build)
            builds.append(build)
        if builds and default_build.install_files:
            raise ValueError(
                "If there are multiple builds then it makes no sense to specify "
                "install-files for the default-build"
            )

        return CMakeRecipe(
            name=name,
            version=version,
            github_project=github_project,
            source_branch=source_branch,
            source_sha1=source_sha1,
            source_patches=source_patches,
            after_checkout_commands=after_checkout_commands,
            default_build=default_build,
            builds=builds,
        )

    def get_builds(self, platform: str) -> list[CMakeBuild]:
        if not self.builds:
            return [self.default_build]
        builds = []
        for build in self.builds:
            if not build.platform or build.platform == platform:
                builds.append(build)
        return builds

    def matches_platform(self, platform: str) -> bool:
        if not self.builds:
            return True
        for build in self.builds:
            if not build.platform or build.platform == platform:
                return True
        return False


@dataclass
class DependenciesJson:
    dependencies: list[str] = field(default_factory=list)
    package_caches: list[PackageCache] = field(default_factory=list)
    conan_recipes: list[ConanRecipes] = field(default_factory=list)
    cmake_recipes: list[CMakeRecipe] = field(default_factory=list)

    @staticmethod
    def parse(json: dict) -> "DependenciesJson":
        dependencies = json.get("dependencies", [])
        _validate_list_type("dependencies", dependencies, str)
        _validate_pattern("package_name", "dependencies", dependencies)

        package_caches = [
            PackageCache.parse(json_cache) for json_cache in json.get("package-caches", [])
        ]

        conan_recipes = [
            ConanRecipes.parse(json_recipes) for json_recipes in json.get("conan-recipes", {})
        ]

        cmake_recipes = [
            CMakeRecipe.parse(json_recipe) for json_recipe in json.get("cmake-recipes", [])
        ]

        return DependenciesJson(dependencies, package_caches, conan_recipes, cmake_recipes)


# =================================================================================================
# region                                     Package building
# =================================================================================================


@dataclass
class BuildOptions:
    platform: str = current_platform()
    clean_build: bool = False
    skip_download_source: bool = False
    deps_json_dir: Optional[str] = None
    generator: Generator = Generator.DEFAULT
    build_platform_info: Optional[BuildPlatformInfo] = None

    @staticmethod
    def initialize(args: argparse.Namespace, deps_json_dir: str) -> "BuildOptions":
        self = BuildOptions()
        self.clean_build = args.clean_build
        self.skip_download_source = args.skip_download_source
        self.deps_json_dir = deps_json_dir
        self.generator = Generator(args.generator) if args.generator else Generator.DEFAULT
        build_platform_info_required = not (os.name == "nt" and self.generator == Generator.DEFAULT)
        if build_platform_info_required:
            self.build_platform_info = build_platform_info(args)
        return self


def get_buildable_package_names(deps: DependenciesJson, platform: str) -> list[str]:
    names = []
    for conan_recipes in deps.conan_recipes:
        if conan_recipes.matches_platform(platform):
            for pkg in conan_recipes.packages:
                names.append(pkg.split(":")[0])

    for recipe in deps.cmake_recipes:
        if recipe.matches_platform(platform):
            names.append(recipe.name)
    return list(set(names))


def build_package(
    deps: DependenciesJson,
    package_name: str,
    target_dir: str,
    build_dir: str,
    options: BuildOptions,
) -> str:
    conan_recipe = find_conan_recipe(deps, package_name, options.platform)
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
        copy_subdirs(target_dir, package_path, ["include", "lib", "bin"])
        return package.version

    cmake_recipe = find_cmake_recipe(deps, package_name, options.platform)
    if cmake_recipe:
        build_cmake_package(cmake_recipe, target_dir, build_dir, options)
        return cmake_recipe.version

    assert False, f"Don't know how to build package: '{package_name}'"


# =================================================================================================
# region                                    Building Conan pkgs
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


def find_conan_recipe(
    deps_json: DependenciesJson, package_name: str, platform: str
) -> Optional[ConanPackageQuery]:
    for conan_recipes in deps_json.conan_recipes:
        if not conan_recipes.matches_platform(platform):
            continue
        for pkg in conan_recipes.packages:
            name, version, query_name = pkg.split(":")
            if name == package_name:
                query = conan_recipes.queries.get(query_name)
                assert query is not None
                return ConanPackageQuery(package_name, version, query)
    return None


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


# =================================================================================================
# region                                  Building CMake pkgs
# =================================================================================================


def find_cmake_recipe(
    deps_json: DependenciesJson, package_name: str, platform: str
) -> Optional[CMakeRecipe]:
    for recipe in deps_json.cmake_recipes:
        if recipe.name == package_name and recipe.matches_platform(platform):
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
            print(f"Updating existing repository: {github_project} to branch/tag: {branch}")
            subprocess.run(["git", "fetch"], cwd=dest_dir, check=True)
            subprocess.run(["git", "checkout", "-f", branch], cwd=dest_dir, check=True)
            subprocess.run(["git", "pull", "origin", branch], cwd=dest_dir, check=True)
        else:
            print(f"Cloning repository: {github_project} branch/tag: {branch}")
            git_clone_cmd = ["git", "clone", "--branch", branch, repo_url, dest_dir]
            subprocess.run(git_clone_cmd, check=True)

    rev_parse_result = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=dest_dir, check=True, stdout=subprocess.PIPE
    )
    current_sha1 = rev_parse_result.stdout.decode("utf-8").strip()

    if current_sha1 != sha1:
        print(f"Checking out specific commit: {sha1}")
        subprocess.run(["git", "checkout", sha1], cwd=dest_dir, check=True)


def configure_cmake_package(
    cmake_build: CMakeBuild,
    options: BuildOptions,
    source_dir: str,
    build_dir: str,
    install_dir: str,
):
    os.makedirs(build_dir, exist_ok=True)
    cmake_cmd = ["cmake", "-S", source_dir, "-DCMAKE_INSTALL_PREFIX=" + install_dir]
    cmake_cmd += [f"-D{key}={value}" for key, value in cmake_build.cmake_options.items()]
    cmake_cmd += [f"-DCMAKE_BUILD_TYPE={cmake_build.build_type}"]
    cmake_cmd += generator_options(options.generator, options.build_platform_info)

    print("CMake command: " + str(cmake_cmd))
    env = (
        options.build_platform_info.vs_env
        if os.name == "nt" and options.build_platform_info
        else None
    )
    subprocess.run(cmake_cmd, cwd=build_dir, env=env, check=True)


def build_cmake_package(
    recipe: CMakeRecipe, target_dir: str, build_dir: str, options: BuildOptions
):
    print_title(f"Building CMake package: {recipe.name}", Color.DEFAULT, Style.BOLD)

    src_sub_dir = os.path.join(build_dir, "src")
    check_commands_available(["git", "cmake"])

    print_step("Updating source code")
    github_update_source(
        recipe.github_project,
        recipe.source_branch,
        recipe.source_sha1,
        src_sub_dir,
        options.skip_download_source,
    )

    if recipe.source_patches:
        print_step("Applying patches")
    for patch_file in recipe.source_patches:
        patch_file_path = os.path.join(options.deps_json_dir, patch_file)
        print(f"Applying patch: {patch_file_path}")
        subprocess.run(["git", "apply", patch_file_path], cwd=src_sub_dir, check=True)

    if recipe.after_checkout_commands:
        print_step("Running after checkout commands")
    for cmd in recipe.after_checkout_commands:
        print(f"Running command: {cmd}")
        subprocess.run(cmd, cwd=src_sub_dir, check=True, shell=True)

    num_threads = os.cpu_count()
    jobs_argument = [f"-j{num_threads}"] if num_threads > 1 else []

    for build in recipe.get_builds(current_platform()):
        print_step(f"Configuring build: {build.full_name}")
        build_suffix = build.build_type + (f"_{build.build_suffix}" if build.build_suffix else "")
        build_sub_dir = os.path.join(build_dir, f"build_{build_suffix}")
        install_sub_dir = os.path.join(build_dir, f"install_{build_suffix}")

        prepare_dir(build_sub_dir, options.clean_build, False)
        prepare_dir(install_sub_dir, True, False)

        configure_cmake_package(build, options, src_sub_dir, build_sub_dir, install_sub_dir)

        print_step(f"Running build: {build.full_name}")
        build_cmd = ["cmake", "--build", ".", "--config", build.build_type, "--target", "install"]
        build_cmd += jobs_argument
        print("Build command: " + str(build_cmd))
        subprocess.run(build_cmd, cwd=build_sub_dir, check=True)

        print_step(f"Installing files for build: {build.full_name}")
        install_files(target_dir, install_sub_dir, build.install_files or [".*"])


# =================================================================================================
# region                                   Package caches
# =================================================================================================


def get_cached_package_names(deps: DependenciesJson, platform: str) -> list[str]:
    names = []
    for cache in deps.package_caches:
        if cache.matches_platform(platform):
            for pkg in cache.packages:
                names.append(pkg.split(":")[0])
    return list(set(names))


# Returns a tuple (version, full_url, hash)
def find_package_in_caches(deps: DependenciesJson, package_name: str) -> Optional[tuple[str, str]]:
    for cache in deps.package_caches:
        for pkg in cache.packages:
            name, version, hash = pkg.split(":")
            if name == package_name:
                additional_slash = "" if cache.url.endswith("/") else "/"
                return (version, f"{cache.url}{additional_slash}{name}_{version}.zip", hash)
    return None


def download_package(deps: DependenciesJson, package_name: str, target_dir: str, package_dir: str):
    cached_package = find_package_in_caches(deps, package_name)
    if not cached_package:
        print(f"Don't know how to download package: {package_name}")
        exit(1)

    (version, url, hash) = cached_package
    print(f"Downloading package: {stylize_text(package_name, Color.DEFAULT, Style.BOLD)}")
    print(f"  Version: {version}")

    package_file_name = f"{package_name}_{version}.zip"
    package_file_path = os.path.join(package_dir, package_file_name)
    if os.path.isfile(package_file_path) and compute_sha256_64(package_file_path) == hash:
        print(f"  Package already downloaded: {package_file_path}")
    else:
        print(f"  Downloading from: {url}")
        download_file_unsafe(url, package_file_path)
        downloaded_hash = compute_sha256_64(package_file_path)
        if downloaded_hash != hash:
            print_error(f"Error: downloaded file has invalid hash: {package_file_path}")
            print_error(f"File hash: {downloaded_hash} expected: {hash}")
            exit(1)

    print(f"  Unpacking to: {target_dir}")
    shutil.unpack_archive(package_file_path, target_dir)
    print("")


# =================================================================================================
# region                                     Main script
# =================================================================================================


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
        "download", help="Download prebuilt packages from caches. [DEFAULT COMMAND]"
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
        sub_parser.add_argument(
            "--skip-download-source",
            action="store_true",
            help="Skip downloading sources if they are already present in the source directory.",
        )
        generators = valid_generators()
        sub_parser.add_argument(
            "-G",
            "--generator",
            type=str,
            choices=generators,
            default=generators[0],
            help="CMake generator and compiler set to use.",
        )
        if os.name == "nt":
            sub_parser.add_argument(
                "--vs-path",
                type=str,
                default=None,
                help="Path to Visual Studio installation "
                "(optional, can be used when installed at a non-standard path).",
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

    args = parser.parse_args()
    if not args.command:
        args.command = "download"
        args.packages = []
    return args


def main():
    args = parse_arguments()

    deps_json_path = args.deps_file or os.path.join(os.getcwd(), "dependencies.json")
    deps_json_dir = os.path.dirname(os.path.abspath(deps_json_path))
    with open(deps_json_path, "r") as file:
        deps_json = DependenciesJson.parse(json.load(file))
    platform = current_platform()

    downloadable_packages = get_cached_package_names(deps_json, platform)
    buildable_packages = get_buildable_package_names(deps_json, platform)

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
        print_error(f"The following requested packages are not available: {unavailable_packages}")
        exit(1)

    default_target_dir = os.path.join(libfwk_path(), "dependencies")
    target_dir = os.path.abspath(args.target_dir or default_target_dir)

    # TODO: multithreading for downloading & unpacking?
    if args.command == "download":
        print_title("Downloading pre-built packages", Color.DEFAULT, Style.BOLD)
        if args.packages is None and args.clean is None:
            args.clean = True

        prepare_target_dir(target_dir, args.clean)
        for dep in selected_deps:
            download_package(deps_json, dep, target_dir, target_dir)

    elif args.command == "build":
        build_dir = os.path.join(libfwk_path(), "build", "dependencies")
        build_options = BuildOptions.initialize(args, deps_json_dir)

        prepare_target_dir(target_dir, args.clean)
        for dep in selected_deps:
            package_build_dir = os.path.join(build_dir, dep)
            build_package(deps_json, dep, target_dir, package_build_dir, build_options)

    elif args.command == "package":
        build_dir = os.path.join(libfwk_path(), "build", "dependencies")
        build_options = BuildOptions.initialize(args, deps_json_dir)

        prepare_dir(target_dir, args.clean)
        download_strings = []
        for dep in selected_deps:
            package_build_dir = os.path.join(build_dir, dep)
            package_target_dir = os.path.join(package_build_dir, "package")
            prepare_dir(package_target_dir, True, False)
            version = build_package(
                deps_json, dep, package_target_dir, package_build_dir, build_options
            )
            zip_file_base = f"{dep}_{version}"
            zip_path = zip_directory(package_target_dir, os.path.join(target_dir, zip_file_base))
            hash = compute_sha256_64(zip_path)
            download_strings.append(f"{dep}:{version}:{hash}")

        print_color("\nBuilt packages (name:version:hash):", Color.DEFAULT, Style.BOLD)
        for download_string in download_strings:
            print(f"  {download_string}")


if __name__ == "__main__":
    main()

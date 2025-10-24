#!/usr/bin/env python

import enum
import argparse, os
import shutil
import subprocess
from dataclasses import dataclass, field
from typing import Optional
import tempfile


def find_visual_studio_installation() -> str:
    """Find the latest installed Visual Studio."""
    base_path = "C:/Program Files/Microsoft Visual Studio"
    if not os.path.exists(base_path):
        return None
    versions = sorted(os.listdir(base_path), reverse=True)
    for version in versions:
        version_path = os.path.join(base_path, version)
        for edition in ["Enterprise", "Professional", "Community"]:
            edition_path = os.path.join(version_path, edition)
            if os.path.exists(edition_path):
                return edition_path
    return None


def find_vcvars64_bat(visual_studio_path: str = None) -> Optional[str]:
    path = os.path.join(visual_studio_path, "VC", "Auxiliary", "Build", "vcvars64.bat")
    if os.path.exists(path):
        return path
    return None


def get_vcvars_environment(vcvars_path: str) -> dict:
    temp_dir = tempfile.gettempdir()
    temp_batch_file = os.path.join(temp_dir, "temp_vcvars.bat")
    with open(temp_batch_file, "w") as f:
        f.write(f'@call "{vcvars_path}"\n')
        f.write(f"@set\n")
    cmd = ["cmd", "/q", "/c", temp_batch_file]

    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error running vcvars64.bat: {e}\nStdout: {e.stdout}\nStderr: {e.stderr}")
        exit(1)
    finally:
        os.remove(temp_batch_file)

    env = {}
    env.update(os.environ)
    for line in result.stdout.strip().split("\n"):
        if "=" in line:
            name, value = line.split("=", 1)
            env[name.strip()] = value.strip().replace("\r", "")
    return env


def visual_studio_year_version(path: str, env: dict) -> Optional[tuple[int, int]]:
    year = None
    for part in reversed(path.split(os.sep)):
        if part.startswith("20") and len(part) == 4 and part.isdigit():
            year = int(part)
            break
    major = env["VisualStudioVersion"].split(".")[0] if "VisualStudioVersion" in env else None
    if year is not None and major is not None:
        return (year, int(major))
    return None


class Generator(enum.Enum):
    NINJA_CLANG_CL = "ninja-clang-cl"
    VS_CLANG_CL = "vs-clang-cl"


def valid_generators() -> list[str]:
    if os.name == "nt":
        return [Generator.NINJA_CLANG_CL.value, Generator.VS_CLANG_CL.value]
    return []


class BuildType(enum.Enum):
    DEBUG = "Debug"
    DEVELOP = "Develop"
    RELEASE = "Release"


@dataclass
class BuildConfig:
    source_dir: str
    build_dir: str
    build_type: Optional[BuildType] = BuildType.DEBUG
    generator: Optional[Generator] = Generator.NINJA_CLANG_CL
    vs_path: Optional[str] = None
    cmake_defines: dict = field(default_factory=dict)


def configure_windows(build_config: BuildConfig):
    vs_path = build_config.vs_path or find_visual_studio_installation()
    vcvars64_path = find_vcvars64_bat(vs_path) if vs_path else None
    if not vs_path or not vcvars64_path:
        print("Visual Studio installation not found or vcvars64.bat is missing.")
        exit(1)
    vs_env = get_vcvars_environment(vcvars64_path)
    vs_year_version = visual_studio_year_version(vs_path, vs_env)
    if vs_year_version is None or vs_year_version[0] < 2022:
        print("Visual Studio 2022 or later is required.")
        exit(1)

    if build_config.generator == Generator.NINJA_CLANG_CL:
        generator = "Ninja"
    else:
        generator = f"Visual Studio {vs_year_version.major} {vs_year_version.year}"

    cmd = ["cmake", "-G", generator, "--fresh"]
    cmd += ["-S", build_config.source_dir, "-B", build_config.build_dir]
    cmd += ["-DCMAKE_C_COMPILER=clang-cl", "-DCMAKE_CXX_COMPILER=clang-cl"]
    cmd += [f"-DCMAKE_BUILD_TYPE={build_config.build_type.value}"]
    for key, value in build_config.cmake_defines.items():
        cmd.append(f"-D{key}={value}")
    print("Running CMake with command:", " ".join(cmd))
    subprocess.run(cmd, env=vs_env, check=True)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="configure",
        description="Helper tool for easier environment setup for libfwk-based projects."
        "Currently it's mainly useful on Windows to properly setup ninja or VS-based builds.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument("--build-dir", type=str, default="build")
    parser.add_argument(
        "--build-type",
        type=str,
        choices=[bt.value for bt in BuildType],
        default=BuildType.DEBUG.value,
        help="Build type.",
    )

    generator_choices = valid_generators()
    parser.add_argument(
        "--generator",
        type=str,
        choices=generator_choices,
        default=generator_choices[0] if generator_choices else None,
        help="CMake generator+compiler set to use.",
    )
    if os.name == "nt":
        parser.add_argument(
            "--vs-path",
            type=str,
            default=None,
            help="Path to Visual Studio installation "
            "(optional, can be used when installed at a non-standard path).",
        )

    parser.add_argument(
        "-D",
        action="append",
        dest="cmake_defines",
        default=[],
        help="CMake definitions (can be specified multiple times).",
    )

    return parser.parse_args()


def main():
    args = parse_arguments()
    source_dir = os.path.abspath(os.getcwd())
    build_dir = os.path.abspath(args.build_dir) or os.path.join(source_dir, "build")
    build_config = BuildConfig(
        source_dir=source_dir,
        build_dir=build_dir,
        build_type=BuildType(args.build_type),
        generator=Generator(args.generator),
        vs_path=args.vs_path,
        cmake_defines={k: v for k, v in (define.split("=", 1) for define in args.cmake_defines)},
    )

    if os.name == "nt":
        configure_windows(build_config)


if __name__ == "__main__":
    main()

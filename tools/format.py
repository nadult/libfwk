#!/usr/bin/env python

import enum
import argparse, os, re, subprocess, shutil
import sys


class CodeFormatter:
    def __init__(self, expected_version=20):
        self.expected_version = expected_version
        self.clang_format_cmd = self._find_clang_format_cmd()
        self._verify_clang_format()

    def _find_clang_format_cmd(self):
        names = [f"clang-format-{self.expected_version}", "clang-format"]
        for name in names:
            if shutil.which(name) is not None:
                return name
        raise Exception(f"{names[0]} is missing")

    def _verify_clang_format(self):
        result = subprocess.run([self.clang_format_cmd, "--version"], stdout=subprocess.PIPE)
        tokens = result.stdout.decode("utf-8").split()
        while len(tokens) > 0 and tokens[0] != "clang-format":
            tokens.pop(0)
        if (
            result.returncode != 0
            or len(tokens) < 3
            or tokens[0] != "clang-format"
            or tokens[1] != "version"
        ):
            raise Exception(f"error while checking clang-format version (version string: {tokens})")
        version = tokens[2].split(".", 2)
        print(f"clang-format version: {version[0]}.{version[1]}.{version[2]}")

        if int(version[0]) < self.expected_version:
            raise Exception(
                f"clang-format is too old; At least version {self.expected_version} is required"
            )

    def format_cpp(self, files, check: bool):
        print("Checking code formatting..." if check else "Formatting code...")
        full_command = [self.clang_format_cmd, "-i"]
        if check:
            full_command += ["--dry-run", "-Werror"]
        full_command += files
        result = subprocess.run(full_command)

        if check:
            if result.returncode != 0:
                exit(1)
            print("All OK")


def find_files(root_dirs, regex):
    out = []
    for root_dir in root_dirs:
        for root, dirs, files in os.walk(root_dir):
            for file in files:
                if regex.match(file):
                    out.append(os.path.join(root, file))
    return out


def check_black_installed():
    cmd = [sys.executable, "-m", "black", "--version"]
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        raise Exception("Black is not installed")


def format_python(files: list[str], check: bool):
    check_black_installed()
    cmd = [sys.executable, "-m", "black", "-l", "100"]
    if check:
        cmd += ["--check", "--color", "--diff"]
    cmd += files
    result = subprocess.run(cmd)
    if check:
        if result.returncode != 0:
            exit(1)
        print("All OK")


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
    print_color(f"\n(*) {text}", Color.HI_WHITE, Style.BOLD)


def print_error(text: str):
    print_color(text, Color.RED, Style.BOLD)
    
def main():
    print("\033[91mThis is BRIGHT RED text.\033[0m")
    print("abbababab \033[1;91mThis is bold BRIGHT RED text.\033[0m")
    print_error("This is an error message.")
    print_step("This is a step message.")
    print_title("This is a title", Color.HI_CYAN, Style.BOLD)
    parser = argparse.ArgumentParser(
        prog=__file__,
        description="Tool for code formatting and format verification",
    )
    parser.add_argument("-c", "--check", action="store_true")
    parser.add_argument("-p", "--python", action="store_true")
    args = parser.parse_args()

    if args.python:
        py_files = find_files(["tools"], re.compile(".*[.]py$"))
        format_python(py_files, args.check)
        return

    formatter = CodeFormatter()
    libfwk_path = os.path.abspath(os.path.join(__file__, "../.."))
    os.chdir(libfwk_path)
    files = find_files(["include", "src"], re.compile(".*[.](h|cpp)$"))
    formatter.format_cpp(files, args.check)


if __name__ == "__main__":
    main()

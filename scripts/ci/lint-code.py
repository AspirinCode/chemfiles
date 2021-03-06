#!/usr/bin/env python
# -*- coding: utf8 -*-
"""
This script check for some functions we don't want to use because they are
better wrappers in chemfiles
"""
import os
import sys
import re
from glob import glob

ROOT = os.path.join(os.path.dirname(__file__), "..", "..")
ERRORS = 0

PARSING_FUNCTIONS = {
    # Match 'std::<...>',  '::<...>' and '<...>'
    r"(?P<name>\b(std)?(::)?stof\b)": "parse<double>",
    r"(?P<name>\b(std)?(::)?stod\b)": "parse<double>",
    r"(?P<name>\b(std)?(::)?stold\b)": "parse<double>",
    r"(?P<name>\b(std)?(::)?stoi\b)": "parse<long long>",
    r"(?P<name>\b(std)?(::)?stol\b)": "parse<long long>",
    r"(?P<name>\b(std)?(::)?stoll\b)": "parse<long long>",
    r"(?P<name>\b(std)?(::)?stoul\b)": "parse<size_t>",
    r"(?P<name>\b(std)?(::)?stoull\b)": "parse<size_t>",
}

CAPI_FUNCTIONS = {
    r"(?P<name>\bstatic_cast<size_t>\b)": "checked_cast",
    r"(?P<name>\bnew\b)": "shared_allocator::make_shared",
    r"(?P<name>\bdelete\b)": "shared_allocator::free",
    r"(?P<name>\bmalloc\b)": "shared_allocator::make_shared",
    r"(?P<name>\bfree\b)": "shared_allocator::free",
}


def error(message):
    global ERRORS
    ERRORS += 1
    print(message)


def check_code(path, replacements):
    with open(path) as fd:
        for i, line in enumerate(fd):
            for bad, replacement in replacements.items():
                match = re.search(bad, line)
                if match and "NOLINT" not in line:
                    error(
                        "Please replace '{}' by '{}' at {}:{}".format(
                            match.group("name"),
                            replacement,
                            os.path.relpath(path),
                            i + 1,
                        )
                    )


if __name__ == "__main__":
    for file in glob(os.path.join(ROOT, "include", "chemfiles", "*.hpp")):
        if file.endswith("utils.hpp"):
            continue
        check_code(file, PARSING_FUNCTIONS)

    for file in glob(os.path.join(ROOT, "include", "chemfiles", "*", "*.hpp")):
        check_code(file, PARSING_FUNCTIONS)

    for file in glob(os.path.join(ROOT, "src", "*.cpp")):
        check_code(file, PARSING_FUNCTIONS)

    for file in glob(os.path.join(ROOT, "src", "*", "*.cpp")):
        check_code(file, PARSING_FUNCTIONS)

    for file in glob(os.path.join(ROOT, "src", "capi", "*.cpp")):
        check_code(file, CAPI_FUNCTIONS)

    if ERRORS != 0:
        sys.exit(1)

#!/bin/bash
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
grep --color --include "*.h" --include "*.cpp" --exclude-dir build $1 $2 $3 $4 $5 $6 $7 $8 -R ./

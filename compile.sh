#!/usr/bin/env bash
set -euox pipefail

mkdir -p out
zig cc -std=c11 -Wall -Wextra -Wpedantic -Werror -o out/classy src/main.c $@

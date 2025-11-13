#!/usr/bin/env bash
set -euox pipefail

mkdir -p out
zig cc -std=gnu11 -Wall -Wextra -Wpedantic -Werror -o out/classy src/main.c $@

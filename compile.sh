#!/usr/bin/env bash
set -euox pipefail

mkdir -p out
zig cc -std=c17 -Wall -Wextra -Wpedantic -o out/classy src/main.c $@

#!/usr/bin/env bash
set -euox pipefail

mkdir -p out
zig cc -std=c17 -Wall -Wextra -Wpedantic -Werror -o out/run-tests tests/main.c
./out/run-tests

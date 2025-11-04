#!/usr/bin/env bash
set -euox pipefail

zig cc -std=c17 -Wall -Wextra -Wpedantic -o classy.o src/main.c $@

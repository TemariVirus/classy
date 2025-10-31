#!/usr/bin/env bash
set -euox pipefail

cc -std=c17 -Wall -Wextra -Wpedantic -o classy.o src/main.c $@

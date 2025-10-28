#!/usr/bin/env bash
set -euox pipefail

cc -std=c17 -Wall -Wextra -o classy.o src/main.c $@

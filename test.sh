#!/usr/bin/env bash
set -euox pipefail

cc -std=c17 -Wall -Wextra -Wpedantic -o run-tests.o tests/main.c
./run-tests.o

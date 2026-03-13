# classy

A classy class management system.

## Building

This section assumes a UNIX environment.

1. Install [Zig](https://ziglang.org/learn/getting-started/). This project was originally compiled with Zig 0.15.2, but any version of Zig that supports `zig cc` should work.
2. Run `./comile.sh`. The compiled binary will be at `./out/classy`.

### Building without Zig

Building without Zig should be as simple as translating `./compile.sh` to use your C prefered compiler. Classy uses a single translation unit build (aka jumbo/unity build), with `src/main.c` as the entrypoint. The code has been tested to compile on MSVC as well (the GNU extensions used are replaced with MSVC equivalents).

## Running tests

This section assumes a UNIX environment.

1. Install Zig.
2. Run `./test.sh`.

### Run tests without Zig

See [Building without Zig](#building-without-zig). The only difference is that the entrypoint is now `tests/main.c`.tests

## Command Syntax

Commands are single-line and cannot exceed 4094 characters in length.
All commands are case-sensitive.

### HELP

```plain
HELP
```

Displays a list of all available commands and what they do.

### OPEN

```plain
OPEN filename
```

Loads the file as the new active database, discarding the old active database (if any).

`filename` is either an absolute path or a path relative to the current working directory.

### SHOW ALL

```plain
SHOW ALL [SORT BY column [ASC|DESC]]
```

Displays all records in the active database.

If the sort column is not specified, it defaults to `ID`.

If the sort order is not specified, it defaults to `ASC`.

### INSERT

```plain
INSERT ID=id Name="name" Programme="programme" Mark=mark
```

Inserts a new record into the active database.

`ID` is a 32-bit unsigned integer.
`Name` and `Programme` are strings and must be enclosed in double quotes (`"`).
`Mark` is a single-precision floating-point number.

The columns may be in any order, but must appear exactly once.

Record values cannot contain newlines (`\n`).
Double quotes (`"`) and backslashes (`\`) within string values must be escaped by preceding them with a backslash.

### QUERY

```plain
QUERY condition [SORT BY column [ASC|DESC]]
```

Displays all records in the active database that satisfy `condition`.

If the sort column is not specified, it defaults to `ID`.

If the sort order is not specified, it defaults to `ASC`.

`condition` may be composed of the following operations (ordered by highest to lowest precedence):

- integer, float, and string equality (`column` = `val`)
- integer and float greater than (`column` > `val`)
- integer and float less than (`column` < `val`)
- string contains (`val` IN `column`)
- boolean not (NOT `expr`)
- boolean and (`expr` AND `expr`)
- boolean or (`expr` OR `expr`)

Parentheses may be used to group sub-conditions.
If necessary, integers will be automatically converted to floats.
Whitespace around the =, >, and < operators is ignored.

Examples:

```plain
QUERY ID=12345 SORT BY Mark DESC
QUERY (Mark > 50.6 OR NOT Programme = "Computer Science") AND "Alice \"in\" Wonderland" IN Name
```

### SHOW SUMMARY

```plain
SHOW SUMMARY [condition]
```

Displays a summary of all records in the active database that satisfy `condition`.

`condition` is as defined in `QUERY`.

If condition is not given, all records in the active database are used.

### UPDATE

```plain
UPDATE ID=id column=value...
```

Updates the values of the other columns in the record with the specified `ID`, if it exists.

The columns may be in any order, but cannot appear more than once.
The `ID` column is required. The other columns are optional, but at least one must be specified.

`value` follows the same constraints as in `INSERT`.

### DELETE

```plain
DELETE ID=id
```

Deletes the record with the specified `ID` from the active database.

### SAVE

```plain
SAVE [filename]
```

Saves the active database to a file.

`filename` is either an absolute path or a path relative to the current working directory.

If `filename` already exists, it will be overwritten.

If `filename` is not specified, it defaults to the filename used in the last successful `OPEN` or `SAVE` command.

## Benchmark

Insert 1 million records with sequential IDs into empty DB; single-threaded.

```plain
> poop ./out/classy -d 2000
Benchmark 1 (26 runs): ./out/classy
  measurement          mean ± σ            min … max           outliers
  wall_time          77.6ms ±  485us    76.8ms … 78.7ms          0 ( 0%)
  peak_rss           93.8MB ± 4.48KB    93.8MB … 93.8MB          3 (12%)
  cpu_cycles          201M  ±  449K      200M  …  202M           2 ( 8%)
  instructions        588M  ± 60.7K      588M  …  588M           0 ( 0%)
  cache_references   1.27M  ± 27.0K     1.22M  … 1.32M           0 ( 0%)
  cache_misses       9.78K  ±  467      9.01K  … 10.5K           0 ( 0%)
  branch_misses      77.3K  ± 7.41K     72.1K  …  106K           3 (12%)
```

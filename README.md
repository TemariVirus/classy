# classy

A classy class management system

## Command Syntax

All commands are case-sensitive. All commands are terminated by a newline character. Including the newline character, commands cannot exceed 4095 characters in length.

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

Displays all rows in the active database.

If the sort column is not specified, it defaults to `ID`.

If the sort order is not specified, it defaults to `ASC`.

### INSERT

```plain
INSERT ID=id Name="name" Programme="programme" Mark=mark
```

Inserts a new row into the active database.

`ID` is a 32-bit unsigned integer.
`Name` and `Programme` are strings and must be enclosed in double quotes (`"`).
`Mark` is a single-precision floating-point number.

The columns may be in any order, but must appear exactly once.

Row values cannot contain newlines (`\n`).
Double quotes (`"`) and backslashes (`\`) within string values must be escaped by preceding them with a backslash.

### QUERY

```plain
QUERY condition
```

Displays all rows in the active database that satisfy `condition`.

`condition` may be composed of the following operations (ordered by highest to lowest precedence):

- integer, float, and string equality (column = val)
- integer and float greater than (column > val)
- integer and float less than (column < val)
- string contains (val IN column)
- boolean not (NOT expr)
- boolean and (expr AND expr)
- boolean or (expr OR expr)

Parentheses may be used to group sub-conditions. If necessary, integers will be automatically converted to floats. Whitespace around the =, >, and < operators is ignored.

Examples:

```plain
QUERY ID=12345
QUERY (Mark > 50.6 OR NOT Programme = "Computer Science") AND "Alice \"in\" Wonderland" IN Name
```

### SHOW SUMMARY

```plain
SHOW SUMMARY [condition]
```

Displays a summary of all rows in the active database that satisfy `condition`.

`condition` is as defined in `QUERY`.

If condition is not given, all rows in the active database are used.

### UPDATE

```plain
UPDATE ID=id[column=value]*
```

Updates the values of the other columns in the row with the specified `ID`, if it exists.

The columns may be in any order, but cannot appear more than once.
The `ID` column is required. The other columns are optional.

`value` follows the same constraints as in `INSERT`.

### DELETE

```plain
DELETE ID=id
```

Deletes the row with the specified `ID` from the active database.

### SAVE

```plain
SAVE [filename]
```

Saves the active database to a file.

`filename` is either an absolute path or a path relative to the current working directory.

If `filename` already exists, it will be overwritten.

If `filename` is not specified, it defaults to the filename used in the last `OPEN` or `SAVE` command.

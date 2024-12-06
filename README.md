# replace

Replace strings in text files or from stdin to stdout.

This program accepts a list of from-string/to-string pairs and replaces
each occurrence of a from-string with the corresponding to-string.

Usage:

```
replace [-s] [-v] from to [from to ...] [--] [files...]

Options:
-s    Silent mode. Suppress non-error messages.
-v    Verbose mode. Output information about processing.
-?    Display help information.
-V    Display version information.
```

Examples:

```
replace foo bar -- file.txt
cat file.txt | replace foo bar
replace foo bar some other -- file.txt
```

Author: Danila Vershinin + ChatGPT. Adapted from Monty's original MySQL implementation.
License: GNU General Public License v2
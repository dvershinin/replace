/*
   Replace strings in text files or from stdin to stdout.

   This program accepts a list of from-string/to-string pairs and replaces
   each occurrence of a from-string with the corresponding to-string.

   Usage:
     replace [-s] [-v] from to [from to ...] [--] [files...]

   Options:
     -s    Silent mode. Suppress non-error messages.
     -v    Verbose mode. Output information about processing.
     -?    Display help information.
     -V    Display version information.

   Author: Danila Vershinin + ChatGPT. Adapted from Monty's original MySQL implementation.
   License: GNU General Public License v2
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>

/* Structure to hold a single replace pair */
typedef struct {
    char *from;
    char *to;
} ReplacePair;

/* Structure to hold all replace pairs */
typedef struct {
    ReplacePair *pairs;
    size_t count;
    size_t capacity;
} ReplaceList;

/* Structure to hold program options */
typedef struct {
    int silent;
    int verbose;
} ProgramOptions;

/* Function Prototypes */
static void print_help(const char *progname);
static void print_version(const char *progname);
static int parse_options(int argc, char **argv, ProgramOptions *options, int *replace_start);
static int parse_replace_strings(int argc, char **argv, ReplaceList *replace_list);
static void free_replace_list(ReplaceList *replace_list);
static char* replace_in_string(const char *str, ReplaceList *replace_list, int *updated);
static int process_stream(FILE *in, FILE *out, ReplaceList *replace_list, ProgramOptions *options);
static int process_file(const char *filename, ReplaceList *replace_list, ProgramOptions *options);

/* Main Function */
int main(int argc, char *argv[]) {
    ProgramOptions options = {0, 0};
    ReplaceList replace_list = {NULL, 0, 0};
    int error = 0;
    int replace_start = 0;

    /* Parse command-line options */
    if (parse_options(argc, argv, &options, &replace_start)) {
        return 1;
    }

    /* Find '--' in remaining arguments to separate replace pairs from files */
    int delimiter = -1;
    for (int i = replace_start; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            delimiter = i;
            break;
        }
    }

    /* Determine the number of replace pairs */
    int replace_args = (delimiter != -1) ? (delimiter - replace_start) : (argc - replace_start);

    /* Ensure even number of replace_args */
    if (replace_args < 2 || (replace_args % 2) != 0) {
        fprintf(stderr, "Error: Replace strings must be in from/to pairs.\n");
        print_help("replace");
        return 1;
    }

    /* Parse from/to replacement strings */
    if (parse_replace_strings(replace_args, argv + replace_start, &replace_list)) {
        free_replace_list(&replace_list);
        return 1;
    }

    /* Verbose: print replace pairs */
    if (options.verbose) {
        printf("Replacement pairs:\n");
        for (size_t i = 0; i < replace_list.count; i++) {
            printf("  '%s' -> '%s'\n", replace_list.pairs[i].from, replace_list.pairs[i].to);
        }
    }

    /* Determine files after '--' */
    int file_start = (delimiter != -1) ? (delimiter + 1) : (replace_start + replace_args);
    int num_files = argc - file_start;
    char **files = (num_files > 0) ? (argv + file_start) : NULL;

    /* Process input sources */
    if (num_files == 0) {
        /* No files provided; read from stdin and write to stdout */
        error = process_stream(stdin, stdout, &replace_list, &options);
    } else {
        /* Process each file provided */
        for (int i = 0; i < num_files; i++) {
            error |= process_file(files[i], &replace_list, &options);
        }
    }

    /* Cleanup */
    free_replace_list(&replace_list);
    return error ? 2 : 0;
}

/* Function Implementations */

/* Print help information */
static void print_help(const char *progname) {
    printf("%s - Replace strings in files or from stdin to stdout.\n", progname);
    printf("Usage: %s [-s] [-v] from to [from to ...] [--] [files...]\n", progname);
    printf("Options:\n");
    printf("  -s    Silent mode. Suppress non-error messages.\n");
    printf("  -v    Verbose mode. Output information about processing.\n");
    printf("  -?    Display this help information.\n");
    printf("  -V    Display version information.\n");
}

/* Print version information */
static void print_version(const char *progname) {
    printf("%s version 1.0\n", progname);
}

/* Parse command-line options using getopt */
static int parse_options(int argc, char **argv, ProgramOptions *options, int *replace_start) {
    int opt;
    while ((opt = getopt(argc, argv, "sv?V")) != -1) {
        switch (opt) {
            case 's':
                options->silent = 1;
                break;
            case 'v':
                options->verbose = 1;
                break;
            case '?':
                print_help(argv[0]);
                exit(0);
            case 'V':
                print_version(argv[0]);
                exit(0);
            default:
                print_help(argv[0]);
                return 1;
        }
    }
    *replace_start = optind;
    return 0;
}

/* Parse from/to replacement strings */
static int parse_replace_strings(int argc, char **argv, ReplaceList *replace_list) {
    /* Initialize ReplaceList */
    replace_list->capacity = argc / 2;
    replace_list->count = 0;
    replace_list->pairs = malloc(replace_list->capacity * sizeof(ReplacePair));
    if (!replace_list->pairs) {
        fprintf(stderr, "Memory allocation failed for replace pairs.\n");
        return 1;
    }

    /* Parse from/to pairs */
    for (int i = 0; i < argc; i += 2) {
        replace_list->pairs[replace_list->count].from = strdup(argv[i]);
        replace_list->pairs[replace_list->count].to = strdup(argv[i + 1]);
        if (!replace_list->pairs[replace_list->count].from || !replace_list->pairs[replace_list->count].to) {
            fprintf(stderr, "Memory allocation failed for replace strings.\n");
            return 1;
        }
        replace_list->count += 1;
    }

    /* Sort replace pairs by descending length of 'from' strings to prioritize longer matches */
    for (size_t i = 0; i < replace_list->count - 1; i++) {
        for (size_t j = i + 1; j < replace_list->count; j++) {
            size_t len_i = strlen(replace_list->pairs[i].from);
            size_t len_j = strlen(replace_list->pairs[j].from);
            if (len_j > len_i) {
                /* Swap */
                ReplacePair temp = replace_list->pairs[i];
                replace_list->pairs[i] = replace_list->pairs[j];
                replace_list->pairs[j] = temp;
            }
        }
    }

    return 0;
}

/* Free memory allocated for ReplaceList */
static void free_replace_list(ReplaceList *replace_list) {
    if (replace_list->pairs) {
        for (size_t i = 0; i < replace_list->count; i++) {
            free(replace_list->pairs[i].from);
            free(replace_list->pairs[i].to);
        }
        free(replace_list->pairs);
        replace_list->pairs = NULL;
    }
    replace_list->count = 0;
    replace_list->capacity = 0;
}

/* Replace occurrences in a single string based on ReplaceList */
static char* replace_in_string(const char *str, ReplaceList *replace_list, int *updated) {
    *updated = 0;
    size_t len = strlen(str);
    /* Allocate a buffer with initial size */
    size_t buffer_size = len + 1;
    char *result = malloc(buffer_size);
    if (!result) {
        fprintf(stderr, "Memory allocation failed for replacement.\n");
        exit(1);
    }
    char *res_ptr = result;

    const char *current = str;
    while (*current) {
        size_t match_len = 0;
        size_t replace_idx = replace_list->count; /* Initialize to invalid index */
        /* Find the longest matching 'from' string */
        for (size_t i = 0; i < replace_list->count; i++) {
            size_t from_len = strlen(replace_list->pairs[i].from);
            if (from_len == 0) continue; /* Avoid empty from-string */
            if (strncmp(current, replace_list->pairs[i].from, from_len) == 0) {
                if (from_len > match_len) { /* Prefer longer matches */
                    match_len = from_len;
                    replace_idx = i;
                }
            }
        }

        if (replace_idx < replace_list->count) {
            /* Match found; perform replacement */
            size_t to_len = strlen(replace_list->pairs[replace_idx].to);
            size_t current_len = res_ptr - result;
            size_t new_len = current_len + to_len + 1;
            if (new_len > buffer_size) {
                buffer_size = new_len * 2;
                char *temp = realloc(result, buffer_size);
                if (!temp) {
                    fprintf(stderr, "Memory allocation failed during replacement.\n");
                    free(result);
                    exit(1);
                }
                res_ptr = temp + (res_ptr - result);
                result = temp;
            }
            memcpy(res_ptr, replace_list->pairs[replace_idx].to, to_len);
            res_ptr += to_len;
            *res_ptr = '\0';
            current += match_len;
            *updated = 1;
        } else {
            /* No match; copy the current character */
            if ((res_ptr - result) + 2 > buffer_size) {
                buffer_size *= 2;
                char *temp = realloc(result, buffer_size);
                if (!temp) {
                    fprintf(stderr, "Memory allocation failed during copying.\n");
                    free(result);
                    exit(1);
                }
                res_ptr = temp + (res_ptr - result);
                result = temp;
            }
            *res_ptr++ = *current++;
            *res_ptr = '\0';
        }
    }

    return result;
}

/* Process a single input stream (stdin or a file) */
static int process_stream(FILE *in, FILE *out, ReplaceList *replace_list, ProgramOptions *options) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    int error = 0;

    while ((read = getline(&line, &len, in)) != -1) {
        /* Remove newline character for consistent processing */
        if (read > 0 && line[read - 1] == '\n') {
            line[read - 1] = '\0';
        }

        int updated = 0;
        char *replaced = replace_in_string(line, replace_list, &updated);
        if (!replaced) {
            fprintf(stderr, "Replacement failed.\n");
            free(line);
            return 1;
        }

        /* Add newline back */
        strcat(replaced, "\n");

        /* Write to output */
        if (fputs(replaced, out) == EOF) {
            fprintf(stderr, "Error writing to output: %s\n", strerror(errno));
            free(replaced);
            free(line);
            return 1;
        }

        if (options->verbose && updated) {
            printf("Replaced in line: %s", replaced);
        }

        free(replaced);
    }

    free(line);
    return error;
}

/* Process a single file: read, replace, write to a temporary file, then replace original */
static int process_file(const char *filename, ReplaceList *replace_list, ProgramOptions *options) {
    FILE *in = fopen(filename, "r");
    if (!in) {
        fprintf(stderr, "Failed to open file %s: %s\n", filename, strerror(errno));
        return 1;
    }

    /* Create a temporary file using mkstemp */
    char temp_template[] = "replace_tempXXXXXX";
    int temp_fd = mkstemp(temp_template);
    if (temp_fd == -1) {
        fprintf(stderr, "Failed to create temporary file: %s\n", strerror(errno));
        fclose(in);
        return 1;
    }

    FILE *out = fdopen(temp_fd, "w");
    if (!out) {
        fprintf(stderr, "Failed to open temporary file: %s\n", strerror(errno));
        close(temp_fd);
        fclose(in);
        return 1;
    }

    /* Process the file */
    int error = process_stream(in, out, replace_list, options);
    fclose(in);
    fclose(out);

    if (error) {
        /* Remove temporary file on error */
        remove(temp_template);
        return 1;
    }

    /* Replace the original file with the temporary file */
    if (remove(filename) != 0) {
        fprintf(stderr, "Failed to remove original file %s: %s\n", filename, strerror(errno));
        remove(temp_template);
        return 1;
    }
    if (rename(temp_template, filename) != 0) {
        fprintf(stderr, "Failed to rename temporary file to %s: %s\n", filename, strerror(errno));
        remove(temp_template);
        return 1;
    }

    if (!options->silent) {
        if (options->verbose) {
            printf("%s converted\n", filename);
        }
    }

    return 0;
}
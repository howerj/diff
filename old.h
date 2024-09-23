#ifndef DIFF_H
#define DIFF_H

#define DIFF_AUTHOR "Richard James Howe"
#define DIFF_LICENSE "0BSD / Public Domain"
#define DIFF_EMAIL "howe.r.j.89@gmail.com"
#define DIFF_REPO "https://github.com/howerj/diff"

#include <stddef.h>
#include <stdio.h>

typedef struct {
	unsigned long *c; /* 2D array of Longest Common Substrings */
	size_t m, n; /* 2D array dimensions */
} diff_t;

typedef struct {
	char *s;
	size_t length;
} diff_line_t;

typedef struct {
	diff_line_t *lines;
	size_t length;
} diff_file_t;

diff_t *lcs(char *x[], size_t xlen, char *y[], size_t ylen);
int diff_print_arrays(diff_t *d, FILE *out, char *x[], char *y[], size_t i, size_t j);
int diff_print(diff_t *d, FILE *out, char **x, char **y);

#endif

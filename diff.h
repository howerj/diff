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
	char *line;
	size_t length;
} diff_line_t;

typedef struct {
	diff_line_t *lines;
	size_t length;
} diff_file_t;

diff_t *diff_lcs(diff_file_t *x, diff_file_t *y);
int diff_files_print(diff_t *d, int (*put)(void *handle, int ch), void *handle, diff_file_t *a, diff_file_t *b);
char *diff_getdelim(int (*get)(void *handle), void *handle, size_t *returned_size, const int delim);
void diff_file_free(diff_file_t *f);
diff_file_t *diff_file_get(int (*get)(void *handle), void *handle);
void diff_free(diff_t *d);

#endif

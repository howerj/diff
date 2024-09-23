/**
 * TODO:
 *      * Simple optimization of ignoring beginning and end of text
 *      * Turn into library
 *      * Use print callback instead of file handle
 *      * Documentation
 *      * Replace getline / Add `slurp` function
 *      * Operate on length/strings instead of ASCIIZ strings?
 *      * Optionally ignore whitespace, case
 *      * Return error code; files differ vs error vs no difference
 * See:
 * <https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Longest_common_subsequence>
 * <http://www.algorithmist.com/index.php/Longest_Common_Subsequence>
 * <https://en.wikipedia.org/wiki/Longest_common_subsequence_problem>
 *
 * XXX BUGS:
 *      * If one file is empty it will not work, instead it returns null
 *      * unsigned array wrap around
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "diff.h"

#define DIFF_MAX(X, Y) ((X) > (Y) ? (X) : (Y))

/*
function LCS(X[1..m], Y[1..n])
    C = array(0..m, 0..n)
    for i := 0..m
       C[i,0] = 0
    for j := 0..n
       C[0,j] = 0
    for i := 1..m
        for j := 1..n
            if X[i] = Y[j]
                C[i,j] := C[i-1,j-1] + 1
            else
                C[i,j] := max(C[i,j-1], C[i-1,j])
    return C*/

diff_t *lcs(char *x[], size_t xlen, char *y[], size_t ylen) {
	assert(x);
	assert(y);
	diff_t *d = NULL;
	unsigned long *c = NULL;
	const size_t m = xlen, n = ylen;
	if (!x || !y)
		return NULL;
	if (!(c = calloc((m + 1) * (n + 1), sizeof(*c))))
		goto fail;
	if (!(d = calloc(1, sizeof(*d))))
		goto fail;
	for (size_t i = 1; i < m; i++)
		for (size_t j = 1; j < n; j++)
			if (!strcmp(x[i - 1], y[j - 1]))
				c[i * n + j] = c[(i - 1) * n + (j - 1)] + 1;
			else
				c[i * n + j] = DIFF_MAX(c[i * n + (j - 1)], c[(i - 1) * n + j]);
	d->c = c;
	d->m = m;
	d->n = n;
	return d;
fail:
	return NULL;
}

static int diff_write(int (*put)(void *handle, int ch), void *handle, const char *s, size_t length) {
	assert(put);
	for (size_t i = 0; i < length; i++) {
		const int ch = s[i];
		if (put(handle, ch) != ch) return -1;
	}
	return 0;
}

static int diff_output_line(int (*put)(void *handle, int ch), void *handle, int op, const char *s, size_t length) {
	char ops[3] = { op, ' ', 0, };
	if (diff_write(put, handle, ops, 2) < 0) return -1;
	return diff_write(put, handle, s, length);
}

/* function printDiff(C[0..m,0..n], X[1..m], Y[1..n], i, j)
    if i > 0 and j > 0 and X[i] = Y[j]
        printDiff(C, X, Y, i-1, j-1)
        print "  " + X[i]
    else if j > 0 and (i = 0 or C[i,j-1] ≥ C[i-1,j])
        printDiff(C, X, Y, i, j-1)
        print "+ " + Y[j]
    else if i > 0 and (j = 0 or C[i,j-1] < C[i-1,j])
        printDiff(C, X, Y, i-1, j)
        print "- " + X[i]
    else
        print "" */

int diff_print_arrays(diff_t *d, FILE *out, char *x[], char *y[], size_t i, size_t j) {
	assert(d);
	assert(out);
	assert(x);
	assert(y);
	// TODO: Recursion limit
	if (i > 0 && j > 0 && !strcmp(x[i - 1], y[j - 1])) {
		if (diff_print_arrays(d, out, x, y, i - 1, j - 1) < 0) return -1;
		if (fprintf(out, "  %s", x[i - 1]) < 0) return -1;
	} else if (j > 0 && (i == 0 || d->c[(i * (d->n)) + (j - 1)] >= d->c[((i - 1) * (d->n)) + j])) {
		if (diff_print_arrays(d, out, x, y, i, j - 1) < 0) return -1;
		if (fprintf(out, "+ %s", y[j - 1]) < 0) return -1;
	} else if (i > 0 && (j == 0 || d->c[(i * (d->n)) + (j - 1)] < d->c[((i - 1) * (d->n)) + j])) {
		if (diff_print_arrays(d, out, x, y, i - 1, j) < 0) return -1;
		if (fprintf(out, "- %s", x[i - 1]) < 0) return -1;
	}
	return 0;
}

int diff_print(diff_t *d, FILE *out, char **x, char **y) {
	assert(d);
	assert(out);
	assert(x);
	assert(y);
	return diff_print_arrays(d, out, x, y, d->m, d->n);
}

#ifdef DIFF_MAIN
#include <errno.h>

static void diff_free_lines(char **lines, size_t length) {
	assert(lines);
	for (size_t i = 0; i < length; i++) {
		free(lines[i]);
		lines[i] = NULL;
	}
	free(lines);
}

// TODO: Return array of string/length, make our own `getline`.
static char **diff_fill_array_with_lines(FILE *f, size_t *returned_length) {
	assert(f);
	assert(returned_length);
	char *line = NULL, **s = NULL;
	size_t ignore = 0, nl = 1;
	*returned_length = 0;
	while (getline(&line, &ignore, f) > 0) {
		if (!(s = realloc(s, (nl + 1) * sizeof(*s)))) {
			// TODO: Free
			return NULL;
		}
		s[nl - 1] = line;
		s[nl] = NULL;
		nl++;
		line = NULL;
		ignore = 0;
	}
	*returned_length = nl - 1;
	free(line);
	return s;
}

static FILE *diff_fopen_or_fail(const char *name, const char *mode) {
	assert(name);
	assert(mode);
	FILE *r = fopen(name, mode);
	if (!r) {
		(void)fprintf(stderr, "Unable to open '%s' in mode '%s': %s\n", name, mode, strerror(errno));
		exit(1);
	}
	return r;
}

int main(int argc, char **argv) {
	diff_t *d = NULL;
	int r = 0;

	if (argc != 3)
		return fprintf(stderr, "Usage: %s file file\n", argv[0]), 1;

	FILE *fa = diff_fopen_or_fail(argv[1], "r");
	FILE *fb = diff_fopen_or_fail(argv[2], "r");

	size_t la = 0, lb = 0;
	char **a = diff_fill_array_with_lines(fa, &la);
	char **b = diff_fill_array_with_lines(fb, &lb);

	if (!(d = lcs(a, la, b, lb)))
		return 0; /* empty files */
	if (diff_print(d, stdout, a, b) < 0)
		r = -1;
	if (fclose(fa) < 0) r = -1;
	if (fclose(fb) < 0) r = -1;

	diff_free_lines(a, la);
	diff_free_lines(b, lb);

	free(d->c);
	free(d);
	return r;
}
#endif

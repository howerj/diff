/**
 * TODO:
 *      * Turn into library
 *      * Documentation
 *      * Optionally ignore whitespace, case, set delim (binary diff?)
 *      * Return error code; files differ vs error vs no difference
 *      * Fuzzy search, combine with regex engine?
 *      * Print out diff matrix?
 *      * Print diff numbers / commands
 *      * Assertions on range
 *      * Levenshtein, Damerau-Levenshtein?
 *      * Unit tests
 *      * Fuzzing
 *      * Uses hashes of lines to avoid expensive comparisons?
 * See:
 *
 * <https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Longest_common_subsequence>
 * <http://www.algorithmist.com/index.php/Longest_Common_Subsequence>
 * <https://en.wikipedia.org/wiki/Longest_common_subsequence_problem>
 * <https://stackoverflow.com/questions/77711427/why-longest-common-subsequence-prohibits-substitution-using-edit-distance-me>
 *
 * XXX BUGS:
 *      * If one file is empty it will not work, instead it returns null
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "diff.h"

#define DIFF_MAX(X, Y) ((X) > (Y) ? (X) : (Y))
#define DIFF_MIN(X, Y) ((X) < (Y) ? (X) : (Y))

static int diff_line_equal(diff_line_t *a, diff_line_t *b) {
	assert(a);
	assert(b);
	if (a->length != b->length)
		return 0;
	return !memcmp(a->line, b->line, a->length);
}

diff_t *diff_lcs(diff_file_t *x, diff_file_t *y) {
	assert(x);
	assert(y);
	diff_t *d = NULL;
	unsigned long *c = NULL;
	const size_t m = x->length, n = y->length;
	if (!x || !y)
		return NULL;
	/*printf("m:%ld n:%ld r:%ld\n", m, n, (m + 1) * (n + 1) * sizeof(*c)); exit(0);*/
	if (!(c = calloc((m + 1) * (n + 1), sizeof(*c))))
		goto fail;
	if (!(d = calloc(1, sizeof(*d))))
		goto fail;
	for (size_t i = 1; i < m; i++)
		for (size_t j = 1; j < n; j++)
			if (diff_line_equal(&x->lines[i - 1], &y->lines[j - 1]))
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
		const int ch = (unsigned char)s[i];
		if (put(handle, ch) != ch) {
			return -1;
		}
	}
	return 0;
}

static int diff_output_line(int (*put)(void *handle, int ch), void *handle, int op, const char *s, size_t length) {
	const char ops[3] = { op, ' ', 0, };
	if (diff_write(put, handle, ops, 2) < 0) return -1;
	return diff_write(put, handle, s, length);
}

static int diff_print_arrays_internal(diff_t *d, int (*put)(void *handle, int ch), void *handle, diff_line_t x[], diff_line_t y[], size_t i, size_t j) {
	assert(d);
	assert(put);
	assert(x);
	assert(y);
	// TODO: Recursion limit
	if (i > 0 && j > 0 && diff_line_equal(&x[i - 1], &y[j - 1])) {
		if (diff_print_arrays_internal(d, put, handle, x, y, i - 1, j - 1) < 0) return -1;
		if (diff_output_line(put, handle, ' ', x[i - 1].line, x[i - 1].length) < 0) return -1;
	} else if (j > 0 && (i == 0 || d->c[(i * (d->n)) + (j - 1)] >= d->c[((i - 1) * (d->n)) + j])) {
		if (diff_print_arrays_internal(d, put, handle, x, y, i, j - 1) < 0) return -1;
		if (diff_output_line(put, handle, '+', y[j - 1].line, y[j - 1].length) < 0) return -1;
	} else if (i > 0 && (j == 0 || d->c[(i * (d->n)) + (j - 1)] < d->c[((i - 1) * (d->n)) + j])) {
		if (diff_print_arrays_internal(d, put, handle, x, y, i - 1, j) < 0) return -1;
		if (diff_output_line(put, handle, '-', x[i - 1].line, x[i - 1].length) < 0) return -1;
	}
	return 0;
}

int diff_files_print(diff_t *d, int (*put)(void *handle, int ch), void *handle, diff_file_t *a, diff_file_t *b) {
	assert(d);
	assert(put);
	assert(a);
	assert(b);
	return diff_print_arrays_internal(d, put, handle, a->lines, b->lines, a->length, b->length);
}

void diff_file_free(diff_file_t *f) {
	diff_line_t *lines = f->lines;
	for (size_t i = 0; i < f->length; i++) {
		free(lines[i].line);
		lines[i].line = NULL;
		lines[i].length = 0;
	}
	free(f->lines);
	f->lines = NULL;
	f->length = 0;
	free(f);
}

void diff_free(diff_t *d) {
	assert(d);
	free(d->c);
	d->c = NULL;
	d->m = 0;
	d->n = 0;
	free(d);
}

char *diff_getdelim(int (*get)(void *handle), void *handle, size_t *returned_size, const int delim) {
	assert(get);
	char *retbuf = NULL;
	const size_t start = 128;
	size_t nchmax = start, nchread = 0;
	int c = 0;
	if (returned_size)
		*returned_size = 0;
	if (!(retbuf = calloc(1, start + 1)))
		goto fail;
	while ((c = get(handle)) != EOF) {
		if (nchread >= nchmax) {
			nchmax = nchread * 2;
			if (nchread >= nchmax)	/*overflow check */
				goto fail;
			char *newbuf = realloc(retbuf, nchmax + 1);
			if (!newbuf)
				goto fail;
			retbuf = newbuf;
		}
		retbuf[nchread++] = c;
		if (c == delim) /* break before previous line to exclude delimiter */
			break;
	}
	if (!nchread && c == EOF)
		goto fail;
	if (retbuf)
		retbuf[nchread] = '\0';
	if (returned_size)
		*returned_size = nchread;
	return retbuf;
fail:
	free(retbuf);
	return NULL;
}

static int diff_getch(void *handle) {
	assert(handle);
	return fgetc((FILE*)handle);
}

static int diff_putch(void *handle, int ch) {
	assert(handle);
	return fputc(ch, (FILE*)handle);
}

diff_file_t *diff_file_get(int (*get)(void *handle), void *handle) {
	assert(get);
	diff_file_t *r = calloc(1, sizeof (*r));
	if (!r) return NULL;
	char *line = NULL;
	size_t sz = 0, nl = 1;

	while ((line = diff_getdelim(get, handle, &sz, '\n'))) {
		if (!(r->lines = realloc(r->lines, (nl + 1) * sizeof (r->lines[0])))) {
			diff_file_free(r);
			return NULL;
		}
		r->lines[nl - 1].line = line;
		r->lines[nl - 1].length = sz;
		nl++;
		line = NULL;
		r->length++;
	}
	free(line);
	return r;
}

#ifdef DIFF_MAIN
#include <errno.h>

static FILE *diff_fopen_or_fail(const char *name, const char *mode) {
	assert(name);
	assert(mode);
	const int prev = errno;
	FILE *r = fopen(name, mode);
	if (!r) {
		(void)fprintf(stderr, "Unable to open '%s' in mode '%s': %s\n", name, mode, strerror(errno));
		exit(1);
	}
	errno = prev;
	return r;
}

int main(int argc, char **argv) {
	diff_t *d = NULL;
	int r = 0, difference = 0;

	if (argc != 3) {
		(void)fprintf(stderr, "Usage: %s file file\n", argv[0]);
		return -1;
	}

	FILE *fa = diff_fopen_or_fail(argv[1], "rb");
	FILE *fb = diff_fopen_or_fail(argv[2], "rb");

	diff_file_t *a = diff_file_get(diff_getch, fa);
	diff_file_t *b = diff_file_get(diff_getch, fb);

	if (fclose(fa) < 0) r = -1;
	if (fclose(fb) < 0) r = -1;

	const size_t length = DIFF_MIN(a->length, b->length);
	size_t head = 0, tail = 0;
	for (size_t i = 0; i < length; i++) {
		if (!diff_line_equal(&a->lines[i], &b->lines[i]))
			break;
		head++;
	}
	for (size_t i = 0; i < length; i++) {
		if (!diff_line_equal(&a->lines[a->length - i - 1], &b->lines[b->length - i - 1]))
			break;
		tail++;
	}
	if (a->length == b->length && head == a->length) { /* no difference */
		difference = 0;
	} else { /* files differ */
		difference = 1;
	}

	diff_file_t da = { .lines = &a->lines[head], .length = a->length - head - tail, };
	diff_file_t db = { .lines = &b->lines[head], .length = b->length - head - tail, };
	diff_file_t *pa = &da, *pb = &db;

	if (!(d = diff_lcs(pa, pb)))
		return 0; /* empty files */

	for (size_t i = 0; i < head; i++) {
		if (diff_output_line(diff_putch, stdout, ' ', a->lines[i].line, a->lines[i].length) < 0) r = -1;
	}

	if (diff_files_print(d, diff_putch, stdout, pa, pb) < 0)
		r = -1;

	for (size_t i = 0; i < tail; i++) {
		if (diff_output_line(diff_putch, stdout, ' ', a->lines[a->length - i - 1].line, a->lines[a->length - i - 1].length) < 0) r = -1;
	}
	
	diff_file_free(a);
	diff_file_free(b);
	diff_free(d);
	return r ? r : difference;
}

#endif

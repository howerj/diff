/* TODO: Do not both loading into memory, that is not needed, this could
 * be a simple filter... */

#define SIM_AUTHOR  "Richard James Howe"
#define SIM_LICENSE "0BSD / Public Domain"
#define SIM_REPO    "https://github.com/howerj/diff"
#define SIM_EMAIL   "howe.r.j.89@gmail.com"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <locale.h> /* Locale sucks, should define my own functions... */

#ifndef SIM_SKIP
#define SIM_SKIP (0)
#endif

typedef struct {
	char *line;
	size_t length;
} sim_line_t;

typedef struct {
	sim_line_t *lines;
	size_t length;
} sim_file_t;

typedef struct {
	uint64_t *sim;
	size_t length;
} sim_index_t;

static inline int sim_popcnt(uint64_t x) {
	int r = 0;
	while (x) {
		r += x & 1;
		x >>= 1;
	}
	return r;
}

/* We could specify the matches in a more generic way, to facilitate experimentation. */
static uint64_t sim_hash(const char *s, size_t length) {
	assert(s);
	uint64_t h = 0;
	const uint8_t *u = (const uint8_t*)s;

	/* We could fold uncommon characters into a one bit; (e.g. x z -> single bit) */
	for (size_t i = 0; i < length; i++) {
		int ch = u[i];
		if (ch >= '0' && ch <= '9') {
			h |= 1ull << (ch - '0'); 
			continue;
		}
		if (ch >= 'A' && ch <= 'Z') 
			ch ^= 32;
		if (i < (length - 2)) {
			char tri[4] = { ch, u[i + 1], u[i + 2], 0, };
			int hit = 0;
			if (!strcmp(tri, "ing")) hit = 60;
			if (!strcmp(tri, "and")) hit = 61;
			if (!strcmp(tri, "the")) hit = 62;
			if (!strcmp(tri, "her")) hit = 62;
			if (hit) {
				h |= 1ull << hit;
				if (SIM_SKIP) {
					i += 2;
					continue;
				}
			}
		}
		if (i < (length - 1)) {
			char di[3] = { ch, u[i + 1], 0, };
			int hit = 0;
			if (!strcmp(di, "es")) hit = 39;
			if (!strcmp(di, "ch")) hit = 40;
			if (!strcmp(di, "sh")) hit = 41;
			if (!strcmp(di, "th")) hit = 42;
			if (!strcmp(di, "ph")) hit = 43;
			if (!strcmp(di, "wh")) hit = 44;
			if (hit) {
				h |= 1ull << hit;
				if (SIM_SKIP) {
					i += 1;
					continue;
				}
			}
		}
		if (ch >= 'a' && ch <= 'z') {
			h |= 1ull << (ch - 'a' + 10);
			continue;
		}
		if (isspace(ch)) { h |= 1ull << 36; }
		if (iscntrl(ch)) { h |= 1ull << 37; }
		if (ispunct(ch)) { h |= 1ull << 38; }
		if (ch > 127) { h |= 1ull << 63; }
	}
	return h;
}

void sim_file_free(sim_file_t *f) {
	sim_line_t *lines = f->lines;
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

char *sim_getdelim(int (*get)(void *handle), void *handle, size_t *returned_size, const int delim) {
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

static int sim_getch(void *handle) {
	assert(handle);
	return fgetc((FILE*)handle);
}

static int sim_putch(void *handle, int ch) {
	assert(handle);
	return fputc(ch, (FILE*)handle);
}

sim_file_t *sim_file_get(int (*get)(void *handle), void *handle) {
	assert(get);
	sim_file_t *r = calloc(1, sizeof (*r));
	if (!r) return NULL;
	char *line = NULL;
	size_t sz = 0, nl = 1;
	while ((line = sim_getdelim(get, handle, &sz, '\n'))) {
		if (!(r->lines = realloc(r->lines, (nl + 1) * sizeof (r->lines[0])))) {
			sim_file_free(r);
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

static FILE *sim_fopen_or_die(const char *name, const char *mode) {
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

static int sim_write(int (*put)(void *handle, int ch), void *handle, const char *s, size_t length) {
	assert(put);
	for (size_t i = 0; i < length; i++) {
		const int ch = (unsigned char)s[i];
		if (put(handle, ch) != ch)
			return -1;
	}
	return 0;
}

static int empty(const char *s, size_t length) {
	if (length == 0) return 1;
	for (size_t i = 0; i < length; i++)
		if (!isspace(s[i]))
			return 0;
	return 1;
}

static int sim_print(int (*put)(void *handle, int ch), void *handle, sim_file_t *f, const char *s, size_t length) {
	assert(put);
	assert(f);
	assert(s);
	const uint64_t hs = sim_hash(s, length);

	for (size_t i = 0; i < f->length; i++) {
		sim_line_t *l = &f->lines[i];
		if (empty(l->line, l->length)) continue;
		const uint64_t hl = sim_hash(l->line, l->length);
		const int sim = sim_popcnt(hs ^ hl);
		char b[66] = { 0, };
		if (sprintf(b, "%d, ", sim) < 0) return -1;
		if (sim_write(put, handle, b, strlen(b)) < 0) return -1;
		if (sim_write(put, handle, l->line, l->length) < 0) return -1;
	}

	return 0;
}

int main(int argc, char **argv) {
	setlocale(LC_ALL, "C");
#if 1
	if (argc != 3) {
		(void)fprintf(stderr, "Usage: %s string file\n", argv[0]);
		return 1;
	}
	
	int r = 0;
	FILE *f = sim_fopen_or_die(argv[2], "rb");
	sim_file_t *file = sim_file_get(sim_getch, f);
	if (fclose(f) < 0) r = -1;
	if (!file) return 1;
	if (sim_print(sim_putch, stdout, file, argv[1], strlen(argv[1])) < 0) r = -1;
	sim_file_free(file);
	return r;
#else
	if (argc != 3) {
		(void)fprintf(stderr, "Usage: %s string string\n", argv[0]);
		return 1;
	}
	const uint64_t ha = sim_hash(argv[1], strlen(argv[1]));
	const uint64_t hb = sim_hash(argv[2], strlen(argv[2]));
	const uint64_t x  = ha ^ hb;
	const int sim = sim_popcnt(x);
	if (fprintf(stdout, "a:%08lX b:%08lX x:%08lX s:%d\n", (long)ha, (long)hb, (long)x, sim) < 0) return 1;
	return 0;
#endif
}

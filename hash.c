#include "bolo.h"

static unsigned int
s_hash_djb2(const char *key)
{
	unsigned int h = 5381;

	for (; *key; key++)
		h = ((h << 5) + h) + *key; /* h * 33 + c */

	return h;
}

#define s_hash s_hash_djb2

struct hbkt {
	char *   key;
	void *   ptr;
	uint64_t val;

	struct hbkt *next;
};

#define HASH_STRIDE  255
struct hash {
	size_t nset;
	struct hbkt *hbkts[HASH_STRIDE];

	/* for iteration */
	int i;
	struct hbkt *last;
};

struct hash *
hash_new()
{
	return xalloc(1, sizeof(struct hash));
}

void
hash_free(struct hash *h)
{
	int i;
	struct hbkt *b, *tmp;

	if (!h)
		return;

	for (i = 0; i < HASH_STRIDE; i++) {
		for (b = h->hbkts[i]; b; ) {
			free(b->key);

			tmp = b->next;
			free(b);
			b = tmp;
		}
	}

	free(h);
}

void
_hash_ebegn(struct hash *h, void *key, void *val)
{
	CHECK(h != NULL, "hash_each() { ... } given a NULL hash to iterate over");

	h->i = 0;
	h->last = NULL;
	_hash_enext(h, key, val);
}

void
_hash_enext(struct hash *h, void *key, void *val)
{
	CHECK(h != NULL,                  "hash_each() { ... } given a NULL hash to iterate over");
	CHECK(key != NULL || val != NULL, "hash_each() { ... } given NULL key and value destination pointers");

	while (h->i < HASH_STRIDE && !h->last) {
		h->last = h->hbkts[h->i++];
	}

	if (h->last) {
		if (key) *(char **)key = h->last->key;
		if (val) *(void **)val = h->last->ptr;
		h->last = h->last->next;
	}
}

int
_hash_edone(struct hash *h)
{
	return h->i >= HASH_STRIDE && h->last == NULL;
}

size_t
hash_nset(struct hash *h)
{
	CHECK(h != NULL, "hash_nset() given a NULL hash to query");
	return h->nset;
}

int
hash_set(struct hash *h, const char *key, void *val)
{
	unsigned int k;
	struct hbkt *b;

	CHECK(h != NULL,   "hash_set() given a NULL hash to insert into");
	CHECK(key != NULL, "hash_set() given a NULL key to insert");

	k = s_hash(key) % HASH_STRIDE;

	/* check existing data */
	for (b = h->hbkts[k]; b; b = b->next) {
		if (!streq(b->key, key))
			continue;

		b->ptr = val;
		return 0;
	}

	/* not in existing data, prepend a new hbkt */
	b = xalloc(1, sizeof(struct hbkt));
	b->key = strdup(key);
	b->ptr = val;
	b->next = h->hbkts[k];
	h->hbkts[k] = b;
	h->nset++;
	return 0;
}

int
hash_get(struct hash *h, void *dst, const char *key)
{
	unsigned int k;
	struct hbkt *b;

	CHECK(h != NULL,   "hash_get() given a NULL hash to query");
	CHECK(key != NULL, "hash_get() given a NULL key to lookup");

	k = s_hash(key) % HASH_STRIDE;

	/* check existing data */
	for (b = h->hbkts[k]; b; b = b->next) {
		if (!streq(b->key, key))
			continue;

		if (dst)
			*(void **)dst = b->ptr;
		return 0;
	}

	/* key not set in the hash */
	errno = BOLO_ENOTSET;
	return -1;
}

struct hash *
hash_read(int from, hash_reader_fn reader, void *udata)
{
	struct hash *h;
	int fd;
	FILE *in;
	char buf[8192], *a, *b;
	uint64_t value;
	void *ptr;

	CHECK(from >= 0, "hash_read() given an invalid file descriptor to read from");

	lseek(from, 0, SEEK_SET);

	fd = dup(from);
	if (fd < 0)
		return NULL;

	h = NULL;
	in = fdopen(fd, "r");
	if (!in)
		goto fail;

	h = hash_new();
	while (fgets(buf, 8192, in) != NULL) {
		a = strchr(buf, '\t');
		b = strchr(buf, '\n');

		errno = BOLO_EBADHASH;
		if (!a && !b)
			goto fail;

		*a++ = *b = '\0';
		value = strtoul(a, &b, 10);
		if (b && *b)
			goto fail;

		ptr = reader(buf, value, udata);
		if (!ptr)
			goto fail;

		if (hash_set(h, buf, ptr) != 0)
			goto fail;
	}

	fclose(in);
	return h;

fail:
	fclose(in);
	hash_free(h);
	return NULL;
}

int
hash_write(struct hash *h, int to, hash_writer_fn writer, void *udata)
{
	FILE *out;
	int i, fd;
	struct hbkt *b;

	CHECK(h != NULL, "hash_write() given a NULL hash pointer to write");
	CHECK(to >= 0,   "hash_write() given an invalid file descriptor to write to");

	if (ftruncate(to, 0) < 0)
		return -1;

	lseek(to, 0, SEEK_SET);

	fd = dup(to);
	if (fd < 0)
		return -1;

	out = fdopen(fd, "w");
	if (!out) {
		close(fd);
		return -1;
	}

	for (i = 0; i < HASH_STRIDE; i++)
		for (b = h->hbkts[i]; b; b = b->next)
			fprintf(out, "%s\t%lu\n", b->key, writer(b->key, b->ptr, udata));

	fclose(out);
	return 0;
}


#ifdef TEST
/* LCOV_EXCL_START */
struct data {
	uint64_t id;
	/* don't need any other fields... */
};

static uint64_t test_writer1(const char *k, void *_ptr, void *_)
{
	insist(_ptr != NULL, "_ptr must not be NULL");
	return ((struct data *)_ptr)->id;
}
/* _u (userdata) will be a contiguous array of
   struct data pointers, NULL-terminated. */
static void * test_reader1(const char *k, uint64_t v, void *_u)
{
	struct data **u, *i;

	insist(_u != NULL, "_u must not be NULL");

	u = (struct data **)_u;
	for (i = *u; i; i++)
		if (i->id == v)
			return i;

	/* not found! */
	return NULL;
}

TESTS {
	subtest {
		isnt_unsigned(
			s_hash("some string"),
			s_hash("some other string"),
			"s_hash() shouldn't collide on different keys");

		is_unsigned(
			s_hash("identical"),
			s_hash("identical"),
			"s_hash() should collide on identical keys");
	}

	subtest {
		struct hash *h;
		const char *v;

		h = hash_new();
		is_unsigned(hash_nset(h), 0, "hash initially has 0 keys");
		ok(!hash_isset(h, "one"), "h[one] should not be set");

		v = NULL;
		ok(hash_get(h, &v, "one") != 0,
			"hash_get(k) should fail, since k is not set in the hash");

		ok(hash_set(h, "one", "the first value") == 0,
			"hash_set(k,v) should succeed");

		is_unsigned(hash_nset(h), 1, "hash has 1 key after a hash_set");
		ok(hash_isset(h, "one"), "h[one] should be set");

		v = NULL;
		ok(hash_get(h, &v, "one") == 0,
			"hash_get(k) should succeed");
		is_string(v, "the first value",
			"hash_get(k) should retrieve what we hash_set(k)");

		hash_free(h);
	}

	subtest {
		struct hash *h;
		int fd;
		struct data *data, *lst[2], *result;

		h = hash_new();
		fd = memfd("hash");

		data = xmalloc(sizeof(struct data));
		data->id = 0xdecafbad;
		lst[0] = data;
		lst[1] = NULL;

		if (hash_set(h, "key", data) != 0)
			BAIL_OUT("failed to set up the test for hash_read/write");

		is_unsigned(hash_nset(h), 1, "hash has 1 value set before write/read");
		ok(hash_write(h, fd, test_writer1, lst) == 0,
			"hash_write() should succeed");

		hash_free(h);

		h = hash_read(fd, test_reader1, lst);
		if (!h)
			BAIL_OUT("failed to read hash back in from the fd");

		is_unsigned(hash_nset(h), 1, "hash has 1 value set after re-read");
		ok(hash_get(h, &result, "key") == 0,
			"hash_get() should succeed after re-read");
		is_ptr(result, data, "hash_get() should return pointer set by reader fn");

		hash_free(h);
		close(fd);
		free(data);
	}

	subtest {
		struct hash *h;
		char *key;
		int i;
		struct data d;

		h = hash_new();
		is_unsigned(hash_nset(h), 0, "hash initially has 0 keys");
		for (i = 0; i < HASH_STRIDE + 1; i++) {
			if (asprintf(&key, "key%d", i) <= 0)
				BAIL_OUT("failed to generate a key for pigeon-hole test");

			if (hash_set(h, key, &d) != 0)
				fail("failed to set key %s => (data) in pigeon-hole test");
			free(key);
		}
		is_unsigned(hash_nset(h), HASH_STRIDE + 1, "each unique key/value increments hash cardinality");

		hash_free(h);
	}

	subtest {
		struct hash *h;
		struct data d1, d2, *v;

		h = hash_new();
		ok(hash_set(h, "x", &d1) == 0, "should set x => d1");
		ok(hash_get(h, &v, "x") == 0, "should be able to retrieve 'x'");
		is_ptr(v, &d1, "'x' should be mapped to d1");

		ok(hash_set(h, "x", &d2) == 0, "should set x => d2");
		ok(hash_get(h, &v, "x") == 0, "should be able to retrieve 'x'");
		is_ptr(v, &d2, "'x' should now be mapped to d2");

		hash_free(h);
	}

	subtest {
		struct hash *before, *after;
		struct data d1, d2, *v;
		char *key;

		before = hash_new();
		ok(hash_set(before, "d1", &d1) == 0, "should set before[d1] => d1");
		ok(hash_set(before, "d2", &d2) == 0, "should set before[d2] => d2");

		after = hash_new();
		ok(hash_get(after, &v, "d1") != 0, "after[d1] is not set pre-traversal");
		ok(hash_get(after, &v, "d2") != 0, "after[d2] is not set pre-traversal");

		hash_each(before, &key, &v) {
			hash_set(after, key, v);
		}

		ok(hash_get(after, &v, "d1") == 0, "after[d1] is set pre-traversal");
		ok(hash_get(after, &v, "d2") == 0, "after[d2] is set pre-traversal");

		hash_free(before);
		hash_free(after);
	}
}
/* LCOV_EXCL_STOP */
#endif

#include "bolo.h"

/* a SLAB endian-check magic number, to be
   read/written as a 32-bit unsigned integer.

   translates into BE as [hex 7e d1 32 4c]
               and LE as [hex 4c 32 d1 7e] */
#define ENDIAN_MAGIC 2127639116U

int tslab_map(struct tslab *s, int fd)
{
	CHECK(s != NULL, "tslab_map() given a NULL tslab to map");
	CHECK(fd >= 0,   "tslab_map() given an invalid file descriptor");

	int i, rc;
	char header[TSLAB_HEADER_SIZE];
	ssize_t nread;
	off_t n;

	errno = BOLO_EBADSLAB;
	nread = read(fd, header, TSLAB_HEADER_SIZE);
	if (nread < 0) /* read error! */
		return -1;

	if (nread != TSLAB_HEADER_SIZE) /* short read! */
		return -1;

	if (memcmp(header, "SLABv1", 6) != 0) /* not a slab! */
		return -1;

	/* check the HMAC */
	errno = BOLO_EBADHMAC;
	if (s->key && hmac_check(s->key->key, s->key->len, header, TSLAB_HEADER_SIZE) != 0)
		return -1;

	/* check host endianness vs file endianness */
	errno = BOLO_EENDIAN;
	if (read32(header, 8) != ENDIAN_MAGIC)
		return -1;

	s->fd         = fd;
	s->block_size = (1 << read8(header, 6));
	s->number     = read64(header, 16);

	n = lseek(s->fd, 0, SEEK_END);
	if (n < 0)
		return -1;
	n -= 4096;

	errno = BOLO_EBADSLAB;
	if (n == 0) /* orphaned header */
		return -1;

	/* scan blocks! */
	memset(s->blocks, 0, sizeof(s->blocks));
	lseek(s->fd, 4096, SEEK_SET);
	for (i = 0; i < TBLOCKS_PER_TSLAB && n > 0; i++, n -= s->block_size) {
		s->blocks[i].key = s->key;
		rc = tblock_map(&s->blocks[i], s->fd,
		                4096 + i * s->block_size, /* grab the i'th block */
		                s->block_size);
		if (rc != 0)
			return -1;

		s->blocks[i].valid = 1;
	}

	return 0;
}

int tslab_unmap(struct tslab *s)
{
	int i, ok;

	CHECK(s != NULL, "tslab_unmap() given a NULL tslab");

	ok = 0;
	for (i = 0; i < TBLOCKS_PER_TSLAB; i++) {
		if (!s->blocks[i].valid)
			break;

		s->blocks[i].valid = 0;
		if (tblock_unmap(&s->blocks[i]) != 0)
			ok = -1;
	}

	close(s->fd);
	return ok;
}

int tslab_sync(struct tslab *s)
{
	int i, ok;

	CHECK(s != NULL, "tslab_sync() given a NULL tslab to synchronize");

	ok = 0;
	for (i = 0; i < TBLOCKS_PER_TSLAB; i++) {
		if (!s->blocks[i].valid)
			break;

		if (tblock_sync(&s->blocks[i]) != 0)
			ok = -1;
	}

	return ok;
}

int tslab_init(struct tslab *s, int fd, uint64_t number, uint32_t block_size)
{
	char header[TSLAB_HEADER_SIZE];
	size_t nwrit;

	CHECK(s != NULL, "tslab_init() given a NULL tslab to initialize");
	CHECK(block_size == (1 << 19), "tslab_init() given a non-standard block size");

	memset(header, 0, sizeof(header));
	memcpy(header, "SLABv1", 6);
	write8(header,   6, 19); /* from block_size, ostensibly */
	write32(header,  8, ENDIAN_MAGIC);
	write64(header, 16, tslab_number(number));
	if (s->key)
		hmac_seal(s->key->key, s->key->len, header, sizeof(header));

	lseek(fd, 0, SEEK_SET);
	nwrit = write(fd, header, sizeof(header));
	if (nwrit != sizeof(header))
		return -1;

	/* align to a page boundary */
	lseek(fd, sysconf(_SC_PAGESIZE) - 1, SEEK_SET);
	if (write(fd, "\0", 1) != 1)
		return -1;

	s->fd         = fd;
	s->block_size = block_size;
	s->number     = number;
	memset(s->blocks, 0, sizeof(s->blocks));

	return 0;
}

int tslab_isfull(struct tslab *s)
{
	int i;

	CHECK(s != NULL, "tslab_isfull() given a NULL tslab to query");

	for (i = 0; i < TBLOCKS_PER_TSLAB; i++)
		if (!s->blocks[i].valid)
			return 0; /* this block is avail; slab is not full */

	return 1; /* no blocks avail; slab is full */
}

int tslab_extend(struct tslab *s, bolo_msec_t base)
{
	int i;
	off_t start;
	size_t len;

	CHECK(s != NULL, "tslab_extend() given a NULL tslab to extend");

	/* seek to the end of the fd, so we can extend it */
	if (lseek(s->fd, 0, SEEK_END) < 0)
		return -1;

	/* find the first available (!valid) block */
	for (i = 0; i < TBLOCKS_PER_TSLAB; i++) {
		if (s->blocks[i].valid)
			continue;

		len   = s->block_size;
		start = sysconf(_SC_PAGESIZE) + i * len;
		CHECK(len == (1 << 19), "tslab_extend() was told to extend a tslab with a non-standard block size");

		/* track the encryption key */
		s->blocks[i].key = s->key;

		/* extend the file descriptor one TBLOCK's worth */
		if (lseek(s->fd, start + len - 1, SEEK_SET) < 0)
			return -1;
		if (write(s->fd, "\0", 1) != 1)
			return -1;

		/* map the new block into memory */
		if (tblock_map(&s->blocks[i], s->fd, start, len) == 0) {
			tblock_init(&s->blocks[i], tslab_number(s->number) | i, base);
			return 0;
		}

		/* map failed; truncate the fd back to previous size */
		ftruncate(s->fd, start);
		lseek(s->fd, 0, SEEK_END);

		/* ... and signal failure */
		return -1;
	}

	/* this slab is full; signal failure */
	return -1;
}

struct tblock *
tslab_tblock(struct tslab *s, uint64_t id, bolo_msec_t ts)
{
	CHECK(s != NULL, "tslab_tblock() given a NULL tslab to query");

	errno = BOLO_ENOBLOCK;
	if (tslab_number(id) != s->number)
		return NULL;

	errno = BOLO_EBLKCONT;
	if (!s->blocks[tblock_number(id)].valid) {
		/* FIXME: right now, we require the tblock to be next in line */
		if (tblock_number(id) > 0
		 && !s->blocks[tblock_number(id - 1)].valid)
			return NULL;

		if (tslab_extend(s, ts) != 0)
			return NULL;

		CHECK(s->blocks[tblock_number(id)].valid, "tslab_tblock() detected tblock corruption; the desired tblock was not marked as valid after extension");
	}
	return &(s->blocks[tblock_number(id)]);
}

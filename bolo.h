#ifndef BOLO_H
#define BOLO_H

#define BOLO_VERSION_MAJOR 0
#define BOLO_VERSION_MINOR 0
#define BOLO_VERSION_POINT 5
#define BOLO_BUILD_DATE "20180411.012136"

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE /* for asprintf */

#ifdef TEST
#  define _GNU_SOURCE /* to expose syscall() */
#  include <ctap.h>
#  include "t/memfd.h"
#endif

#define GITHUB_REPO   "jhunt/bolo-roboto"
#define THIS_IS_A_BUG " - this is a bug; please open an issue against https://github.com/" GITHUB_REPO

#define ASSERTION_VERBOSE   1
//#define ASSERTION_DEBUGGING 1
#include "insist.h"
#define CHECK(t,msg) insist(t,msg THIS_IS_A_BUG)

#include "compiler.h"
#include "errno.h"

#include <errno.h>

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>


#ifndef DEFAULT_QUERY_SAMPLES
#define DEFAULT_QUERY_SAMPLES 2048
#endif

#ifndef DEFAULT_QUERY_CF
#define DEFAULT_QUERY_CF CF_MEDIAN
#endif

#ifndef DEFAULT_BUCKET_STRIDE
#define DEFAULT_BUCKET_STRIDE 60
#endif

#ifndef DEFAULT_QUERY_WINDOW
#define DEFAULT_QUERY_WINDOW 14400
#endif



#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif


/*************************************************  truly global variables  ***/

/* belongs to db.o */
extern const char *ENC_KEY;
extern size_t      ENC_KEY_LEN;

/*******************************************************  common utilities  ***/

#define xmalloc(len)   xalloc(1,   (len))
#define xcalloc(n,len) xalloc((n), (len))
static inline void *
xalloc(size_t n, size_t len)
{
	void *p = calloc(n, len);
	if (p) return p;
	fprintf(stderr, "***** MEMORY ALLOCATION FAILED *****\n");
	exit(5);
}

#define streq(a,b) (strcmp((a),(b)) == 0)
void bail(const char *msg);
const char * error(int num) RETURNS;

#define DEV_URANDOM "/dev/urandom"
uint32_t urandn(uint32_t n);
uint32_t urand32();
uint64_t urand64();
int urand(void *buf, size_t len);

int mktree(int dirfd, const char *path, mode_t mode) RETURNS;

struct list {
	struct list *next;
	struct list *prev;
};

#define item(l,t,m) ((t*)((uint8_t*)(l) - offsetof(t,m)))
#define for_each(v,l,m) \
	for ( v = item((l)->next, typeof(*v), m); \
	     &v->m != (l); v = item(v->m.next, typeof(*v), m))

#define for_eachx(v,t,l,m) \
	for ( v = item((l)->next, typeof(*v), m), t = item(v->m.next, typeof(*v), m); \
	     &v->m != (l); v = t, t = item(t->m.next, typeof(*t), m))

#define empty(l) ((l)->next = (l)->prev = (l))
#define isempty(l) ((l)->next == (l))

size_t len(const struct list *l) RETURNS;
void push(struct list *list, struct list *add);
void delist(struct list *node);

#define LOG_ERRORS   0
#define LOG_WARNINGS 1
#define LOG_INFO     2

void startlog(const char *bin, pid_t pid, int level);
void logto(int fd);
void errorf(const char *fmt, ...);
void errnof(const char *fmt, ...);
void warningf(const char *fmt, ...);
void infof(const char *fmt, ...);

int  debugto(int fd);
#define debugf(...) debugf2(__func__, __FILE__, __LINE__, __VA_ARGS__)
void debugf2(const char *func, const char *file, unsigned long line, const char *fmt, ...);


/********************************************************************  time ***/

typedef uint64_t bolo_msec_t;
typedef uint32_t bolo_sec_t;

#define INVALID_MS (bolo_msec_t)(-1)
#define INVALID_S  (bolo_sec_t)(-1)

bolo_msec_t bolo_ms(const struct timeval *tv) RETURNS;
bolo_msec_t bolo_s (const struct timeval *tv) RETURNS;

/*****************************************************************  config  ***/

#define AGENT_CONFIG 1
#define CORE_CONFIG  2
struct agent_check {
	unsigned int  interval;   /* how often (in ms) to run this check */
	char         *cmdline;    /* command to execute via `/bin/sh -c ...` */
	struct hash  *env;        /* shared environment for execution;
	                             (points back to agent_config.env) */

	bolo_msec_t next_run;     /* timestamp (ms) of earliest subsequent run */
	struct list q;            /* current scheduling queue */
};

struct agent_config {
	int log_level;

	char         *bolo_endpoint;
	int           schedule_splay;
	int           max_runners;

	size_t              nchecks;
	struct agent_check *checks;

	struct hash *env;
};

struct core_config {
	int log_level;

	/* db.* - database settings */
	char *db_secret_key;
	char *db_data_root;

	/* query.* - query listener settings */
	char *query_listen;
	int   query_max_connections;

	/* metric.* - metric listener settings */
	char *metric_listen;
	int   metric_max_connections;
};

int configure(int type, void *, int fd) RETURNS;
void deconfigure(int type, void *);


/****************************************************************  SHA-512  ***/

#define SHA512_DIGEST   64
#define SHA512_BLOCK   128

struct sha512 {
	uint64_t state[8];
	uint64_t bytes[2];
	uint64_t block[16];
};

void sha512_init(struct sha512 *c);
void sha512_feed(struct sha512 *c, const void *buf, size_t len);
void sha512_done(struct sha512 *c);

int sha512_raw(struct sha512 *c, void *digest, size_t len) RETURNS;
int sha512_hex(struct sha512 *c, void *digest, size_t len) RETURNS;

/***********************************************************  HMAC-SHA-512  ***/

struct hmac_sha512 {
	struct sha512 sha;
	char key[128];
};

void hmac_sha512_init(struct hmac_sha512 *c, const char *key, size_t len);
void hmac_sha512_feed(struct hmac_sha512 *c, const void *buf, size_t len);
void hmac_sha512_done(struct hmac_sha512 *c);

int hmac_sha512_raw(struct hmac_sha512 *c, void *hmac, size_t len) RETURNS;
int hmac_sha512_hex(struct hmac_sha512 *c, void *hmac, size_t len) RETURNS;

void hmac_sha512_seal (const char *key, size_t klen, const void *buf, size_t len);
int  hmac_sha512_check(const char *key, size_t klen, const void *buf, size_t len) RETURNS;
#define hmac_seal  hmac_sha512_seal
#define hmac_check hmac_sha512_check


/*****************************************************************  fdpoll  ***/

#ifndef FDPOLL_MAX_EVENTS
#define FDPOLL_MAX_EVENTS 100
#endif

#define FDPOLL_READ  1
#define FDPOLL_WRITE 2

struct fdpoll;
typedef int (*fdpoll_fn)(int, void *);
struct fdpoll * fdpoller(int max);
int fdpoll_watch(struct fdpoll *fdp, int fd, int flags, fdpoll_fn fn, void *udata);
int fdpoll_unwatch(struct fdpoll *fdp, int fd);
void fdpoll_timeout(struct fdpoll *fdp, int timeout_ms, fdpoll_fn fn, void *udata);
void fdpoll_every(struct fdpoll *fdp, fdpoll_fn fn, void *udata);
int fdpoll(struct fdpoll *fdp);


/****************************************************************  network  ***/

int net_bind(const char *addr, int backlog);
int net_connect(const char *addr);


/****************************************************************  hashing  ***/

struct hash;
typedef void *   (*hash_reader_fn)(const char *k, uint64_t v, void *udata);
typedef uint64_t (*hash_writer_fn)(const char *k, void    *v, void *udata);

struct hash * hash_new();
void hash_free(struct hash *h);

struct hash * hash_read(int fd, hash_reader_fn fn, void *udata);
int hash_write(struct hash *h, int fd, hash_writer_fn fn, void *udata);

int hash_set(struct hash *h, const char *key, void *value);
int hash_get(struct hash *h, void *dst, const char *key);
size_t hash_nset(struct hash *h);
#define hash_isset(h,k) (hash_get((h), NULL, (k)) == 0)
#define hash_isempty(h) (hash_nset((h)) == 0)

void _hash_ebegn(struct hash *h, void *key, void *val);
void _hash_enext(struct hash *h, void *key, void *val);
int  _hash_edone(struct hash *h);
/* usage: hash_each(h, &key, &value) { ... } */
#define hash_each(h,k,v) \
	for (_hash_ebegn((h), k, v); \
	    !_hash_edone((h)); \
	     _hash_enext((h), k, v))

/***********************************************************  bit twiddling ***/

#define MAX_U8  0xff
#define MAX_U16 0xffff
#define MAX_U32 0xffffffff
#define MAX_U64 0xffffffffffffffff

#define read8(b,o)   (*(uint8_t  *)((const uint8_t*)(b)+(o)))
#define read16(b,o)  (*(uint16_t *)((const uint8_t*)(b)+(o)))
#define read32(b,o)  (*(uint32_t *)((const uint8_t*)(b)+(o)))
#define read64(b,o)  (*(uint64_t *)((const uint8_t*)(b)+(o)))
#define read64f(b,o) (*(double   *)((const uint8_t*)(b)+(o)))

static inline void  write8 (void *b, size_t o, uint8_t  v) { memmove((uint8_t *)b+o, &v, 1); }
static inline void write16 (void *b, size_t o, uint16_t v) { memmove((uint8_t *)b+o, &v, 2); }
static inline void write32 (void *b, size_t o, uint32_t v) { memmove((uint8_t *)b+o, &v, 4); }
static inline void write64 (void *b, size_t o, uint64_t v) { memmove((uint8_t *)b+o, &v, 8); }
static inline void write64f(void *b, size_t o, double   v) { memmove((uint8_t *)b+o, &v, 8); }

static inline void writen(void *b, size_t o, const void *x, size_t l)
{ memmove((uint8_t *)b+o, x, l); }

/***********************************************************  mmap'd paging ***/

struct page {
	int     fd;
	void   *data;
	size_t  len;
};


int page_map  (struct page *p, int fd, off_t start, size_t len) RETURNS;
int page_unmap(struct page *p) RETURNS;
int page_sync (struct page *p) RETURNS;

uint8_t  page_read8  (struct page *p, size_t o);
uint16_t page_read16 (struct page *p, size_t o);
uint32_t page_read32 (struct page *p, size_t o);
uint64_t page_read64 (struct page *p, size_t o);
double   page_read64f(struct page *p, size_t o);

void page_write8  (struct page *p, size_t o, uint8_t  v);
void page_write16 (struct page *p, size_t o, uint16_t v);
void page_write32 (struct page *p, size_t o, uint32_t v);
void page_write64 (struct page *p, size_t o, uint64_t v);
void page_write64f(struct page *p, size_t o, double   v);

void page_writen(struct page *p, size_t o, const void *buf, size_t len);
ssize_t page_readn(struct page *p, size_t o, void *buf, size_t len);

/******************************************************************  btree  ***/

/* Almost all systems have 4k or 8k memory pages, a fact
   which can be verified with sysconfig(_SC_PAGESIZE), so
   we will make our btree pages 8k, a multiple of each.
 */
#define BTREE_PAGE_SIZE 8192

/* The degree of a btree governs how many keys each page
   can store.  Since the nodes flank the keys, a btree
   of degree K has K+1 nodes.

   We reserve 1b for flagging this page as a leaf node,
   and 2b (a 16-bit value) to track how many keys are
   actually in use.

   Specifically, this btree implementation stores 64-bit
   keys and 64-bit values, so the degree can be calculated
   as the page size, less 3 octets for header data, less
   another 8 octets (64 bits for the +1 node), divided by the
   composite key+value size (16 octets, or 2x 64-bit values).
 */
#define BTREE_DEGREE ((BTREE_PAGE_SIZE - 1 - 2 - 8) / 16)

/* The btree split factor governs how a btree node is split
   into two pieces to ensure balance.  It ranges (0,1) and
   acts as a percentage.  I.e. a value of 0.5 (50%) nets a
   "classical" btree tuned for random-order insertion.

   Since our btree uses timestamps as its keys, and most
   values will be inserted in-order, we choose a split factor
   higher than 0.5 to bias the balance towards inserting
   "newer" keys.
 */
#define BTREE_SPLIT_FACTOR 0.9

struct btree {
	uint16_t used;  /* how many keys are populated?
	                  (must be strictly <= BTREE_DEGREE */

	int leaf;      /* is this node a leaf node?
	                  (leaf nodes contain immediate data,
	                   non-leaf nodes point to other nodes) */

	uint64_t id;   /* identity of this block, on-disk */

	struct btree *kids[BTREE_DEGREE+1];

	struct page page;
};

struct btree * btree_create(int fd);
struct btree * btree_read(int fd);
int btree_write(struct btree *t);
int btree_close(struct btree *t);

void btree_print(struct btree *t);

int btree_insert(struct btree *t, bolo_msec_t key, uint64_t block_number);
int btree_find(struct btree *t, uint64_t *dst, bolo_msec_t key);
int btree_isempty(struct btree *t);
bolo_msec_t btree_first(struct btree *t);
bolo_msec_t btree_last(struct btree *t);

/***************************************************************  database  ***/

/* a SLAB file can be up to 8g in size
   (plus a single page for the header) */
#define TSLAB_MAX_SIZE    (1 << 30)
#define TSLAB_HEADER_SIZE 88

/* a BLOCK in a SLAB is exactly 512k
   with a 32b header and an HMAC-SHA512
   footer, leaving 524,192b for data */
#define TBLOCK_SIZE         (1 << 19)
#define TBLOCK_HEADER_SIZE  32
#define TBLOCK_DATA_SIZE    (TBLOCK_SIZE - TBLOCK_HEADER_SIZE - SHA512_DIGEST)

/* each CELL has a 4b relative timestamp
   (ms), and an 8b IEEE-754 float64 value */
#define TCELL_SIZE       12

#define TBLOCKS_PER_TSLAB (TSLAB_MAX_SIZE  / TBLOCK_SIZE)
#define TCELLS_PER_TBLOCK (TBLOCK_DATA_SIZE / TCELL_SIZE)

#define tslab_number(x)  ((x) & ~0x7ff)
#define tblock_number(x) ((x) &  0x7ff)

#ifndef DEFAULT_KEY_SIZE
#define DEFAULT_KEY_SIZE 128
#endif

struct dbkey {
	char   *key;
	size_t  len;
};

/* these are part of db.o */
struct dbkey * rand_key(size_t len) RETURNS;
struct dbkey * read_key(const char *s) RETURNS;

/********************************************************  database blocks  ***/

typedef double bolo_value_t;

struct tblock {
	int valid;         /* is this block real? */
	int cells;         /* how many cells are in use? */
	bolo_msec_t base;  /* base timestamp (ms) for this block */

	uint64_t number;   /* composite slab / block number,
	                      where bits 0-53 are the slab number
	                      and bits 54-63 are the block number */
	uint64_t next;     /* block number of the next logical block
	                      in the (chronologically ordered) series */

	struct dbkey *key; /* encryption key to use */
	struct page page;  /* backing data page */
};

#define tblock_read8(  b,o) page_read8  (&(b)->page, (o))
#define tblock_read16( b,o) page_read16 (&(b)->page, (o))
#define tblock_read32( b,o) page_read32 (&(b)->page, (o))
#define tblock_read64( b,o) page_read64 (&(b)->page, (o))
#define tblock_read64f(b,o) page_read64f(&(b)->page, (o))

#define tblock_write8(  b,o,v) page_write8  (&(b)->page, (o), (v))
#define tblock_write16( b,o,v) page_write16 (&(b)->page, (o), (v))
#define tblock_write32( b,o,v) page_write32 (&(b)->page, (o), (v))
#define tblock_write64( b,o,v) page_write64 (&(b)->page, (o), (v))
#define tblock_write64f(b,o,v) page_write64f(&(b)->page, (o), (v))

int tblock_map(struct tblock *b, int fd, off_t offset, size_t len) RETURNS;
void tblock_init(struct tblock *b, uint64_t number, bolo_msec_t base);
int tblock_isfull(struct tblock *b) RETURNS;
int tblock_canhold(struct tblock *b, bolo_msec_t when) RETURNS;
int tblock_insert(struct tblock *b, bolo_msec_t when, bolo_value_t what) RETURNS;
void tblock_next(struct tblock *b, struct tblock *next);
#define tblock_unmap(b) page_unmap(&(b)->page)
#define tblock_sync(b)  page_sync(&(b)->page)

#define tblock_seal( b,k) hmac_seal ((k)->key,(k)->len,(b)->page.data,(b)->page.len)
#define tblock_check(b,k) hmac_check((k)->key,(k)->len,(b)->page.data,(b)->page.len)

#define tblock_value(b,n)              tblock_read64f((b), 32 + (n) * 12 + 4)
#define tblock_ts(b,n)    ((b)->base + tblock_read32 ((b), 32 + (n) * 12))

/*********************************************************  database slabs  ***/

struct tslab {
	struct list l;           /* list hook for database slab refs */

	int fd;                  /* file descriptor of the slab file */
	uint32_t block_size;     /* how big is each data block page? */
	uint64_t number;         /* slab number, with the least significant
	                            11-bits cleared. */

	struct dbkey *key;       /* encryption key to use */

	struct tblock                 /* list of all blocks in this slab.    */
	  blocks[TBLOCKS_PER_TSLAB];  /* present blocks will have .valid = 1 */
};

int tslab_map(struct tslab *s, int fd) RETURNS;
int tslab_unmap(struct tslab *s) RETURNS;
int tslab_sync(struct tslab *s) RETURNS;
int tslab_init(struct tslab *s, int fd, uint64_t number, uint32_t block_size) RETURNS;
int tslab_isfull(struct tslab *s) RETURNS;
int tslab_extend(struct tslab *s, bolo_msec_t base);
struct tblock * tslab_tblock(struct tslab *s, uint64_t id, bolo_msec_t ts);

/***************************************************************  database  ***/

struct idx {
	struct list l;         /* list hook for database idxrefs */
	struct btree *btree;   /* balanced B-tree of ts -> slabid */
	uint64_t number;       /* unique identifier for this index */
};

struct multidx {
	struct list l;          /* list hook for memory management (free) */
	struct multidx *next;   /* list hook for chaining tag pointers */
	struct idx     *idx;    /* pointer to a tagged time-series */
};

struct db {
	int           rootfd;   /* file descriptor of database root directory */
	struct hash  *main;     /* primary time series (name|tag,tags,... => <block-id>) */
	struct hash  *tags;     /* auxiliary tag lookup (tag => [idx], tag=value => [idx]) */
	struct hash  *metrics;  /* auxiliary metric lookup (name => [idx]) */

	struct list   idx;      /* unsorted list of time series indices */
	struct list   slab;     /* unsorted list of tslab structures */
	struct list   multidx;  /* list of allocated multidx's, for freeing later */

	struct dbkey *key;      /* database integrity signing key */

	uint64_t next_tblock;   /* ID of the next tblock to hand out */
};

struct db * db_mount(const char *path, struct dbkey *k) RETURNS;
struct db * db_init(const char *path, struct dbkey *k) RETURNS;
int db_sync(struct db *db) RETURNS;
int db_unmount(struct db *db) RETURNS;
int db_insert(struct db *, char *name, bolo_msec_t when, bolo_value_t what) RETURNS;
struct tblock * db_findblock(struct db *, uint64_t blkid);


/****************************************************************  tagging  ***/

int tags_valid(const char *tags);
int tags_canonicalize(char *tags);
char * tags_next(char *tags, char **tag, char **val);


/***************************************************  bucket consolidation  ***/

#define CF_MIN     1
#define CF_MAX     2
#define CF_SUM     3
#define CF_MEAN    4
#define CF_MEDIAN  5
#define CF_STDEV   6
#define CF_VAR     7
#define CF_DELTA   8

struct cf {
	int    type;   /* what type of consolidation is this? (one of CF_*) */
	size_t slots;  /* how many slots total are in rsv[] */
	size_t used;   /* how many slots are in used in rsv[] */
	size_t i;      /* what slot should the next sample go into */
	size_t n;      /* how many samples have been seen total */
	double carry;  /* a value carried across resets (for delta) */
	int    active; /* do we care what's in carry? (1 = yes) */
	double rsv[];  /* reservoir sample for lossy consolidation */
};

struct cf *cf_new(int type, size_t n);
void cf_free(struct cf *cf);

void cf_reset(struct cf *cf);
void cf_sample(struct cf *cf, double v);
double cf_value(struct cf *cf);


/*****************************************************  ingestion protocol  ***/

#ifndef INGEST_BUF_SIZE
#define INGEST_BUF_SIZE 8192
#endif

struct ingestor {
	int     fd;
	int     eof;
	char    buf[INGEST_BUF_SIZE];
	size_t  len;
	char   *last;

	char        *metric;
	char        *tags;
	bolo_msec_t  time;
	bolo_value_t value;
};

int ingest_eof(struct ingestor *);
int ingest_read(struct ingestor *);
int ingest(struct ingestor *);

/*********************************************************  query language  ***/

#define COND_EQ    1
#define COND_AND   2
#define COND_OR    3
#define COND_NOT   4
#define COND_EXIST 5

struct qcond {
	int op;
	void *a;
	void *b;
	struct multidx *midx;
};


struct result {
	bolo_msec_t start;
	bolo_msec_t finish;
	double      value;
};

struct resultset {
	size_t        len;       /* how many (ts,value) tuples are there? */
	struct result results[]; /* list of `len` result (ts,value) tuples */
};

/* NOTE: it is vital to the correctness of the implementation
         that the *C math operators are exactly one greater
         than their metric-only counterparts. */
#define QOP_PUSH  1
#define QOP_ADD   2
#define QOP_ADDC  3
#define QOP_SUB   4
#define QOP_SUBC  5
#define QOP_MUL   6
#define QOP_MULC  7
#define QOP_DIV   8
#define QOP_DIVC  9
#define QOP_AGGR  10
#define QOP_RETURN 100

struct qop {
	int code;
	union {
		double  imm;
		struct {
			int               raw;
			char             *metric;
			struct resultset *rset;
			struct multidx   *set;
		} push;
		struct {
			int cf;
		} aggr;
	} data;
};

#define QERR_NOSUCHREF   1
#define QERR_MISSINGCOND 2
#define QERR__TOP        2

struct qfield {
	struct qfield    *next;

	char             *name;
	struct qop       *ops;

	struct resultset *result;
};

struct bucket {
	int samples;
	int stride;
	int cf;
};

struct query {
	struct qfield *select;
	struct qcond  *where;
	int            from;
	int            until;

	struct bucket bucket;
	struct bucket aggr;

	int   err_num;
	char *err_data;
};

struct query_ctx {
	bolo_msec_t now;
};

struct query * bql_parse(const char *q);
struct query * query_parse(const char *q);
void query_free(struct query *q);

int query_plan(struct query *q, struct db *db);
int query_exec(struct query *q, struct db *db, struct query_ctx *ctx);

const char * query_strerror(struct query *q);


#endif

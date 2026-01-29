/*
 * Splinter unit tests, inspired by TAP (just made lightweight and dynamic)
 * There are many backwards-compatibility hacks for older loggers that have
 * on-board clock failure issues and extremely sparse FAT16 implementations.
 * 
 * I don't generally number them, I just kind of herd them into groups that
 * make the most sense. What matters is they get written :)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/limits.h>
#include <time.h>
#include "splinter.h"
#include "config.h"
#include <fcntl.h>
#include <stdalign.h>

#ifdef HAVE_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

#ifndef TIME_T_MAX
#define TIME_T_MAX 0x00
#endif

/* 
 * Returns true on success, false if the value cannot be represented. 
 * Older data loggers can wake up pre-1970 on battery changes, so we
 * need to not overflow signedness.
 */

static bool time_to_unsigned_long(time_t src, unsigned long *dst)
{
    /* Reject negative timestamps. */
    if (src < 0) {
        return false;
    }

    /* Ensure the destination type is large enough.
       This works at compile time for the common cases,
       and falls back to a run‑time check when necessary. */
#if ULONG_MAX >= TIME_T_MAX
    /* On platforms where unsigned long can already hold any time_t value,
       we can just cast. */
    (void)TIME_T_MAX;               // silence unused‑macro warning
#else
    /* Otherwise we need a run‑time guard. */
    if ((unsigned long)src > ULONG_MAX) {
        return false; // overflow would occur
    }
#endif
    *dst = (unsigned long)src;
    return true;
}

/* test statistics */
static int total = 0;
static int passed = 0;

/* this is used to make temporary stores / keys */
static pid_t pid = 0;

#define TEST(name, expr) do { \
  total++; \
  if (expr) { \
    passed++; \
    printf("ok %d - %s\n", total, name); \
  } else { \
    printf("not ok %d - %s\n", total, name); \
  } \
} while (0)

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int main(void) {
  char bus[16] = { 0 };
  char buspath[PATH_MAX] = { 0 };
  pid = getpid();

  snprintf(bus, 16, "%d-tap-test", pid);
  TEST("splinter slot 64 byte alignment check", (alignof(struct splinter_slot) == 64));
  TEST("create splinter store", splinter_create_or_open(bus, 1000, 4096) == 0);

  const char *test_key = "test_key";
  const char *test_value = "hello world";
  TEST("set key-value pair", splinter_set(test_key, test_value, strlen(test_value)) == 0);

  char buf[256];
  size_t out_sz;
  TEST("get key-value pair", splinter_get(test_key, buf, sizeof(buf), &out_sz) == 0);

  buf[out_sz] = '\0'; 
  TEST("retrieved value matches", strcmp(buf, test_value) == 0);
  TEST("retrieved size is correct", out_sz == strlen(test_value));

  size_t query_sz;
  TEST("query size with NULL buffer", splinter_get(test_key, NULL, 0, &query_sz) == 0);
  TEST("queried size matches", query_sz == strlen(test_value));

  const char *new_value = "updated value";
  TEST("update existing key", splinter_set(test_key, new_value, strlen(new_value)) == 0);
  TEST("get updated value", splinter_get(test_key, buf, sizeof(buf), &out_sz) == 0);
  buf[out_sz] = '\0';
  TEST("updated value is correct", strcmp(buf, new_value) == 0);
  TEST("set second key", splinter_set("key2", "value2", 6) == 0);
  TEST("set third key", splinter_set("key3", "value3", 6) == 0);

  char *key_list[10];
  size_t key_count;
  TEST("list keys", splinter_list(key_list, 10, &key_count) == 0);
  TEST("correct number of keys", key_count == 3);
  TEST("unset key", splinter_unset("key2") >= 0);

  int original_av = splinter_get_av();
  TEST("set auto scrubbing mode", splinter_set_av(0) == 0);
  TEST("get auto scrubbing mode", splinter_get_av() == 0);
  splinter_set_av((uint32_t) original_av);

  splinter_header_snapshot_t snap = { 0 };
  TEST("get header snapshot", splinter_get_header_snapshot(&snap) == 0);
  TEST("magic number greater than zero", snap.magic > 0);
  TEST("epoch greater than zero", snap.epoch > 0);
  TEST("auto_scrubbing is really off", (snap.core_flags & SPL_SYS_AUTO_SCRUB) == 0 ? 1 : 0);
  TEST("slots are non-zero", snap.slots > 0);
  
  splinter_slot_snapshot_t snap1 = { 0 };
  TEST("create header snapshot key", splinter_set("header_snap", "hello", 5) == 0);
  TEST("take snapshot of header_snap slot metadata", splinter_get_slot_snapshot("header_snap", &snap1) == 0);
  TEST("snap1 epoch is nonzero", snap1.epoch > 0);
  TEST("length of header_snap is 5: h e l l o", snap1.val_len == 5);

  splinter_slot_snapshot_t snap2 = { 0 };
  TEST("name slot as text", splinter_set_named_type("header_snap", SPL_SLOT_TYPE_VARTEXT) == 0);
  TEST("re-acquire snapshot to test named type", splinter_get_slot_snapshot("header_snap", &snap2) ==0);
  TEST("ensure header_snap is SPL_SLOT_TYPE_VARTEXT", (snap2.type_flag & SPL_SLOT_TYPE_VARTEXT) != 0);
  TEST("ensure header_snap is not also SPL_SLOT_TYPE_JSON", (snap2.type_flag & SPL_SLOT_TYPE_JSON) == 0);

  time_t curtime = time(NULL);
  unsigned long longtime = 0;
  TEST("host can convert time_t to unsigned long and temporal tests can continue", 
    time_to_unsigned_long(curtime, &longtime) == true);
  splinter_slot_snapshot_t snap3 = { 0 };
  TEST("set key creation time", splinter_set_slot_time("header_snap", SPL_TIME_CTIME, curtime, 0) == 0);
  TEST("set key last access time", splinter_set_slot_time("header_snap", SPL_TIME_ATIME, curtime, 0) == 0);
  TEST("rea-acquire snapshot to test timestamps", splinter_get_slot_snapshot("header_snap", &snap3) == 0);
  TEST("snapshot ctime = snapshot curtime", (snap3.ctime == longtime));
  TEST("snapshot atime = snapshot curtime", (snap3.atime == longtime));
  splinter_unset("header_snap");
#ifdef SPLINTER_EMBEDDINGS
  float mock_vec[SPLINTER_EMBED_DIM] = { 0 };
  for (int i = 0; i < SPLINTER_EMBED_DIM; i++) {
    mock_vec[i] = (float)i * 0.1f; // Linear mock values
  }
  TEST("set 768-dim embedding", splinter_set_embedding(test_key, mock_vec) == 0);

  float read_vec[SPLINTER_EMBED_DIM] = { 0 };
  TEST("get 768-dim embedding", splinter_get_embedding(test_key, read_vec) == 0);

  int vec_match = 1;
  for (int i = 0; i < SPLINTER_EMBED_DIM; i++) {
    if (read_vec[i] != mock_vec[i]) {
      vec_match = 0;
      break;
    }
  }
  TEST("embedding vector data matches exactly", vec_match == 1);
  splinter_slot_snapshot_t embed_snap = { 0 };
  TEST("get slot snapshot with embedding", splinter_get_slot_snapshot(test_key, &embed_snap) == 0);
  TEST("snapshot embedding encapsulation check", 
       embed_snap.embedding[0] == mock_vec[0] && 
       embed_snap.embedding[SPLINTER_EMBED_DIM-1] == mock_vec[SPLINTER_EMBED_DIM-1]);
#endif // SPLINTER_EMBEDDINGS

splinter_close();
splinter_header_snapshot_t closed = { 0 };
TEST("store actually closed", splinter_get_header_snapshot(&closed) != 0);

#ifndef SPLINTER_PERSISTENT
  snprintf(buspath, sizeof(buspath) -1, "/dev/shm/%s", bus);
#else
  snprintf(buspath, sizeof(buspath) -1, "./%s", bus);
#endif /* SPLINTER_PERSISTENT */
  unlink(buspath);

#ifdef HAVE_VALGRIND_H
  if (RUNNING_ON_VALGRIND) {
    printf("\n** Valgrind Detected. Thank you for your diligence! **\n\n");
    if (VALGRIND_COUNT_ERRORS) {
      fprintf(stderr,"\nValgrind detected errors in this run. Exiting abnormally.\n");
      fprintf(stderr, "(sad trombone sound)\n");
      return 1;
    }
  }
#endif // HAVE_VALGRIND_H
  
  return (passed == total) ? 0 : 1;
}

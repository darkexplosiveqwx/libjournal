// SPDX-License-Identifier: BSD-3-Clause

#include "encode.h"
#include "journal.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

static int n_pass = 0;
static int n_fail = 0;
static int n_skip = 0;
static int has_journal = 0;

#define SKIP_JOURNAL()                                                                             \
	do                                                                                             \
	{                                                                                              \
		if (!has_journal)                                                                          \
		{                                                                                          \
			printf("  SKIP (no journald)\n");                                                      \
			n_skip++;                                                                              \
			return;                                                                                \
		}                                                                                          \
	} while (0)

#define TEST(name)                                                                                 \
	do                                                                                             \
	{                                                                                              \
		printf("  %s ... ", name);                                                                 \
		fflush(stdout);                                                                            \
	} while (0)

#define PASS()                                                                                     \
	do                                                                                             \
	{                                                                                              \
		printf("PASS\n");                                                                          \
		n_pass++;                                                                                  \
	} while (0)

#define FAIL(msg)                                                                                  \
	do                                                                                             \
	{                                                                                              \
		printf("FAIL: %s\n", msg);                                                                 \
		n_fail++;                                                                                  \
	} while (0)

#define ASSERT(cond)                                                                               \
	do                                                                                             \
	{                                                                                              \
		if (!(cond))                                                                               \
		{                                                                                          \
			FAIL(#cond);                                                                           \
			return;                                                                                \
		}                                                                                          \
	} while (0)

#define ASSERT_EQ(a, b)                                                                            \
	do                                                                                             \
	{                                                                                              \
		if ((a) != (b))                                                                            \
		{                                                                                          \
			char _buf[256];                                                                        \
			snprintf(_buf, sizeof(_buf), "%s == %s: got %d, expected %d", #a, #b, (int)(a),        \
					 (int)(b));                                                                    \
			FAIL(_buf);                                                                            \
			return;                                                                                \
		}                                                                                          \
	} while (0)

#define ASSERT_STREQ(a, b)                                                                         \
	do                                                                                             \
	{                                                                                              \
		if (strcmp((a), (b)) != 0)                                                                 \
		{                                                                                          \
			char _buf[512];                                                                        \
			snprintf(_buf, sizeof(_buf), "%s == %s: got \"%s\", expected \"%s\"", #a, #b, (a),     \
					 (b));                                                                         \
			FAIL(_buf);                                                                            \
			return;                                                                                \
		}                                                                                          \
	} while (0)

/* --- encode_validate_key tests --- */

static void test_validate_key_accept(void)
{
	ASSERT_EQ(encode_validate_key("MESSAGE", 7), 0);
	ASSERT_EQ(encode_validate_key("SYSLOG_IDENTIFIER", 17), 0);
	ASSERT_EQ(encode_validate_key("FOO_123", 7), 0);
	PASS();
}

static void test_validate_key_reject(void)
{
	ASSERT_EQ(encode_validate_key("_", 1), -EINVAL);
	ASSERT_EQ(encode_validate_key("BAD=KEY", 7), -EINVAL);
	ASSERT_EQ(encode_validate_key("BAD\nKEY", 7), -EINVAL);
	ASSERT_EQ(encode_validate_key("BAD KEY", 7), -EINVAL);
	ASSERT_EQ(encode_validate_key("bad", 3), -EINVAL);
	ASSERT_EQ(encode_validate_key("123", 3), 0); /* digits are valid */
	ASSERT_EQ(encode_validate_key("", 0), -EINVAL);
	ASSERT_EQ(encode_validate_key(NULL, 5), -EINVAL);
	PASS();
}

static void test_validate_key_trusted(void)
{
	ASSERT_EQ(encode_validate_key("_PID", 4), -EINVAL);
	ASSERT_EQ(encode_validate_key("_UID", 4), -EINVAL);
	ASSERT_EQ(encode_validate_key("_SYSTEMD_UNIT", 13), -EINVAL);
	PASS();
}

/* --- encode_needs_binary tests --- */

static void test_needs_binary(void)
{
	ASSERT_EQ(encode_needs_binary("hello", 5), 0);
	ASSERT_EQ(encode_needs_binary("hello\nworld", 11), 1);
	ASSERT_EQ(encode_needs_binary("hello\0world", 11), 1);
	ASSERT_EQ(encode_needs_binary("", 0), 0);
	ASSERT_EQ(encode_needs_binary(NULL, 0), 0);
	ASSERT_EQ(encode_needs_binary("line1\nline2\n", 12), 1);
	PASS();
}

/* --- encode_trim_trailing_whitespace tests --- */

static void test_trim_whitespace(void)
{
	char buf[64];

	strcpy(buf, "hello");
	ASSERT_EQ(encode_trim_trailing_whitespace(buf, 5), 5);
	ASSERT_STREQ(buf, "hello");

	strcpy(buf, "hello   ");
	ASSERT_EQ(encode_trim_trailing_whitespace(buf, 8), 5);
	ASSERT_STREQ(buf, "hello");

	strcpy(buf, "hello\n\n");
	ASSERT_EQ(encode_trim_trailing_whitespace(buf, 7), 5);
	ASSERT_STREQ(buf, "hello");

	strcpy(buf, "hello\t\r\n");
	ASSERT_EQ(encode_trim_trailing_whitespace(buf, 8), 5);
	ASSERT_STREQ(buf, "hello");

	strcpy(buf, "  hello");
	ASSERT_EQ(encode_trim_trailing_whitespace(buf, 7), 7);
	ASSERT_STREQ(buf, "  hello");

	strcpy(buf, "");
	ASSERT_EQ(encode_trim_trailing_whitespace(buf, 0), 0);

	ASSERT_EQ(encode_trim_trailing_whitespace(NULL, 5), 5);
	PASS();
}

/* --- encode_text tests --- */

static void test_encode_text(void)
{
	char buf[128];
	int len;

	len = encode_text(buf, sizeof(buf), "MESSAGE", 7, "hello", 5);
	ASSERT_EQ(len, 14);
	ASSERT_EQ(buf[13], '\n');
	buf[13] = '\0';
	ASSERT_STREQ(buf, "MESSAGE=hello");

	len = encode_text(buf, sizeof(buf), "PRIORITY", 8, "6", 1);
	ASSERT_EQ(len, 11);
	buf[10] = '\0';
	ASSERT_STREQ(buf, "PRIORITY=6");

	len = encode_text(buf, sizeof(buf), "X", 1, "", 0);
	ASSERT_EQ(len, 3);
	ASSERT_EQ(buf[0], 'X');
	ASSERT_EQ(buf[1], '=');
	ASSERT_EQ(buf[2], '\n');

	ASSERT_EQ(encode_text(buf, 3, "K", 1, "v", 1), -ENOSPC);
	ASSERT_EQ(encode_text(NULL, 10, "K", 1, "v", 1), -EINVAL);
	ASSERT_EQ(encode_text(buf, 10, NULL, 1, "v", 1), -EINVAL);
	PASS();
}

/* --- encode_binary tests --- */

static void test_encode_binary(void)
{
	struct iovec iov[8];
	int idx = 0;
	unsigned char le_buf[8];
	char key_buf[16];

	int r = encode_binary(iov, 8, &idx, "MYFIELD", 7, "hello", 5, key_buf, le_buf);
	ASSERT_EQ(r, 0);
	ASSERT_EQ(idx, 4);

	ASSERT_EQ(iov[0].iov_len, 8);
	ASSERT_EQ(memcmp(iov[0].iov_base, "MYFIELD\n", 8), 0);

	ASSERT_EQ(iov[1].iov_len, 8);
	{
		uint64_t le_val;
		memcpy(&le_val, iov[1].iov_base, 8);
		ASSERT_EQ(le_val, 5);
	}

	ASSERT_EQ(iov[2].iov_len, 5);
	ASSERT_EQ(memcmp(iov[2].iov_base, "hello", 5), 0);

	ASSERT_EQ(iov[3].iov_len, 1);
	ASSERT_EQ(*(char *)iov[3].iov_base, '\n');
	PASS();
}

static void test_encode_binary_empty(void)
{
	struct iovec iov[8];
	int idx = 0;
	unsigned char le_buf[8];
	char key_buf[16];

	int r = encode_binary(iov, 8, &idx, "F", 1, "", 0, key_buf, le_buf);
	ASSERT_EQ(r, 0);
	ASSERT_EQ(idx, 4);
	ASSERT_EQ(iov[2].iov_len, 0);
	PASS();
}

static void test_encode_binary_too_small(void)
{
	struct iovec iov[2];
	int idx = 0;
	unsigned char le_buf[8];
	char key_buf[16];

	int r = encode_binary(iov, 2, &idx, "F", 1, "hello", 5, key_buf, le_buf);
	ASSERT_EQ(r, -ENOSPC);
	PASS();
}

/* --- journal_send validation tests --- */

static void test_send_bad_key(void)
{
	int r = journal_send("BAD KEY=value", NULL);
	ASSERT_EQ(r, -EINVAL);

	r = journal_send("bad=value", NULL);
	ASSERT_EQ(r, -EINVAL);

	r = journal_send("_PID=123", NULL);
	ASSERT_EQ(r, -EINVAL);

	r = journal_send("=value", NULL);
	ASSERT_EQ(r, -EINVAL);
	PASS();
}

static void test_send_empty(void)
{
	SKIP_JOURNAL();
	ASSERT_EQ(journal_send("MESSAGE=empty", NULL), 0);
	PASS();
}

/* --- journal_send whitespace trimming --- */

static void test_send_trim_value(void)
{
	SKIP_JOURNAL();
	int r = journal_send("MESSAGE=hello   \n\n", NULL);
	ASSERT_EQ(r, 0);

	r = journal_send("MESSAGE=%s", "hello   \n\n", NULL);
	ASSERT_EQ(r, 0);
	PASS();
}

/* --- journal_print tests --- */

static void test_print_empty(void)
{
	SKIP_JOURNAL();
	ASSERT_EQ(journal_print(LOG_INFO, "test"), 0);
	ASSERT_EQ(journal_print(LOG_INFO, NULL), -EINVAL);
	PASS();
}

/* --- journal_sendv tests --- */

static void test_sendv_basic(void)
{
	SKIP_JOURNAL();
	struct iovec iov[2];
	char f1[] = "MESSAGE=hello\n";
	char f2[] = "PRIORITY=6\n";
	iov[0].iov_base = f1;
	iov[0].iov_len = sizeof(f1) - 1;
	iov[1].iov_base = f2;
	iov[1].iov_len = sizeof(f2) - 1;
	ASSERT_EQ(journal_sendv(iov, 2), 0);
	ASSERT_EQ(journal_sendv(NULL, 0), 0);
	ASSERT_EQ(journal_sendv(NULL, -1), -EINVAL);
	PASS();
}

/* --- binary field via journal_sendv --- */

static void test_sendv_binary(void)
{
	SKIP_JOURNAL();
	struct iovec iov[8];
	unsigned char le_buf[8];
	char key_buf[16];
	int idx = 0;

	const char *data = "hello\nworld";
	int r = encode_binary(iov, 8, &idx, "BINARY", 6, data, 11, key_buf, le_buf);
	ASSERT_EQ(r, 0);
	ASSERT_EQ(idx, 4);

	ASSERT_EQ(journal_sendv(iov, idx), 0);
	PASS();
}

/* --- large message via journal_sendv (exercises memfd path) --- */

static void test_sendv_large(void)
{
	SKIP_JOURNAL();
	size_t sz = 200 * 1024;
	char *buf = malloc(8 + sz + 1);
	ASSERT(buf != NULL);
	memcpy(buf, "MESSAGE=", 8);
	memcpy(buf + 8, "LARGE_MEMFD_PAYLOAD:", 20);
	for (size_t i = 20; i < sz; i++)
		buf[8 + i] = 'A';
	buf[8 + sz] = '\n';
	const char *prio = "PRIORITY=6\n";
	struct iovec iov[2];
	iov[0].iov_base = buf;
	iov[0].iov_len = 8 + sz + 1;
	iov[1].iov_base = (void *)prio;
	iov[1].iov_len = strlen(prio);
	ASSERT_EQ(journal_sendv(iov, 2), 0);
	free(buf);
	PASS();
}

/* --- util_write_le64 --- */

static void test_write_le64(void)
{
	unsigned char buf[8];
	util_write_le64(buf, 0x0102030405060708ULL);
	ASSERT_EQ(buf[0], 0x08);
	ASSERT_EQ(buf[1], 0x07);
	ASSERT_EQ(buf[2], 0x06);
	ASSERT_EQ(buf[3], 0x05);
	ASSERT_EQ(buf[4], 0x04);
	ASSERT_EQ(buf[5], 0x03);
	ASSERT_EQ(buf[6], 0x02);
	ASSERT_EQ(buf[7], 0x01);
	PASS();
}

/* --- util_count_printf_conversions --- */

static void test_count_conversions(void)
{
	ASSERT_EQ(util_count_printf_conversions("hello"), 0);
	ASSERT_EQ(util_count_printf_conversions("%s"), 1);
	ASSERT_EQ(util_count_printf_conversions("%d %s"), 2);
	ASSERT_EQ(util_count_printf_conversions("100%%"), 0);
	ASSERT_EQ(util_count_printf_conversions("%%s"), 0);
	ASSERT_EQ(util_count_printf_conversions("%%%s"), 1);
	ASSERT_EQ(util_count_printf_conversions("%*d"), 2);
	ASSERT_EQ(util_count_printf_conversions("%.5f"), 1);
	ASSERT_EQ(util_count_printf_conversions("%s=%d"), 2);
	ASSERT_EQ(util_count_printf_conversions("%lld"), 1);
	ASSERT_EQ(util_count_printf_conversions("%Lf"), 1);
	ASSERT_EQ(util_count_printf_conversions(NULL), 0);
	PASS();
}

/* --- journal_send with multiple fields --- */

static void test_send_multi(void)
{
	SKIP_JOURNAL();
	int r = journal_send("MESSAGE=test", "PRIORITY=%i", LOG_INFO, "USER=testuser", NULL);
	ASSERT_EQ(r, 0);
	PASS();
}

/* --- journal_init / close --- */

static void test_init_close(void)
{
	SKIP_JOURNAL();
	journal_close();
	ASSERT_EQ(journal_init(), 0);
	ASSERT_EQ(journal_init(), 0);
	ASSERT_EQ(journal_get_fd() >= -1, 1);
	journal_close();
	PASS();
}

/* --- binary value auto-detection in journal_send --- */

static void test_send_auto_binary(void)
{
	SKIP_JOURNAL();
	int r = journal_send("MESSAGE=hello\nworld", "PRIORITY=6", NULL);
	ASSERT_EQ(r, 0);

	r = journal_send("MESSAGE=hello\x01world", "PRIORITY=6", NULL);
	ASSERT_EQ(r, 0);
	PASS();
}

/* --- integration: run demo and verify journal fields --- */

static void test_demo_integration(void)
{
	SKIP_JOURNAL();

	char since[64];
	FILE *p;

	/* 1. Get current time with nanosecond precision */
	p = popen("date '+%Y-%m-%d %H:%M:%S.%N'", "r");
	ASSERT(p != NULL);
	ASSERT(fgets(since, sizeof(since), p) != NULL);
	pclose(p);
	since[strcspn(since, "\n")] = '\0';

	/* 2. Run the demo */
	int r = system(DEMO_BIN " >/dev/null 2>&1");
	ASSERT_EQ(r, 0);

	/* 3. Query journalctl --output=export gives one KEY=value per line,
	 *    no JSON escaping and no field size truncation. */
	char cmd[512];
	snprintf(cmd, sizeof(cmd),
			 "journalctl --boot --output=export _COMM=journal-demo --since='%s'",
			 since);
	p = popen(cmd, "r");
	ASSERT(p != NULL);

	/*
	 * export format: each field is "KEY=value\n", entries separated by "\n".
	 * The large MESSAGE field may exceed our buffer; we only need to match
	 * the prefix "LARGE_MEMFD_PAYLOAD" which appears within the first few
	 * bytes of the value on its own line, so a modest buffer suffices.
	 */
	char line[4096];
	int found_hello = 0;
	int found_structured = 0;
	int found_iovec = 0;
	int found_priority_6 = 0;
	int found_priority_7 = 0;
	int found_user_test = 0;
	int found_large = 0;

	while (fgets(line, sizeof(line), p))
	{
		if (strstr(line, "MESSAGE=Hello world"))
			found_hello = 1;
		if (strstr(line, "MESSAGE=Structured message"))
			found_structured = 1;
		if (strstr(line, "MESSAGE=Manual iovec field"))
			found_iovec = 1;
		if (strstr(line, "PRIORITY=6"))
			found_priority_6 = 1;
		if (strstr(line, "PRIORITY=7"))
			found_priority_7 = 1;
		if (strstr(line, "USER=test"))
			found_user_test = 1;
		if (strstr(line, "MESSAGE=LARGE_MEMFD_PAYLOAD:AAAAA"))
			found_large = 1;
	}
	pclose(p);

	ASSERT(found_hello);
	ASSERT(found_structured);
	ASSERT(found_iovec);
	ASSERT(found_priority_6);
	ASSERT(found_priority_7);
	ASSERT(found_user_test);
	ASSERT(found_large);
	PASS();
}

/* --- main --- */

int main(void)
{
	printf("=== probing journald ===\n");
	has_journal = (journal_init() >= 0);
	if (!has_journal)
	{
		/* Keep going; transport tests will be skipped */
		journal_close();
	}

	printf("=== encode_validate_key ===\n");
	TEST("accept valid keys");
	test_validate_key_accept();
	TEST("reject invalid keys");
	test_validate_key_reject();
	TEST("reject trusted keys");
	test_validate_key_trusted();

	printf("\n=== encode_needs_binary ===\n");
	TEST("detect binary");
	test_needs_binary();

	printf("\n=== encode_trim_trailing_whitespace ===\n");
	TEST("trim whitespace");
	test_trim_whitespace();

	printf("\n=== encode_text ===\n");
	TEST("encode text fields");
	test_encode_text();

	printf("\n=== encode_binary ===\n");
	TEST("encode binary fields");
	test_encode_binary();
	TEST("empty binary field");
	test_encode_binary_empty();
	TEST("iov too small");
	test_encode_binary_too_small();

	printf("\n=== util ===\n");
	TEST("write_le64");
	test_write_le64();
	TEST("count conversions");
	test_count_conversions();

	printf("\n=== journal_send ===\n");
	TEST("bad keys ignored");
	test_send_bad_key();
	TEST("NULL format");
	test_send_empty();
	TEST("trailing whitespace");
	test_send_trim_value();
	TEST("multiple fields");
	test_send_multi();
	TEST("auto binary encoding");
	test_send_auto_binary();

	printf("\n=== journal_print ===\n");
	TEST("empty and NULL format");
	test_print_empty();

	printf("\n=== journal_sendv ===\n");
	TEST("basic sendv");
	test_sendv_basic();
	TEST("binary via sendv");
	test_sendv_binary();
	TEST("large message via sendv");
	test_sendv_large();

	printf("\n=== init/close ===\n");
	TEST("init and close");
	test_init_close();

	printf("\n=== integration ===\n");
	TEST("demo integration");
	test_demo_integration();

	printf("\n=== results ===\n");
	printf("  PASS: %d\n", n_pass);
	printf("  SKIP: %d\n", n_skip);
	printf("  FAIL: %d\n", n_fail);

	journal_close();
	return n_fail > 0 ? 1 : 0;
}

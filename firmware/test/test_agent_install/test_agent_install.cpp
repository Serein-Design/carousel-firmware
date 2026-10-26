// SPDX-License-Identifier: AGPL-3.0-only
// The installer the panel serves: the prelude that carries the token, the
// address and the name, refused for anything outside its characters; the
// refusal answers, which curl pipes into sh too; and the gzip stream around
// them and the body, whose last line is the only one that runs anything.
//
//   pio test -e native -f test_agent_install
#include <unity.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

#include "../../src/agent_install.cpp"
#include "../../src/agent_scripts.cpp"

void setUp() {}
void tearDown() {}

static const char *TOKEN = "0123456789abcdef0123456789abcdef";   // a fake token
static const char *PRELUDE =
    "#!/bin/sh\n"
    "CAROUSEL_TOKEN='0123456789abcdef0123456789abcdef'\n"
    "CAROUSEL_IP='192.168.1.20'\n"
    "CAROUSEL_NAME='carousel-695c'\n";

static int prelude(const char *token, const char *ip, const char *name) {
  char out[INSTALL_PRELUDE_SIZE];
  return installPrelude(out, sizeof(out), token, ip, name);
}

static void test_prelude_bytes() {
  char out[INSTALL_PRELUDE_SIZE];
  memset(out, 'x', sizeof(out));
  const int n = installPrelude(out, sizeof(out), TOKEN, "192.168.1.20", "carousel-695c");
  TEST_ASSERT_EQUAL_INT((int)strlen(PRELUDE), n);
  TEST_ASSERT_EQUAL_STRING(PRELUDE, out);
}

static void test_prelude_fits_the_longest_address() {
  TEST_ASSERT_GREATER_THAN(0, prelude(TOKEN, "255.255.255.255", "carousel-ffff"));
}

static void test_prelude_refuses_a_bad_token() {
  TEST_ASSERT_EQUAL_INT(-1, prelude("0123456789abcdef0123456789abcde", "192.168.1.20", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude("0123456789abcdef0123456789abcdef0", "192.168.1.20", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude("0123456789abcdeg0123456789abcdef", "192.168.1.20", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude("0123456789abcdeF0123456789abcdef", "192.168.1.20", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude("0123456789abcde'0123456789abcdef", "192.168.1.20", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude("", "192.168.1.20", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(nullptr, "192.168.1.20", "carousel-695c"));
}

static void test_prelude_refuses_a_bad_address() {
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "192.168.1.20'", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "192.168.1.20; rm", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "fe80::1", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, nullptr, "carousel-695c"));
}

static void test_prelude_refuses_a_bad_name() {
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "192.168.1.20", ""));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "192.168.1.20", "Carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "192.168.1.20", "carousel'695c"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "192.168.1.20", "../x"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "192.168.1.20", "carousel.local"));
  TEST_ASSERT_EQUAL_INT(-1, prelude(TOKEN, "192.168.1.20", nullptr));
}

static void test_prelude_one_byte_short_refused() {
  char out[INSTALL_PRELUDE_SIZE];
  const size_t need = strlen(PRELUDE) + 1;   // with its terminating zero
  TEST_ASSERT_EQUAL_INT(-1, installPrelude(out, need - 1, TOKEN, "192.168.1.20", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT((int)need - 1, installPrelude(out, need, TOKEN, "192.168.1.20", "carousel-695c"));
  TEST_ASSERT_EQUAL_INT(-1, installPrelude(nullptr, 0, TOKEN, "192.168.1.20", "carousel-695c"));
}

// 32 hex characters in a row: what a token looks like
static bool holdsHexRun(const char *s) {
  int run = 0;
  for (; *s; s++) {
    const bool hex = (*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F');
    run = hex ? run + 1 : 0;
    if (run >= 32) return true;
  }
  return false;
}

static void checkRefusal(InstallRefusal why, const char *says) {
  const std::string s = installRefusalScript(why);
  TEST_ASSERT_EQUAL_INT(0, s.rfind("#!/bin/sh\n", 0));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, s.find(">&2"));
  TEST_ASSERT_NOT_EQUAL(std::string::npos, s.find(says));
  TEST_ASSERT_TRUE(s.size() >= 7 && s.compare(s.size() - 7, 7, "exit 1\n") == 0);
  TEST_ASSERT_EQUAL(std::string::npos, s.find("CAROUSEL_TOKEN"));
  TEST_ASSERT_FALSE(holdsHexRun(s.c_str()));
}

static void test_bad_link_script() {
  checkRefusal(INSTALL_BAD_LINK,
               "this install link is not valid any more, copy the command again from the Panel page");
}

static void test_locked_script() { checkRefusal(INSTALL_LOCKED, "factory reset"); }

// The served stream is gzip, made of the head, the prelude, the body's
// deflate bytes from flash and the tail: gzip itself checks its CRC and length
static void test_gzip_head_bytes() {
  uint8_t head[INSTALL_GZIP_HEAD_SIZE];
  installGzipHead(head, 0x0102);
  const uint8_t want[] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff,   // gzip, deflate, no name
                          0x00, 0x02, 0x01, 0xfd, 0xfe};           // a stored block, not the last
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, head, sizeof(want));
}

static void test_gzip_tail_vector() {
  uint8_t tail[INSTALL_GZIP_TAIL_SIZE];
  installGzipTail(tail, "abc", 3, 0x0cc4e161 /* CRC-32 of "def" */, 3);
  const uint8_t want[] = {0xef, 0x39, 0x8e, 0x4b /* CRC-32 of "abcdef" */, 6, 0, 0, 0};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, tail, sizeof(want));
}

static void test_served_stream_inflates_to_prelude_and_body() {
  char pre[INSTALL_PRELUDE_SIZE];
  const int n = installPrelude(pre, sizeof(pre), TOKEN, "192.168.1.20", "carousel-695c");
  TEST_ASSERT_TRUE(n > 0);
  uint8_t head[INSTALL_GZIP_HEAD_SIZE], tail[INSTALL_GZIP_TAIL_SIZE];
  installGzipHead(head, n);
  installGzipTail(tail, pre, n, INSTALL_SH_CRC, INSTALL_SH_LEN);
  char path[] = "/tmp/carousel-install-XXXXXX";
  const int fd = mkstemp(path);
  TEST_ASSERT_TRUE(fd >= 0);
  FILE *f = fdopen(fd, "wb");
  fwrite(head, 1, sizeof(head), f);
  fwrite(pre, 1, n, f);
  fwrite(INSTALL_SH_GZ, 1, INSTALL_SH_GZ_LEN, f);
  fwrite(tail, 1, sizeof(tail), f);
  fclose(f);
  const std::string cmd = std::string("gzip -dc < ") + path;
  FILE *p = popen(cmd.c_str(), "r");
  TEST_ASSERT_NOT_NULL(p);
  std::string out;
  char buf[4096];
  for (size_t got; (got = fread(buf, 1, sizeof(buf), p)) > 0;) out.append(buf, got);
  const int status = pclose(p);
  unlink(path);
  TEST_ASSERT_EQUAL_INT(0, status);   // CRC and length right, nothing after the stream
  TEST_ASSERT_TRUE(INSTALL_SH_GZ_LEN <= 16384);
  TEST_ASSERT_EQUAL_size_t(n + INSTALL_SH_LEN, out.size());
  TEST_ASSERT_EQUAL_STRING(PRELUDE, out.substr(0, n).c_str());
  const std::string body = out.substr(n);
  const std::string last = "\nmain \"$@\"\n";
  TEST_ASSERT_TRUE(body.size() > last.size() &&
                   body.compare(body.size() - last.size(), last.size(), last) == 0);
  // the prelude carries the token: the body never does
  TEST_ASSERT_EQUAL(std::string::npos, body.find("CAROUSEL_TOKEN='"));
  TEST_ASSERT_FALSE(holdsHexRun(body.c_str()));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_prelude_bytes);
  RUN_TEST(test_prelude_fits_the_longest_address);
  RUN_TEST(test_prelude_refuses_a_bad_token);
  RUN_TEST(test_prelude_refuses_a_bad_address);
  RUN_TEST(test_prelude_refuses_a_bad_name);
  RUN_TEST(test_prelude_one_byte_short_refused);
  RUN_TEST(test_bad_link_script);
  RUN_TEST(test_locked_script);
  RUN_TEST(test_gzip_head_bytes);
  RUN_TEST(test_gzip_tail_vector);
  RUN_TEST(test_served_stream_inflates_to_prelude_and_body);
  return UNITY_END();
}

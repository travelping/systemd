/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
  Copyright 2013 Thomas H.P. Andersen

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <string.h>

#include "util.h"

static void test_streq_ptr(void) {
        assert_se(streq_ptr(NULL, NULL));
        assert_se(!streq_ptr("abc", "cdef"));
}

static void test_first_word(void) {
        assert_se(first_word("Hello", ""));
        assert_se(first_word("Hello", "Hello"));
        assert_se(first_word("Hello world", "Hello"));
        assert_se(first_word("Hello\tworld", "Hello"));
        assert_se(first_word("Hello\nworld", "Hello"));
        assert_se(first_word("Hello\rworld", "Hello"));
        assert_se(first_word("Hello ", "Hello"));

        assert_se(!first_word("Hello", "Hellooo"));
        assert_se(!first_word("Hello", "xxxxx"));
        assert_se(!first_word("Hellooo", "Hello"));
}

static void test_parse_boolean(void) {
        assert_se(parse_boolean("1") == 1);
        assert_se(parse_boolean("y") == 1);
        assert_se(parse_boolean("Y") == 1);
        assert_se(parse_boolean("yes") == 1);
        assert_se(parse_boolean("YES") == 1);
        assert_se(parse_boolean("true") == 1);
        assert_se(parse_boolean("TRUE") == 1);
        assert_se(parse_boolean("on") == 1);
        assert_se(parse_boolean("ON") == 1);

        assert_se(parse_boolean("0") == 0);
        assert_se(parse_boolean("n") == 0);
        assert_se(parse_boolean("N") == 0);
        assert_se(parse_boolean("no") == 0);
        assert_se(parse_boolean("NO") == 0);
        assert_se(parse_boolean("false") == 0);
        assert_se(parse_boolean("FALSE") == 0);
        assert_se(parse_boolean("off") == 0);
        assert_se(parse_boolean("OFF") == 0);

        assert_se(parse_boolean("garbage") < 0);
        assert_se(parse_boolean("") < 0);
}

static void test_parse_pid(void) {
        int r;
        pid_t pid;

        r = parse_pid("100", &pid);
        assert_se(r == 0);
        assert_se(pid == 100);

        r = parse_pid("0x7FFFFFFF", &pid);
        assert_se(r == 0);
        assert_se(pid == 2147483647);

        pid = 65; /* pid is left unchanged on ERANGE. Set to known arbitrary value. */
        r = parse_pid("0", &pid);
        assert_se(r == -ERANGE);
        assert_se(pid == 65);

        pid = 65; /* pid is left unchanged on ERANGE. Set to known arbitrary value. */
        r = parse_pid("-100", &pid);
        assert_se(r == -ERANGE);
        assert_se(pid == 65);

        pid = 65; /* pid is left unchanged on ERANGE. Set to known arbitrary value. */
        r = parse_pid("0xFFFFFFFFFFFFFFFFF", &pid);
        assert(r == -ERANGE);
        assert_se(pid == 65);
}

static void test_parse_uid(void) {
        int r;
        uid_t uid;

        r = parse_uid("100", &uid);
        assert_se(r == 0);
        assert_se(uid == 100);
}

static void test_safe_atolli(void) {
        int r;
        long long l;

        r = safe_atolli("12345", &l);
        assert_se(r == 0);
        assert_se(l == 12345);

        r = safe_atolli("junk", &l);
        assert_se(r == -EINVAL);
}

static void test_safe_atod(void) {
        int r;
        double d;

        r = safe_atod("0.2244", &d);
        assert_se(r == 0);
        assert_se(abs(d - 0.2244) < 0.000001);

        r = safe_atod("junk", &d);
        assert_se(r == -EINVAL);
}

static void test_foreach_word_quoted(void) {
        char *w, *state;
        size_t l;
        const char test[] = "test a b c 'd' e '' '' hhh '' ''";
        printf("<%s>\n", test);
        FOREACH_WORD_QUOTED(w, l, test, state) {
                char *t;

                assert_se(t = strndup(w, l));
                printf("<%s>\n", t);
                free(t);
        }
}

static void test_default_term_for_tty(void) {
        puts(default_term_for_tty("/dev/tty23"));
        puts(default_term_for_tty("/dev/ttyS23"));
        puts(default_term_for_tty("/dev/tty0"));
        puts(default_term_for_tty("/dev/pty0"));
        puts(default_term_for_tty("/dev/pts/0"));
        puts(default_term_for_tty("/dev/console"));
        puts(default_term_for_tty("tty23"));
        puts(default_term_for_tty("ttyS23"));
        puts(default_term_for_tty("tty0"));
        puts(default_term_for_tty("pty0"));
        puts(default_term_for_tty("pts/0"));
        puts(default_term_for_tty("console"));
}

static void test_memdup_multiply(void) {
        int org[] = {1, 2, 3};
        int *dup;

        dup = (int*)memdup_multiply(org, sizeof(int), 3);

        assert_se(dup);
        assert_se(dup[0] == 1);
        assert_se(dup[1] == 2);
        assert_se(dup[2] == 3);
        free(dup);
}

int main(int argc, char *argv[]) {
        test_streq_ptr();
        test_first_word();
        test_parse_boolean();
        test_default_term_for_tty();
        test_parse_pid();
        test_parse_uid();
        test_safe_atolli();
        test_safe_atod();
        test_foreach_word_quoted();
        test_memdup_multiply();

        return 0;
}

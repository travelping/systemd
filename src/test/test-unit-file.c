/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2012 Lennart Poettering
  Copyright 2013 Zbigniew Jędrzejewski-Szmek

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

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "install.h"
#include "install-printf.h"
#include "specifier.h"
#include "util.h"
#include "macro.h"
#include "hashmap.h"
#include "load-fragment.h"
#include "strv.h"
#include "fileio.h"

static void test_unit_file_get_set(void) {
        int r;
        Hashmap *h;
        Iterator i;
        UnitFileList *p;

        h = hashmap_new(string_hash_func, string_compare_func);
        assert(h);

        r = unit_file_get_list(UNIT_FILE_SYSTEM, NULL, h);
        log_info("unit_file_get_list: %s", strerror(-r));
        assert(r >= 0);

        HASHMAP_FOREACH(p, h, i)
                printf("%s = %s\n", p->path, unit_file_state_to_string(p->state));

        unit_file_list_free(h);
}

static void check_execcommand(ExecCommand *c,
                              const char* path,
                              const char* argv0,
                              const char* argv1,
                              bool ignore) {
        assert_se(c);
        log_info("%s %s %s %s",
                 c->path, c->argv[0], c->argv[1], c->argv[2]);
        assert_se(streq(c->path, path));
        assert_se(streq(c->argv[0], argv0));
        assert_se(streq(c->argv[1], argv1));
        assert_se(c->argv[2] == NULL);
        assert_se(c->ignore == ignore);
}

static void test_config_parse_exec(void) {
        /* int config_parse_exec( */
        /*         const char *filename, */
        /*         unsigned line, */
        /*         const char *section, */
        /*         const char *lvalue, */
        /*         int ltype, */
        /*         const char *rvalue, */
        /*         void *data, */
        /*         void *userdata) */
        int r;

        ExecCommand *c = NULL, *c1;

        /* basic test */
        r = config_parse_exec("fake", 1, "section",
                              "LValue", 0, "/RValue r1",
                              &c, NULL);
        assert_se(r >= 0);
        check_execcommand(c, "/RValue", "/RValue", "r1", false);

        r = config_parse_exec("fake", 2, "section",
                              "LValue", 0, "/RValue///slashes/// r1",
                              &c, NULL);
       /* test slashes */
        assert_se(r >= 0);
        c1 = c->command_next;
        check_execcommand(c1, "/RValue/slashes", "/RValue///slashes///",
                          "r1", false);

        /* honour_argv0 */
        r = config_parse_exec("fake", 3, "section",
                              "LValue", 0, "@/RValue///slashes2/// argv0 r1",
                              &c, NULL);
        assert_se(r >= 0);
        c1 = c1->command_next;
        check_execcommand(c1, "/RValue/slashes2", "argv0", "r1", false);

        /* ignore && honour_argv0 */
        r = config_parse_exec("fake", 4, "section",
                              "LValue", 0, "-@/RValue///slashes3/// argv0a r1",
                              &c, NULL);
        assert_se(r >= 0);
        c1 = c1->command_next;
        check_execcommand(c1,
                          "/RValue/slashes3", "argv0a", "r1", true);

        /* ignore && honour_argv0 */
        r = config_parse_exec("fake", 4, "section",
                              "LValue", 0, "@-/RValue///slashes4/// argv0b r1",
                              &c, NULL);
        assert_se(r >= 0);
        c1 = c1->command_next;
        check_execcommand(c1,
                          "/RValue/slashes4", "argv0b", "r1", true);

        /* ignore && ignore */
        r = config_parse_exec("fake", 4, "section",
                              "LValue", 0, "--/RValue argv0 r1",
                              &c, NULL);
        assert_se(r == 0);
        assert_se(c1->command_next == NULL);

        /* ignore && ignore */
        r = config_parse_exec("fake", 4, "section",
                              "LValue", 0, "-@-/RValue argv0 r1",
                              &c, NULL);
        assert_se(r == 0);
        assert_se(c1->command_next == NULL);

        /* semicolon */
        r = config_parse_exec("fake", 5, "section",
                              "LValue", 0,
                              "-@/RValue argv0 r1 ; "
                              "/goo/goo boo",
                              &c, NULL);
        assert_se(r >= 0);
        c1 = c1->command_next;
        check_execcommand(c1,
                          "/RValue", "argv0", "r1", true);

        c1 = c1->command_next;
        check_execcommand(c1,
                          "/goo/goo", "/goo/goo", "boo", false);

        /* trailing semicolon */
        r = config_parse_exec("fake", 5, "section",
                              "LValue", 0,
                              "-@/RValue argv0 r1 ; ",
                              &c, NULL);
        assert_se(r >= 0);
        c1 = c1->command_next;
        check_execcommand(c1,
                          "/RValue", "argv0", "r1", true);

        assert_se(c1->command_next == NULL);

        /* escaped semicolon */
        r = config_parse_exec("fake", 5, "section",
                              "LValue", 0,
                              "/usr/bin/find \\;",
                              &c, NULL);
        assert_se(r >= 0);
        c1 = c1->command_next;
        check_execcommand(c1,
                          "/usr/bin/find", "/usr/bin/find", ";", false);

        exec_command_free_list(c);
}

#define env_file_1 \
        "a\n"      \
        "b\\\n"    \
        "c\n"      \
        "d\\\n"    \
        "e\\\n"    \
        "f\n"      \
        "g\\ \n"   \
        "h\n"      \
        "i\\"

#define env_file_2 \
        "a\\\n"

#define env_file_3 \
        "#SPAMD_ARGS=\"-d --socketpath=/var/lib/bulwark/spamd \\\n" \
        "#--nouser-config                                     \\\n" \
        "normal=line"

static void test_load_env_file_1(void) {
        char _cleanup_strv_free_ **data = NULL;
        int r;

        char name[] = "/tmp/test-load-env-file.XXXXXX";
        int _cleanup_close_ fd = mkstemp(name);
        assert(fd >= 0);
        assert_se(write(fd, env_file_1, sizeof(env_file_1)) == sizeof(env_file_1));

        r = load_env_file(name, &data);
        assert(r == 0);
        assert(streq(data[0], "a"));
        assert(streq(data[1], "bc"));
        assert(streq(data[2], "def"));
        assert(streq(data[3], "g\\"));
        assert(streq(data[4], "h"));
        assert(streq(data[5], "i\\"));
        assert(data[6] == NULL);
        unlink(name);
}

static void test_load_env_file_2(void) {
        char _cleanup_strv_free_ **data = NULL;
        int r;

        char name[] = "/tmp/test-load-env-file.XXXXXX";
        int _cleanup_close_ fd = mkstemp(name);
        assert(fd >= 0);
        assert_se(write(fd, env_file_2, sizeof(env_file_2)) == sizeof(env_file_2));

        r = load_env_file(name, &data);
        assert(r == 0);
        assert(streq(data[0], "a"));
        assert(data[1] == NULL);
        unlink(name);
}

static void test_load_env_file_3(void) {
        char _cleanup_strv_free_ **data = NULL;
        int r;

        char name[] = "/tmp/test-load-env-file.XXXXXX";
        int _cleanup_close_ fd = mkstemp(name);
        assert(fd >= 0);
        assert_se(write(fd, env_file_3, sizeof(env_file_3)) == sizeof(env_file_3));

        r = load_env_file(name, &data);
        assert(r == 0);
        assert(data == NULL);
        unlink(name);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull"

static void test_install_printf(void) {
        char    name[] = "name.service",
                path[] = "/run/systemd/systemd/name.service",
                user[] = "xxxx-no-such-user";
        InstallInfo i = {name, path, user};
        InstallInfo i2 = {name, path, NULL};
        char    name3[] = "name@inst.service",
                path3[] = "/run/systemd/systemd/name.service";
        InstallInfo i3 = {name3, path3, user};
        InstallInfo i4 = {name3, path3, NULL};

        char _cleanup_free_ *mid, *bid, *host;

        assert_se((mid = specifier_machine_id('m', NULL, NULL)));
        assert_se((bid = specifier_boot_id('b', NULL, NULL)));
        assert_se((host = gethostname_malloc()));

#define expect(src, pattern, result)                                    \
        {                                                               \
                char _cleanup_free_ *t = install_full_printf(&src, pattern); \
                char _cleanup_free_                                     \
                        *d1 = strdup(i.name),                           \
                        *d2 = strdup(i.path),                           \
                        *d3 = strdup(i.user);                           \
                memzero(i.name, strlen(i.name));                        \
                memzero(i.path, strlen(i.path));                        \
                memzero(i.user, strlen(i.user));                        \
                assert(d1 && d2 && d3);                                 \
                if (result) {                                           \
                        printf("%s\n", t);                              \
                        assert(streq(t, result));                       \
                } else assert(t == NULL);                               \
                strcpy(i.name, d1);                                     \
                strcpy(i.path, d2);                                     \
                strcpy(i.user, d3);                                     \
        }

        assert_se(setenv("USER", "root", 1) == 0);

        expect(i, "%n", "name.service");
        expect(i, "%N", "name");
        expect(i, "%p", "name");
        expect(i, "%i", "");
        expect(i, "%u", "xxxx-no-such-user");
        expect(i, "%U", NULL);
        expect(i, "%m", mid);
        expect(i, "%b", bid);
        expect(i, "%H", host);

        expect(i2, "%u", "root");
        expect(i2, "%U", "0");

        expect(i3, "%n", "name@inst.service");
        expect(i3, "%N", "name@inst");
        expect(i3, "%p", "name");
        expect(i3, "%u", "xxxx-no-such-user");
        expect(i3, "%U", NULL);
        expect(i3, "%m", mid);
        expect(i3, "%b", bid);
        expect(i3, "%H", host);

        expect(i4, "%u", "root");
        expect(i4, "%U", "0");
}
#pragma GCC diagnostic pop

int main(int argc, char *argv[]) {

        test_unit_file_get_set();
        test_config_parse_exec();
        test_load_env_file_1();
        test_load_env_file_2();
        test_load_env_file_3();
        test_install_printf();

        return 0;
}

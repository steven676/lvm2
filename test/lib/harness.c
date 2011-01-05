/*
 * Copyright (C) 2010 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU General Public License v.2.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

pid_t pid;
int fds[2];

#define MAX 1024

struct stats {
	int nfailed;
	int nskipped;
	int npassed;
	int nwarned;
	int status[MAX];
};

struct stats s;

char *readbuf = NULL;
int readbuf_sz = 0, readbuf_used = 0;

int die = 0;
int verbose = 0;
int verbose_off = 0;

struct subst {
        char *key;
        char *value;
};

struct subst subst[2];

char *TESTDIR = NULL;
char *PREFIX = NULL;

#define PASSED 0
#define SKIPPED 1
#define FAILED 2
#define WARNED 3

void handler( int s ) {
	signal( s, SIG_DFL );
	kill( pid, s );
	die = s;
}

int outline(char *buf, int start, int force) {
        char *from = buf + start;
        char *next = strchr(buf + start, '\n');

        if (!next && !force) /* not a complete line yet... */
                return start;

        if (!next)
                next = from + strlen(from);
        else
                ++next;

        if (!strncmp(from, "@TESTDIR=", 9)) {
                subst[0].key = "@TESTDIR@";
                subst[0].value = strndup(from + 9, next - from - 9 - 1);
        } else if (!strncmp(from, "@PREFIX=", 8)) {
                subst[1].key = "@PREFIX@";
                subst[1].value = strndup(from + 8, next - from - 8 - 1);
        } else {
                char *line = strndup(from, next - from);
                char *a = line, *b;
                do {
                        b = line + strlen(line);
                        int idx = -1;
                        int i;
                        for ( i = 0; i < 2; ++i ) {
                                if (subst[i].key) {
                                        // printf("trying: %s -> %s\n", subst[i].value, subst[i].key);
                                        char *stop = strstr(a, subst[i].value);
                                        if (stop && stop < b) {
                                                idx = i;
                                                b = stop;
                                        }
                                }
                        }
                        fwrite(a, 1, b - a, stdout);
                        a = b;

                        if ( idx >= 0 ) {
                                fprintf(stdout, "%s", subst[idx].key);
                                a += strlen(subst[idx].value);
                        }
                } while (b < line + strlen(line));
                free(line);
        }

        return next - buf + (force ? 0 : 1);
}

void dump() {
        int counter = 0;

        while ( counter < readbuf_used )
                counter = outline( readbuf, counter, 1 );
}

void clear() {
	readbuf_used = 0;
}

void drain() {
	int sz;
	char buf[2048];
        memset(buf, 0, 2048);

	while (1) {
		sz = read(fds[1], buf, 2047);
		if (verbose)
			write(1, buf, sz);
		if (sz <= 0)
			return;
		if (readbuf_used + sz >= readbuf_sz) {
			readbuf_sz = readbuf_sz ? 2 * readbuf_sz : 4096;
			readbuf = realloc(readbuf, readbuf_sz);
		}
		if (!readbuf)
			exit(205);
		memcpy(readbuf + readbuf_used, buf, sz);
		readbuf_used += sz;
		readbuf[readbuf_used] = 0;
	}
}

void passed(int i, char *f) {
	if (strstr(readbuf, "TEST WARNING")) {
		++s.nwarned;
		s.status[i] = WARNED;
		printf("warnings\n");
	} else {
		++ s.npassed;
		s.status[i] = PASSED;
		printf("passed.\n");
	}
}

void skipped(int i, char *f) {
	++ s.nskipped;
	s.status[i] = SKIPPED;
	printf("skipped.\n");
}

void failed(int i, char *f, int st) {
	++ s.nfailed;
	s.status[i] = FAILED;
	if(die == 2) {
		printf("interrupted.\n");
		return;
	}
	printf("FAILED.\n");
	printf("-- FAILED %s ------------------------------------\n", f);
	dump();
	printf("-- FAILED %s (end) ------------------------------\n", f);
}

void run(int i, char *f) {
	pid = fork();
	if (pid < 0) {
		perror("Fork failed.");
		exit(201);
	} else if (pid == 0) {
		close(0);
		dup2(fds[0], 1);
		dup2(fds[0], 2);
                close(fds[0]);
                close(fds[1]);
		execlp("bash", "bash", f, NULL);
		perror("execlp");
		fflush(stderr);
		_exit(202);
	} else {
		char buf[128];
		snprintf(buf, 128, "%s ...", f);
		buf[127] = 0;
		printf("Running %-40s ", buf);
		fflush(stdout);
		int st, w;
		while ((w = waitpid(pid, &st, WNOHANG)) == 0) {
			drain();
			usleep(20000);
		}
		if (w != pid) {
			perror("waitpid");
			exit(206);
		}
		drain();
		if (WIFEXITED(st)) {
			if (WEXITSTATUS(st) == 0) {
				passed(i, f);
			} else if (WEXITSTATUS(st) == 200) {
				skipped(i, f);
			} else {
				failed(i, f, st);
			}
		} else {
			failed(i, f, st);
		}
		clear();
	}
}

int main(int argc, char **argv) {
	int i;

	if (argc >= MAX) {
		fprintf(stderr, "Sorry, my head exploded. Please increase MAX.\n");
		exit(1);
	}

	s.nwarned = s.nfailed = s.npassed = s.nskipped = 0;

	char *be_verbose = getenv("VERBOSE");
	if (be_verbose && atoi(be_verbose))
		verbose = 1; // XXX

	if (socketpair(PF_UNIX, SOCK_STREAM, 0, fds)) {
		perror("socketpair");
		return 201;
	}

        if ( fcntl( fds[1], F_SETFL, O_NONBLOCK ) == -1 ) {
		perror("fcntl on socket");
		return 202;
	}

	/* set up signal handlers */
        for (i = 0; i <= 32; ++i) {
            if (i == SIGCHLD || i == SIGWINCH || i == SIGURG)
                continue;
            signal(i, handler);
        }

	/* run the tests */
	for (i = 1; i < argc; ++ i) {
		run(i, argv[i]);
		if (die)
			break;
	}

	printf("\n## %d tests: %d OK, %d warnings, %d failures; %d skipped\n",
	       s.nwarned + s.npassed + s.nfailed + s.nskipped,
	       s.npassed, s.nwarned, s.nfailed, s.nskipped);

	/* print out a summary */
	if (s.nfailed || s.nskipped) {
		for (i = 1; i < argc; ++ i) {
			switch (s.status[i]) {
			case FAILED:
				printf("FAILED: %s\n", argv[i]);
				break;
			case SKIPPED:
				printf("skipped: %s\n", argv[i]);
				break;
			}
		}
		printf("\n");
		return s.nfailed > 0 || die;
	}
	return die;
}
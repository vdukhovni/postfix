/*
 * Test: SIGTERM during malloc triggers heap corruption in master_sigdeath().
 *
 * Postfix's master_sigdeath() signal handler calls msg_info() and
 * msg_fatal(), which internally use formatting (vstring/snprintf)
 * and syslog — all of which call malloc(). If the signal arrives
 * while the process is already inside malloc(), the heap is corrupted.
 *
 * This test reproduces the bug by:
 *   1. Forking a child that loops in malloc/free (simulating normal work)
 *   2. Installing a signal handler that calls malloc-dependent functions
 *      (simulating what master_sigdeath does)
 *   3. Sending SIGTERM from the parent to hit the malloc window
 *
 * Run with Guard Malloc on macOS for reliable detection:
 *   DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib ./master_sig_test
 *
 * Or with ASAN:
 *   cc -fsanitize=address -g -o master_sig_test master_sig_test.c
 */

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

static volatile sig_atomic_t got_signal = 0;

/*
 * Simulate what master_sigdeath() does: call non-async-safe functions
 * from a signal handler. The real code calls msg_info() which goes
 * through vstring formatting and syslog(). We use fprintf + snprintf
 * which have the same malloc-reentrancy problem.
 */
static void sigdeath_like_master(int sig)
{
    char    buf[256];

    /*
     * This is the bug: snprintf and fprintf call malloc internally.
     * If we interrupted malloc(), this reenters it and corrupts the heap.
     */
    snprintf(buf, sizeof(buf), "terminating on signal %d", sig);
    fprintf(stderr, "[child] %s\n", buf);
    fflush(stderr);

    /*
     * Do some allocations from the signal handler to make corruption
     * more likely to manifest immediately.
     */
    for (int i = 0; i < 100; i++) {
        void   *p = malloc(64 + (i * 7));

        if (p == NULL) {
            const char msg[] = "[child] malloc returned NULL in signal handler\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            _exit(99);
        }
        memset(p, 0xAA, 64);
        free(p);
    }

    got_signal = 1;
}

/*
 * Child process: tight malloc/free loop to maximize the window where
 * the heap lock is held, then handle SIGTERM like master does.
 */
static void child_work(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigdeath_like_master;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);

    /* Tell parent we're ready. */
    write(STDOUT_FILENO, "R", 1);

    /*
     * Tight alloc/free loop. Each iteration is a chance for the signal
     * to arrive while we're inside malloc or free.
     */
    while (!got_signal) {
        void   *p = malloc(1 + (arc4random() % 4096));

        if (p == NULL) {
            fprintf(stderr, "[child] malloc returned NULL — heap corrupt?\n");
            _exit(99);
        }
        memset(p, 0xBB, 1);
        free(p);
    }

    /*
     * Post-signal allocations to detect latent heap corruption that
     * didn't crash immediately.
     */
    for (int i = 0; i < 10000; i++) {
        void   *p = malloc(1 + (arc4random() % 4096));

        if (p == NULL) {
            fprintf(stderr, "[child] post-signal malloc failed — heap corrupt\n");
            _exit(99);
        }
        memset(p, 0xCC, 1);
        free(p);
    }
    _exit(0);
}

int main(int argc, char **argv)
{
    int     attempts = 200;
    int     failures = 0;
    int     signals_caught = 0;

    if (argc > 1)
        attempts = atoi(argv[1]);

    fprintf(stderr, "Testing signal handler async-safety (%d attempts)...\n",
            attempts);
    fprintf(stderr, "Run with Guard Malloc for best detection:\n");
    fprintf(stderr, "  DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib %s\n\n",
            argv[0]);

    for (int i = 0; i < attempts; i++) {
        int     pipefd[2];

        if (pipe(pipefd) < 0) {
            perror("pipe");
            return 1;
        }

        pid_t   pid = fork();

        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            child_work();
            _exit(0);
        }
        close(pipefd[1]);

        /* Wait for child to be ready (in the malloc loop). */
        char    ready;

        read(pipefd[0], &ready, 1);
        close(pipefd[0]);

        /*
         * Random delay 0-2ms so we hit different points in the malloc
         * cycle.
         */
        usleep(arc4random() % 2000);
        kill(pid, SIGTERM);

        int     status;

        waitpid(pid, &status, 0);

        if (WIFSIGNALED(status)) {
            fprintf(stderr, "  attempt %3d: CRASHED (signal %d) — "
                    "heap corruption\n", i, WTERMSIG(status));
            failures++;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) == 99) {
            fprintf(stderr, "  attempt %3d: malloc failed — "
                    "heap corruption detected\n", i);
            failures++;
        } else if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            signals_caught++;
        } else {
            fprintf(stderr, "  attempt %3d: unexpected status %d\n",
                    i, status);
        }
    }

    fprintf(stderr, "\nResults:\n");
    fprintf(stderr, "  %d/%d clean (signal handled without hitting window)\n",
            signals_caught, attempts);
    fprintf(stderr, "  %d/%d corrupted (signal hit malloc window)\n",
            failures, attempts);

    if (failures > 0) {
        fprintf(stderr, "\nBUG CONFIRMED: signal handler calling "
                "non-async-safe functions corrupts heap.\n");
        return 1;
    } else {
        fprintf(stderr, "\nNo corruption detected in %d attempts.\n"
                "Try more attempts or run with Guard Malloc.\n", attempts);
        return 0;
    }
}

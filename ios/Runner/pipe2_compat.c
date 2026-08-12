// Apple platforms (iOS/macOS) don't implement the Linux-only pipe2() syscall,
// but the prebuilt libGStreamer.a (via glib) references it unconditionally.
// Provide a real, correct implementation in terms of pipe() + fcntl(), the
// same portability technique used by gnulib's pipe2 replacement.
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int pipe2(int pipefd[2], int flags) {
    if (flags & ~(O_CLOEXEC | O_NONBLOCK)) {
        errno = EINVAL;
        return -1;
    }

    if (pipe(pipefd) != 0) {
        return -1;
    }

    if (flags & O_CLOEXEC) {
        fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
    }
    if (flags & O_NONBLOCK) {
        fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
        fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
    }

    return 0;
}

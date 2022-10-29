#include <asm-generic/errno-base.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>

static volatile int token = 0;
const int kTokenLimit = 100;

void sighandler(int s) {
    if (s != SIGALRM) {
        return;
    }
    alarm(1);
    token++;
    if (token > kTokenLimit) {
        token = kTokenLimit;
    }
}

// 考虑信号影响的读写调用
int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage...\n");
        exit(EXIT_FAILURE);
    }

    int sfd = open(argv[1], O_RDONLY);
    if (sfd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    signal(SIGALRM, sighandler);
    alarm(1);

    // 每秒读十个
    const int kCnt = 10;
    while (true) {
        char buf[kCnt];
        // 令牌桶核心：当read、write被打断时，token值可能会大于1，此时一秒内可以read更多的数据！！！
        while (token <= 0) {
            pause();
        }
        token--;
        ssize_t rtd = read(sfd, buf, kCnt);
        if (rtd < 0) {
            if (rtd == EINTR) {
                continue;
            }
            perror("read");
            exit(EXIT_FAILURE);
        }
        if (rtd == 0) {
            break;
        }
        // write
        ssize_t wtd = 0;
        while (wtd < rtd) {
            ssize_t ret = write(STDOUT_FILENO, buf + wtd, rtd - wtd);
            if (ret < 0) {
                if (ret == EINTR) {
                    continue;
                }
                perror("write");
                exit(EXIT_FAILURE);
            }
            wtd += ret;
        }
    }

    return 0;
}
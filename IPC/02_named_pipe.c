#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FIFO_NAME "myfifo"

int main() {
    int fd;
    char write_msg[] = "Hello, child process!";

    // FIFO 파일 생성
    if(mkfifo(FIFO_NAME, 0666) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        fd = open(FIFO_NAME, O_WRONLY);
        if(fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        // 메시지 쓰기
        write(fd, write_msg, strlen(write_msg) + 1);
        close(fd);
    } else {
        char read_msg[100];

        fd = open(FIFO_NAME, O_RDONLY);
        if(fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        // 메시지 읽기
        read(fd, read_msg, sizeof(read_msg));
        printf("Child process received message: %s\n", read_msg);
        close(fd);

        // FIFO 파일 삭제
        unlink(FIFO_NAME);
    }

    return 0;
}
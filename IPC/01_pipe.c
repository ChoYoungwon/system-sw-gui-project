#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main() {
    int pipefd[2];   // 파이프 파일 디스크립터 배열, 0 : 읽기 전용, 1 : 쓰기 전용
    pid_t pid;
    char write_msg[] = "Hello, child process!";
    char read_msg[100];

    // 파이프 생성
    if(pipe(pipefd)== -1) {     // 성공 시, 운영체제가 pipefd를 유효한 값으로 채운다(파일 디스크립터)
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    // 자식 프로세스 생성
    pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {          
        // 부모 프로세스
        close(pipefd[0]);                                           // 읽기 종단 닫기
        write(pipefd[1], write_msg, strlen(write_msg) + 1);         // 메시지 쓰기(NULL 문자까지 함께 보내기 위해 1을 더해줌)
        close(pipefd[1]);                                           // 쓰기 종단 닫기
    } else {
        // 자식 프로세스
        close(pipefd[1]);                                           // 쓰기 종단 닫기
        read(pipefd[0], read_msg, sizeof(read_msg));                // 메시지 읽기
        printf("Child process received message: %s\n", read_msg);   // 메시지 읽기
        close(pipefd[0]);                                           // 읽기 종단 닫기
    }

    return 0;
}
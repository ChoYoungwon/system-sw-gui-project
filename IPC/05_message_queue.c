#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MSG_SIZE 100

// 메시지 구조체 정의
struct msg_buffer {
    long msg_type;
    char msg_text[MSG_SIZE];
};

int main() {
    key_t key = ftok("msgqueue", 65);           // 키 생성
    int msgid = msgget(key, 0666 | IPC_CREAT);  // 메시지 큐 생성
    if (msgid == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        struct msg_buffer message;
        message.msg_type = 1;
        strcpy(message.msg_text, "Hello, child process!");

        // 메시지 큐에 메시지 전송
        if (msgsnd(msgid, &message, sizeof(message.msg_text), 0) == -1) {
            perror("msgsnd");
            exit(EXIT_FAILURE);
        }

        wait(NULL); // 자식 프로세스 종료 대기

        // 메시지 큐 삭제
        if(msgctl(msgid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
            exit(EXIT_FAILURE);
        }
    } else {
        struct msg_buffer message;

        // 메시지 큐에서 메시지 수신
        if (msgrcv(msgid, &message, sizeof(message.msg_text), 1, 0) == -1) {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        printf("Child process received message: %s\n", message.msg_text);
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>

#define SHM_SIZE 1024       // 공유 메모리 크기

struct sembuf sem_op;       // 세마포어 작업 구조

int main() {
    key_t key = ftok("shmfile", 65);    // 키 생성

    // 공유 메모리 생성
    int shmid = shmget(key, SHM_SIZE, 0666 | IPC_CREAT);    
    if (shmid == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    // 세마포어 생성
    int semid = semget(key, 1, 0666 | IPC_CREAT);   
    if (semid == -1) {
        perror("semget");
        exit(EXIT_FAILURE);
    }

    // 세마포어 초기화
    if (semctl(semid, 0, SETVAL, 0) == -1) {
        perror("semctl");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        // 공유 메모리 연결
        char *str = (char*) shmat(shmid, NULL, 0);
        if (str == (char*) -1 ) {
            perror("shmat");
            exit(EXIT_FAILURE);
        }

        char write_msg[] = "Hello, child process!";
        strcpy(str, write_msg);     // 메시지 쓰기

        // 세마포어 증가 (자식 프로세스가 메시지를 읽을 수 있도록 신호 보내기)
        sem_op.sem_num = 0;
        sem_op.sem_op = 1;      // 세마포어 값을 1 만큼 증가(V 연산)
        sem_op.sem_flg = 0;
        if (semop(semid, &sem_op, 1) == -1) {
            perror("semop");
            exit(EXIT_FAILURE);
        }

        shmdt(str);     // 공유 메모리 분리
        wait(NULL);     // 자식 프로세스 종료 대기

        // 공유 메모리 및 세마포어 삭제
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID);
    } else {
        char *str = (char*) shmat(shmid, NULL, 0);
        if (str == (char*) -1) {
            perror("shmat");
            exit(EXIT_FAILURE);
        }

        // 세마포어 감소 (부모 프로세스의 신호 대기)
        sem_op.sem_num = 0;
        sem_op.sem_op = -1;     // 세마포어 값을 1만큼 감소시키라는 설정(P 연산)
        sem_op.sem_flg = 0;
        if (semop(semid, &sem_op, 1) == -1) {
            perror("semop");
            exit(EXIT_FAILURE);
        }

        printf("Child process received message : %s \n", str);  // 메시지 읽기
        shmdt(str);
    }
    return 0;
}

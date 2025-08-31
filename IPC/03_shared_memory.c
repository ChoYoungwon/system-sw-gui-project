#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>


#define SHM_SIZE 1024       // 공유 메모리 크기

int main() {
    int fd;
    char write_msg[] = "Hello, child process!";
    key_t key = 1234;       // 공유 메모리를 위한 키
    char *shm_addr;

    // 1. 공유 메모리 공간 생성 요청 (shmget)
    int shmid = shmget(key, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {  // 부모 프로세스
        // 2. 생성된 공유 메모리를 프로세스 주소 공간에 연결 (shmat)
        shm_addr = (char*) shmat(shmid, NULL, 0);  // 공유 메모리 연결
        if (shm_addr == (char *) -1) {
            perror("shmat");
            exit(EXIT_FAILURE);
        }

        // 3. 공유 메모리에 데이터 쓰기
        strcpy(shm_addr, "Hello from Shared Memory!");
        printf("Parrent : Data written to shared memory.\n");

        // 4. 공유 메모리 연결 해제 (shmdt)
        shmdt(shm_addr);

        wait(NULL);     // 자식이 다 쓸 때까지 기다림

        // 5. 공유 메모리 삭제 (shmctl)
        shmctl(shmid, IPC_RMID, NULL);
    } else {
        // 공유 메모리 연결
        shm_addr = (char*) shmat(shmid, NULL, 0);
        if (shm_addr == (char *) -1) {
            perror("shmat");
            exit(EXIT_FAILURE);
        }

        // 공유 메모리에서 데이터 읽기
        printf("Child received message: %s\n", shm_addr);

        // 공유 메모리 연결 해제
        shmdt(shm_addr);
    }

    return 0;
}

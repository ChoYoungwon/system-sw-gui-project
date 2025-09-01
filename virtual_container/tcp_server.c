#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 9000

char buffer[BUFSIZ] = "hello, world";

int main()
{
    int c_socket, s_socket;
    struct sockaddr_in s_addr, c_addr;
    int len;
    int n;

    // 소캣 생성
    s_socket = socket(PF_INET, SOCK_STREAM, 0);

    // 연결 요청을 수신할 주소 설정
    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(PORT);

    // 소캣을 포트에 연결
    if(bind(s_socket, (struct sockaddr *) &s_addr, sizeof(s_addr)) == -1) {
        printf("Can anot Bind \n");
        return -1;
    }

    // 커널에 개통 요청
    if(listen(s_socket, 5) == -1) {
        printf("listen Fail\n");
        return -1;
    }

    while (1) {
        // 클라이언트 연결 요청 수신
        len = sizeof(c_addr);
        c_socket = accept(s_socket, (struct sockaddr *)&c_addr, &len);

        // 클라이언트 요청 서비스 제공
        n = strlen(buffer);
        write(c_socket, buffer, n);

        // 클라이언트와 연결 종료
        close(c_socket);

    }

    // 서버 종료
    close(s_socket);

    return 0;
}
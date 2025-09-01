#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h> // memset, strlen 사용
#include <unistd.h> // read, write, close 사용
#include <arpa/inet.h> // inet_addr 사용
#include <netdb.h> // gethostbyname 사용

#define PORT 9000
#define SERVER_HOSTNAME "server"

int main() 
{
    int c_socket;
    struct sockaddr_in c_addr;
    struct hostent *server_host;
    int len;
    int n;

    char rcvBuffer[BUFSIZ];

    if ((server_host = gethostbyname(SERVER_HOSTNAME)) == NULL) {
        printf("Can not find host: %s\n", SERVER_HOSTNAME);
        return -1;
    }

    // 소켓을 생성
    c_socket = socket(PF_INET, SOCK_STREAM, 0);

    // 연결할 서버의 주소 설정
    memset(&c_addr, 0, sizeof(c_addr));
    c_addr.sin_family = AF_INET;

    memcpy(&c_addr.sin_addr.s_addr, server_host->h_addr_list[0], server_host->h_length);
    c_addr.sin_port = htons(PORT);

    // 소켓을 서버에 연결
    if(connect(c_socket, (struct sockaddr *) &c_addr, sizeof(c_addr)) == -1) {
        printf("Can not connect \n");
        close(c_socket);
        return -1;
    }

    // 서비스 요청과 처리
    if((n = read(c_socket, rcvBuffer, sizeof(rcvBuffer))) < 0) {
        return (-1);
    }

    // 소켓 연결을 종료
    rcvBuffer[n] = '\0';
    printf("received Data : %s\n", rcvBuffer);

    close(c_socket);
}
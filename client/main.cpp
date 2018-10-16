#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>

#include "wrap.h"

#define MAXLINE 8192
#define SERV_PORT 8000
#define OPEN_MAX 5000

int main(int argc, char *argv[])
{
    struct sockaddr_in servaddr;
    char buf[MAXLINE];
    int client_sockfd, n;

    ssize_t nready, efd, res;
    struct epoll_event tep, ep[OPEN_MAX];       //tep: epoll_ctl参数  ep[] : epoll_wait参数

    FILE* fp = stdin;

    int stdinfd = fileno(fp);

    client_sockfd = Socket(AF_INET, SOCK_STREAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    servaddr.sin_port = htons(SERV_PORT);

    Connect(client_sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

    efd = epoll_create(OPEN_MAX);               //创建epoll模型, efd指向红黑树根节点
    if (efd == -1)
        perr_exit("epoll_create error");

    tep.events = EPOLLIN; tep.data.fd = client_sockfd;           //指定lfd的监听时间为"读"
    res = epoll_ctl(efd, EPOLL_CTL_ADD, client_sockfd, &tep);    //将lfd及对应的结构体设置到树上,efd可找到该树
    if (res == -1)
        perr_exit("epoll_add sockfd error");

    tep.events = EPOLLIN; tep.data.fd = stdinfd;           //指定lfd的监听时间为"读"
    res = epoll_ctl(efd, EPOLL_CTL_ADD,stdinfd , &tep);    //将lfd及对应的结构体设置到树上,efd可找到该
    if (res == -1)
        perr_exit("epoll_stdin error");

    for ( ; ; ) {
        /*epoll为server阻塞监听事件, ep为struct epoll_event类型数组, OPEN_MAX为数组容量, -1表永久阻塞*/
        nready = epoll_wait(efd, ep, OPEN_MAX, -1);
        if (nready == -1)
            perr_exit("epoll_wait error");

        for (int i = 0; i < nready; i++) {
            int fd = ep[i].data.fd;
            n = Read(fd, buf, MAXLINE);

            if (n == 0 && fd == client_sockfd) {                       //读到0,说明客户端关闭链接

                return 0;

            } else if (n < 0) {                 //出错
                perror("read n < 0 error: ");
                res = epoll_ctl(efd, EPOLL_CTL_DEL, client_sockfd, NULL);
                Close(client_sockfd);

            } else {                            //实际读到了字节数
                if (fd == client_sockfd) {
                    Write(STDOUT_FILENO, buf, n);
                } else {
                    Write(client_sockfd, buf, n);
                }
            }
        }
    }
}


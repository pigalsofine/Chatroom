#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <ctype.h>
#include <iostream>
#include <map>
#include <csignal>
#include "wrap.h"
#include "threadpool.h"

using namespace std;

#define MAXLINE 8192
#define SERV_PORT 8000
#define OPEN_MAX 5000

map<int,string> all_fd;
struct send_msg{
    int socket;
    string msg;
};

void* start_thread(void* args)
{
    send_msg tmp_send_msg = *((struct send_msg*)args);

    const char* pbuf = tmp_send_msg.msg.data();

    Writen(tmp_send_msg.socket, pbuf, strlen(pbuf));

}

int main(int argc, char *argv[])
{

    int i, listenfd, connfd, sockfd;
    int  n, num = 0;
        ssize_t nready, efd, res;
    char buf[MAXLINE], str[INET_ADDRSTRLEN];
    socklen_t clilen;

    string tmp;

    struct threadpool *pool = threadpool_init(10, 20);
    pthread_t ntid;

    struct sockaddr_in cliaddr, servaddr;
    struct epoll_event tep, ep[OPEN_MAX];       //tep: epoll_ctl参数  ep[] : epoll_wait参数

    listenfd = Socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));      //端口复用

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    Bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    Listen(listenfd, 20);


    efd = epoll_create(OPEN_MAX);               //创建epoll模型, efd指向红黑树根节点
    if (efd == -1)
        perr_exit("epoll_create error");

    tep.events = EPOLLIN; tep.data.fd = listenfd;           //指定lfd的监听时间为"读"
    res = epoll_ctl(efd, EPOLL_CTL_ADD, listenfd, &tep);    //将lfd及对应的结构体设置到树上,efd可找到该树
    if (res == -1)
        perr_exit("epoll_ctl error");

    for ( ; ; ) {
        /*epoll为server阻塞监听事件, ep为struct epoll_event类型数组, OPEN_MAX为数组容量, -1表永久阻塞*/
        nready = epoll_wait(efd, ep, OPEN_MAX, -1);
        if (nready == -1)
            perr_exit("epoll_wait error");

        for (i = 0; i < nready; i++) {
            if (!(ep[i].events & EPOLLIN))      //如果不是"读"事件, 继续循环
                continue;

            if (ep[i].data.fd == listenfd) {    //判断满足事件的fd是不是lfd
                clilen = sizeof(cliaddr);
                connfd = Accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);    //接受链接

                string user_description;
                user_description = "<ip:";
                user_description += inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str));
                user_description += " port:"+to_string( ntohs(cliaddr.sin_port) )+">";

                cout << user_description<<"\n";
                all_fd.insert(make_pair(connfd,user_description));

                printf("received from %s at PORT %d\n",
                       inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
                       ntohs(cliaddr.sin_port));
                printf("cfd %d---client %d\n", connfd, ++num);

                tep.events = EPOLLIN; tep.data.fd = connfd;
                res = epoll_ctl(efd, EPOLL_CTL_ADD, connfd, &tep);
                if (res == -1)
                    perr_exit("epoll_ctl error");

            } else {                                //不是lfd,
                sockfd = ep[i].data.fd;
                n = Read(sockfd, buf, MAXLINE);

                if (n == 0) {                       //读到0,说明客户端关闭链接
                    res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);  //将该文件描述符从红黑树摘除
                    if (res == -1)
                        perr_exit("epoll_ctl error");
                    Close(sockfd);                  //关闭与该客户端的链接
                    printf("client[%d] closed connection\n", sockfd);

                } else if (n < 0) {                 //出错
                    perror("read n < 0 error: ");
                    res = epoll_ctl(efd, EPOLL_CTL_DEL, sockfd, NULL);
                    Close(sockfd);

                    all_fd.erase(sockfd);

                } else {                            //实际读到了字节数

                    map<int,string>::iterator iter;


                    for (iter = all_fd.begin(); iter != all_fd.end(); iter++){


                        if (iter->first != sockfd){

                            tmp = iter->second + buf;

                            cout << "tmp "<< tmp<<"\n";

                         //   const char* pbuf = tmp.data();

                            send_msg *tmp_send_msg = new  send_msg();
                            tmp_send_msg->socket = iter->first;
                            tmp_send_msg->msg = tmp;

                            threadpool_add_job(pool, start_thread, (void*)tmp_send_msg);

                         //   cout << "pbuf "<< pbuf<<"\n";



                          //  Writen(iter->first, pbuf, strlen(pbuf));
                        }
                    }
                }
            }
        }
    }
}

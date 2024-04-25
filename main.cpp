#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535 //最大文件描述符个数
#define MAX_EVENT_NUMBER 10000 //一次性可以监听的最大事件数量

//添加信号捕捉
void addsig(int sig,void(handler)(int)){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

//添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
//从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);
//从epoll中修改文件描述符
extern void modfd(int epollfd, int fd, int ev);

int main(int argc, char* argv[]){
    if(argc <= 1){
        printf("请至少输入两个参数,第一个参数是程序名，第二个参数是port_number\n");
        exit(-1);
    }

    //获取端口号
    int port = atoi(argv[1]);

    //对SIGPIPE信号进行处理
    addsig(SIGPIPE,SIG_IGN);

    //创建线程池，初始化所有线程,此时所以线程都阻塞在run函数中，等待信号量m_queuestat的值增加（意味着此时请求队列中有新的任务进来）
    threadpool<http_conn> * pool = NULL;
    try{
        pool = new threadpool<http_conn>;   //堆区开辟线程池，此时所以线程共享threadpool中的成员变量和函数,使用完需要释放该资源
    }catch(...){
        exit(-1);
    }

    //创建一个数组用来保存客户端信息
    http_conn * users = new http_conn[MAX_FD];

    //创建服务器监听的fd
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    //设置端口复用(端口复用必须在绑定之前),以避免服务器关闭后需要等待2MSL时间才能再次使用端口
    int reuse = 1;
    setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

    //将服务器的listenfdfd和其地址、端口进行绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr =INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd,(struct sockaddr*)&address,sizeof(address));

    //开始监听listenfd文件描述符上是否有数据输入进来
    listen(listenfd,5);

    //创建epoll对象,事件数组，添加
    int epollfd = epoll_create(5);
    epoll_event events[MAX_EVENT_NUMBER];
    
    //将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd;
    
    //开始监听epoll实例中epollfd指向的就绪列表（存储的fd列表）是否有数据输入
    while(1){
        // 就绪列表中如果没有数据输入就阻塞在这里；
        // 如果有数据输入则返回监听到的fd的数目（有数据输入的fd的数目），并且将事件存储在事件数组events中
        int num=epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
    
        //开始循环遍历事件数组
        for(int i = 0; i < num; ++i){
            // 事件数组中存储的fd其实就是发生了变化的文件描述符的fd
            int sockfd = events[i].data.fd;

            // 如果sockfd==listenfd，那意味着服务端的监听描述符监听到了客户端的连接请求数据，接下来需要将客户端的各种信息存储下来
            if(sockfd == listenfd){
                sockaddr_in client_address; // 客户端的ip地址和端口信息存储在client_address中
                socklen_t client_addrlen = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr*)&client_address,&client_addrlen);//服务器生成connfd用来代表客户端fd

                //目前连接数满了,给客户端写一个信息：服务器正忙
                if(http_conn::m_user_count >= MAX_FD){
                    close(connfd);
                    continue;
                }

                //将新客户端的信息存入users数组中http_conn类型的对象，并进行初始化
                users[connfd].init(connfd, client_address);
            }
            
            //对方是读事件
            else if(events[i].events & EPOLLIN){
                //循环把所有数据都读完
                if(users[sockfd].read()){               //将数据读取到该http_conn对象中的m_read_buf缓冲区中并更新了m_read_idx的值
                    //将读取的数据插入请求队列中
                    pool->append(users+sockfd);         //将读取到的数据发送到请求队列中去，此时信号量m_queuestat的值会增加，线程池中会随机挑选一个线程对该数据进行处理
                }
                //对方关闭连接或读取数据出现错误就关闭连接，并在epollfd指向的就绪列表中删除sockfd文件描述符
                else{
                    users[sockfd].close_conn();
                }
            }         
            
            
            //对方是写事件
            else if(events[i].events & EPOLLOUT){
                if(!users[sockfd].write()){
                    //没能一次性写完所有数据
                    users[sockfd].close_conn();
                }
            }

            //对方异常断开或错误等事件
            else if(events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR)){
                users[sockfd].close_conn();
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete [] users;
    delete pool;
    return 0;
}

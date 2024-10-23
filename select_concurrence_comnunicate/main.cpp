#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<arpa/inet.h>
#include<pthread.h>
#include<ctype.h>

pthread_mutex_t mutex;              //因为有共享数据所以需要初始化而且要加锁


typedef struct message {
    int* fd;
    fd_set* reset;
    int* maxi;
}message;       //线程里需要用到的函数


void* AcceptConnect(void* arg);      //接收连接的线程的任务函数
void *Receive(void* arg);


int main()
{
    pthread_mutex_init(&mutex, NULL);

    //创建套接字
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
    {
        perror("socket");
    }

    //绑定
    struct  sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(9999);
    saddr.sin_addr.s_addr = INADDR_ANY;
    socklen_t len = sizeof(saddr);
    int ret = bind(fd, (struct sockaddr*)&saddr, len);
    if (ret == -1)
    {
        perror("bind");
    }
    //监听
    ret = listen(fd, 100);
    if (ret == -1)
    {
        perror("listen");
    }

    struct sockaddr_in caddr;
    socklen_t lens = sizeof(caddr);

    fd_set reset;//文件描述符集合
    FD_ZERO(&reset);//初始化集合
    FD_SET(fd, &reset);//把fd放入集合中

    int maxi = fd;      //记录最大的那个文件描述符

    while (1)
    {
        pthread_mutex_lock(&mutex);
        fd_set tmp = reset;     //因为内核会修改数据所以要先把它存起来
        pthread_mutex_unlock(&mutex);
        int ret = select(maxi + 1, &tmp, NULL, NULL, NULL);         //先判断文件描述符是否可读
        //判断是不是监听的文件描述符
        if (FD_ISSET(fd, &tmp))
        {
            message ms;
            ms.fd = &fd;
            ms.maxi = &maxi;
            ms.reset = &reset;
            //创建线程
            pthread_t tid;
            pthread_create(&tid, NULL, AcceptConnect, &ms);
            //和主线程分离
            pthread_detach(tid);
        }
        for (int i = 0; i <= maxi; i++)
        {
            if (i != fd && FD_ISSET(i, &tmp))       //首先检测文件描述它不是用于监听的 而且它要在检测后的集合中
            {
                message ms;
                ms.fd = &i;
                ms.reset = &reset;
                //创建线程
                pthread_t tid;
                pthread_create(&tid, NULL, Receive, &ms);
                //和主线程分离
                pthread_detach(tid);
            }
        }

    }

    close(fd);
    pthread_mutex_destroy(&mutex);      //销毁锁
    return 0;
}



void* AcceptConnect(void* arg)
{
    printf("子线程ID: %ld\n", pthread_self());
    message* ms = (message*)arg;
    int fds = accept(*(ms->fd), NULL, NULL);
    pthread_mutex_lock(&mutex);
    FD_SET(fds,ms->reset);        //把新的文件描述符存入集合中
    if (fds > *(ms->maxi))
    {
        *(ms->maxi) = fds;
    }
    pthread_mutex_unlock(&mutex);
    free(ms);
    return NULL;
}


void *Receive(void* arg)
{
    printf("子线程ID: %ld\n", pthread_self());
    message* ms = (message*)arg;


    //接收数据
    char buff[1024];
    int len = recv(*(ms->fd), buff, sizeof(buff), 0);//接收客户端数据

    if (len > 0)
    {
        printf("client says:%s\n", buff);
        send(*(ms->fd), buff, strlen(buff) + 1, 0); //再把数据发送回去

    }
    else if (len == 0)
    {
        printf("client disconnected");
        pthread_mutex_lock(&mutex);
        FD_CLR(*(ms->fd), ms->reset);     //断开连接之后要把文件描述符从集合里删除
        pthread_mutex_unlock(&mutex);
        close(*(ms->fd));

        free(ms);
        return NULL;
    }
    else {
        perror("recv");
        free(ms);
        return NULL;
    }

    free(ms);
    return NULL;
}
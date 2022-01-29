#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <poll.h>
#include <map>

#define SERVER_PORT 3311
#define QUEUE 10

struct client{
    int fd;
    char *address;
    bool status;
};

int kill(std::string announcment){
    std::cerr<<announcment<<':'<<errno;
    exit(0);
}

void* getAddress(sockaddr* sa){
    return &(((sockaddr_in*)sa)->sin_addr.s_addr);
}

void getListener(int &serv, sockaddr_in &server){
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_family = AF_INET;
    server.sin_port = htons(SERVER_PORT);
    memset(server.sin_zero,0,sizeof(server.sin_zero));
    if((serv= socket(AF_INET, SOCK_STREAM, 0))<0)
        kill("Socket()");
    if((bind(serv, (sockaddr*)&server, sizeof(server)))<0)
        kill("bind()");
    if((listen(serv, QUEUE))<0)
        kill("listen()");
}

void addFd(pollfd *&ptr, int fd, int event){
    ptr[0].fd = fd;
    ptr[0].events = event;
}

int main(){

    int serverSockfd, clientSockfd;
    int num_fds = 1;
    char buf[INET_ADDRSTRLEN];
    socklen_t addr_len = INET_ADDRSTRLEN;
    int pollfd_size = 100;

    struct pollfd *pollfds = new pollfd[pollfd_size];
    struct client clients[pollfd_size];

    struct sockaddr_in server; struct sockaddr_storage client;

    getListener(serverSockfd, server);

    pollfds[0].fd = serverSockfd;
    pollfds[0].events = POLLIN;
    std::cout<<"Waiting for connections..."<<std::endl;

    while(1){

        int res = poll(pollfds, num_fds, -1);
        if(res<0)
            kill("poll()");

        for(int i=0;i<num_fds;i++){

            if(i==serverSockfd){
                clientSockfd = accept(serverSockfd, (sockaddr*)&client, &addr_len);
                std::cout<<"Connection established with: ";
                inet_ntop(AF_INET, getAddress((sockaddr*)&client), buf, sizeof(buf));
                std::cout<<buf<<'\n';
                addFd(pollfds, clientSockfd, POLLIN);

                clients[0].address = buf;
                clients[0].fd = clientSockfd;
                clients[0].status = 0;
            }
            else{
                
            }
        }
    }

    return 0;
}

#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <string.h>
#include <poll.h>
#include <unistd.h>

#define SERVER_PORT 3311
#define QUEUE 10

struct client{
    int fd;
    char *address;
    bool status;
};

int kill(std::string announcment){
    std::cerr<<announcment<<':'<<errno<<std::endl;
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

void addFd(pollfd *&ptr, int i, int fd, int event){
    ptr[i].fd = fd;
    ptr[i].events = event;
}

void info(int num_fds, client *clients){
    if(num_fds<=1)
        std::cout<<"No connections"<<std::endl;
    else{
        std::cout<<"Clients number: "<<num_fds-1<<std::endl;
        for(int i=1;i<num_fds;i++){
            std::cout<<"Client "<<i<<" : "<<clients[i-1].address<<" : "<<clients[i-1].fd<<std::endl;
        }
    }
}

void del_if(pollfd *&polls, int &fds, client *&clients){
    char buf[1];
    for(int i=1;i<fds;i++){
        if((recv(polls[i].fd, buf, sizeof(buf), MSG_DONTWAIT))==0){
            std::cout<<"Host (id:"<<polls[i].fd<<") has hung up!"<<std::endl;
            close(polls[i].fd);
            //pollfd
            polls[i].fd = polls[fds-1].fd;
            //clients
            clients[i-1] = clients[fds-2];
            fds--;
        }
    }
}

int main(){

    //Initiations
    int serverSockfd, clientSockfd;
    int num_fds = 1;
    int yes=1;
    char buf[INET_ADDRSTRLEN];
    socklen_t addr_len = INET_ADDRSTRLEN;
    int pollfd_size = 100;
    struct pollfd *pollfds = new pollfd[pollfd_size];
    struct client *clients = new client[pollfd_size];
    struct sockaddr_in server; struct sockaddr_storage client;

    //setting up basic environment
    setsockopt(serverSockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    getListener(serverSockfd, server);

    pollfds[0].fd = serverSockfd;
    pollfds[0].events = POLLIN;
    std::cout<<"Waiting for connections..."<<std::endl;

    //main loop
    while(1){

        del_if(pollfds, num_fds, clients);

        info(num_fds,clients);

        int res = poll(pollfds, num_fds, -1);
        if(res<0)
            kill("poll()");

        for(int i=0;i<num_fds;i++){
            if(pollfds[i].revents & POLLIN){
                if(pollfds[i].fd == serverSockfd){
                    clientSockfd = accept(serverSockfd, (sockaddr*)&client, &addr_len);
                    if(clientSockfd<0)
                        kill("accept()");
                    std::cout<<"Connection established with: ";
                    inet_ntop(AF_INET, getAddress((sockaddr*)&client), buf, sizeof(buf));
                    std::cout<<buf<<'\n';
                    addFd(pollfds,num_fds,clientSockfd,POLLIN | POLLRDHUP);
                    clients[num_fds-1].address = buf;
                    clients[num_fds-1].fd = clientSockfd;
                    clients[num_fds-1].status = 0;
                    num_fds++;
                }
            }
        }
    }

    return 0;
}

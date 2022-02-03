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
    std::string address;
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

std::string getClientsList(int num, client *clients, int own_fd){
    std::string text;
    if(num<=2){
        text = "No connections\n";
    }
    else{
        for(int i=0;i<num-1;i++){
            if(clients[i].fd != own_fd){
                std::string tmp = "Client: ";
                char tmp2 = i;
                std::string tmp3 = " - ";
                std::string tmp4 = clients[i].address;
                char tmp5 = '\n';
                text += tmp + tmp2 + tmp3 + tmp4 + tmp5;
            }
        }
    }
    return text;
}

char* recv_or_del(pollfd *&polls, int &fds, client *&clients, int i){
    char *buf = new char[32];
    int res = recv(polls[i].fd, buf, sizeof(buf), MSG_DONTWAIT);
    if(res<0)
        kill("recv()");
    if(!res){
        std::cout<<"Host (id:"<<polls[i].fd<<") has hung up!"<<std::endl;
        close(polls[i].fd);
        //pollfd
        polls[i].fd = polls[fds-1].fd;
        //clients
        clients[i-1] = clients[fds-2];
        fds--;
        return 0;
    }
    buf[res] = '\n';
    return buf;
}

int send_preserved(int fd, const char* buff, int &len){
    int bytes_sent = 0;
    int bytes_left = len;
    int res;
    while(bytes_sent<len){
        res = send(fd, buff+bytes_sent, bytes_left, 0);
        bytes_sent+=res;
        bytes_left-=res;
    }
    len = bytes_sent;
    return res==-1?-1:0;
}

void add_to_structs(int fd, char* ip, client *&c, pollfd *&pf, int &numfds){
    pf[numfds].fd = fd;
    pf[numfds].events = POLLIN;
    c[numfds-1].address = ip;
    c[numfds-1].fd = fd;
    c[numfds-1].status = 0;
    numfds++;
}

int main(){

    //Initiations
    int serverSockfd, clientSockfd;
    int num_fds = 1;
    int yes=1;
    /* char buff[32]; */
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

        //check if any host has closed the connection
        /* del_if(pollfds, num_fds, clients); */

        int res = poll(pollfds, num_fds, -1);
        //check if poll is ready to return
        if(res<0)
            kill("poll()");
        char n=0;

        for(int i=0;i<num_fds;i++){
            if(pollfds[i].revents & POLLIN){
                if(pollfds[i].fd == serverSockfd){
                    clientSockfd = accept(serverSockfd, (sockaddr*)&client, &addr_len);
                    if(clientSockfd<0)
                        kill("accept()");

                    char buf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, getAddress((sockaddr*)&client), buf, sizeof(buf));
                    add_to_structs(clientSockfd,buf,clients,pollfds,num_fds);
                    for(int i=0;i<num_fds-1;i++){
                        std::cout<<clients[i].address<<' '<<clients[i].fd<<'\n';
                    }
                    std::string select_info = "\nSelect client you want to message to: \n";
                    std::string clients_info =  getClientsList(num_fds,clients,clientSockfd);
                    std::string mess = clients_info+select_info;
                    int length = mess.length();
                    
                    int res = send_preserved(clientSockfd,mess.c_str(), length);

                    info(num_fds,clients);
                }
                else{
                    char *buff = recv_or_del(pollfds,num_fds,clients,i);
                    if(buff!=0)
                        std::cout<<buff;
                }
            }
        }
    }

    return 0;
}

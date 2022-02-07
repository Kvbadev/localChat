#include <string>
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
        text="No connections!\n";
    }
    else{
        for(int i=0;i<num-1;i++){
            if(clients[i].status == 1){
                std::string tmp;
                if(clients[i].fd==own_fd)
                    tmp = "(You) Client: "+std::to_string(i+1)+" - "+clients[i].address+'\n';
                else
                    tmp = "Client: "+std::to_string(i+1)+" - "+clients[i].address+'\n';
                text+=tmp;
            }
        }
        text+="Select: ";
    }
    return text;
}

int recv_or_del(pollfd *&polls, int &fds, client *&clients, int i, char* mess, int mess_len){
    int res = recv(polls[i].fd, mess, mess_len, 0);
    if(res<0)
        return -1;
    if(!res){
        std::cout<<"Host (id:"<<polls[i].fd<<") has hung up!"<<std::endl;
        close(polls[i].fd);
        polls[i].fd = polls[fds-1].fd;
        clients[i-1] = clients[fds-2];
        fds--;
        return 1;
    }
    return 0;
}

int send_secured(int fd, const char* buff, int len){
    int bytes_sent = 0;
    int bytes_left = len;
    int res;
    while(bytes_sent<len){
        res = send(fd, buff+bytes_sent, bytes_left, 0);
        bytes_sent+=res;
        bytes_left-=res;
    }
    return res==-1?-1:0;
}

void add_to_structs(int fd, std::string ip, client *&c, pollfd *&pf, int &numfds){
    pf[numfds].fd = fd;
    pf[numfds].events = POLLIN;
    c[numfds-1].address = ip;
    c[numfds-1].fd = fd;
    c[numfds-1].status = 1;
    numfds++;
}

std::string print_help(){
    return     "Welcome in easy messeneger local server!\n"
               "To print active clients enter: show\n"
               "To select host to send message to enter: select\n"
               "To print that message enter: help\n";
}

int send_requested(const char *c,int fd, int num, client *clients){
    std::string x = c;
    if(x.substr(0,4)=="help"){
        std::string s = print_help();
        if(send_secured(fd, s.c_str(),sizeof(s))==-1)
            return -1;
    }
    else if(x.substr(0,4)=="show"){
        std::string s = getClientsList(num,clients,fd);   
        if(send_secured(fd, s.c_str(),sizeof(s))==-1)
            return -1;
    }
    return 0;
}

int main(){

    //Initializations
    int serverSockfd, clientSockfd;
    int num_fds = 1;
    int yes=1;
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

        int res = poll(pollfds, num_fds, -1);
        //check if poll is ready to return
        if(res<0)
            kill("poll()");

        for(int i=0;i<num_fds;i++){
            if(pollfds[i].revents & POLLIN){

                if(pollfds[i].fd == serverSockfd){
                    clientSockfd = accept(serverSockfd, (sockaddr*)&client, &addr_len);
                    if(clientSockfd<0)
                        kill("accept()");

                    char buf[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, getAddress((sockaddr*)&client), buf, sizeof(buf));
                    add_to_structs(clientSockfd,buf,clients,pollfds,num_fds);

                    std::string message = getClientsList(num_fds,clients,clientSockfd);
                    
                    //Sending clients list to client
                    int res = send_secured(clientSockfd,message.c_str(),message.length());
                    if(res<0)
                        kill("send_secured()");

                    //display basic info about working server
                    info(num_fds,clients);
                }
                else{
                    //receive message or 1 if host disconnects
                    char buff[32]; 
                    int res = recv_or_del(pollfds,num_fds,clients,i,buff,sizeof(buff));
                    if(!res){
                        int receiver = (int)buff[0]-'0';
                        std::cout<<"Number: "<<receiver<<'\n';
                        send_secured(clients[receiver-1].fd, "test", 5);
                    }
                    else if(res==1)
                        std::cout<<"Host disconnected!"<<std::endl;
                    else
                        kill("recv()");
                        
                }
            }
        }
    }

    delete[] pollfds;
    delete[] clients;

    return 0;
}

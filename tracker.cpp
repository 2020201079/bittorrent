#include<stdio.h>
#include<stdlib.h> //atoi
#include<string.h>
#include<unistd.h>
#include<sys/socket.h> //structs needed for sockets eg sockaddr
#include<sys/types.h> // contains structures needed for internet domain addresses eg sockaddr_in
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<iostream>
#include<vector>
#include<pthread.h>
#include<fstream>
#include<fcntl.h>
#include<unordered_map>

#define MAXDATASIZE 100 // max number of bytes we can get at once

class torFile{
    public:
        std::string fileName;
        std::string peerAddr;
};

class Peer{
        public:
            std::string id;
            std::string password;
            std::string IP;
            std::string port;
            Peer(std::string id,std::string password,std::string ip,std::string port){
                this->id = id;
                this->password = password;
                this->IP = ip;
                this->port = port;
            }
};

std::unordered_map<std::string,Peer*> peerMap;

pthread_mutex_t lockPeerMap;

int makeServer(std::string ip,std::string port){
    int sock_fd;
    struct addrinfo hints, *res;

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM; // for tcp

    if(getaddrinfo(ip.c_str(),port.c_str(),&hints,&res) != 0 ){
        printf("socket failed \n");
        exit(1);
    }

    // make a socket
    sock_fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    if(sock_fd <= 0){
        printf("socket failed \n");
        exit(1);
    }

    if(bind(sock_fd,res->ai_addr,res->ai_addrlen) == -1){
        std::cout<<"sock_fd is :"<<sock_fd<<std::endl;
        std::cout<<"ai_addr is "<<res->ai_addr<<std::endl;
        printf("bind failed \n");
        exit(1);
    }

    freeaddrinfo(res);

    if(listen(sock_fd,10)==-1){ // number of pending connecctions 
        printf("listen failed \n");
        exit(1);
    }

    return sock_fd;
}

int dummySend(int new_fd){
    int dummySize = 10;
    char buf[dummySize] ={0};
    if(send(new_fd,buf,dummySize,0) == -1){ //dummysend
        printf("sendind user_id failed \n");
        close(new_fd);
        exit(1);
    }
    return 0;
}

int dummyRecv(int sock_fd){
    int dummySize = 10;
    char buf[dummySize] = {0}; // dummy recieve
    int numbytes;
    if((numbytes = recv(sock_fd,buf,dummySize,0))==-1){ //dummy read
        printf("error reading data");
        exit(1);
    }
    return 0;
}

std::string getStringFromSocket(int new_fd){ //after recieving also sends a dummySend
    char buf[MAXDATASIZE] = {0};
    int numbytes;

    if((numbytes = recv(new_fd,buf,MAXDATASIZE-1,0))==-1){
        std::cout<<"Error recieving string"<<std::endl;
        exit(1);
    }
    else if(numbytes == 0){
        return "connectionClosedBySender";
    }
    //std::cout<<"Number of bytes recieved "<<numbytes<<std::endl; --> might later use for debugging
    std::string recvString(buf);
    fflush(stdout);
    dummySend(new_fd);
    return recvString;
}

int acceptTorFileFromPeer(int new_fd){

    std::string peerAddr = getStringFromSocket(new_fd);
    std::cout<<"Recieving file from : "<<peerAddr<<std::endl;

    std::string fileName = getStringFromSocket(new_fd);
    std::cout<<"file name :"<<fileName<<std::endl;

    std::string filePath = getStringFromSocket(new_fd);
    std::cout<<"file path :"<<filePath<<std::endl;

    std::string countHashBlocksStr = getStringFromSocket(new_fd);
    std::cout<<"Number of blocks :"<<countHashBlocksStr<<std::endl;
    int countHashBlocks = std::stoi(countHashBlocksStr);

    std::vector<std::pair<int,std::string>> hashBlocks;
    for(int i=0;i<countHashBlocks;i++){
        std::string blockSizeStr = getStringFromSocket(new_fd);
        int blockSize = std::stoi(blockSizeStr);

        std::string hash = getStringFromSocket(new_fd);
        hashBlocks.push_back({blockSize,hash});
    }

    std::cout<<"hash blocks recvd size is: "<<hashBlocks.size()<<std::endl;
    return 0;
}

std::string updatePeerMap(Peer* peer,std::string user_id){
    pthread_mutex_lock(&lockPeerMap);
    std::string status;
    if(peerMap.find(user_id) == peerMap.end()){
        peerMap[user_id] = peer;
        status = "user created";
    }
    else{
        status ="userid exists";
    }
    pthread_mutex_unlock(&lockPeerMap);
    std::cout<<"Number of users :"<<peerMap.size()<<std::endl;
    return status;
}

int create_new_user(int new_fd){

    std::string user_id = getStringFromSocket(new_fd);

    std::string passwd = getStringFromSocket(new_fd);

    std::string ip = getStringFromSocket(new_fd);

    std::string port = getStringFromSocket(new_fd);

    Peer* peer = new Peer(user_id,passwd,ip,port); 
    
    std::string status = updatePeerMap(peer,user_id);
    /*
    std::string status;
    if(peerMap.find(user_id) == peerMap.end()){
        peerMap[user_id] = peer;
        status = "user created";
    }
    else{
        status ="userid exists";
    }*/

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending success signal failed in create user \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of create_user"<<std::endl;
    return 0;
}

void* serviceToPeer(void* i){
    int new_fd = *((int *)i);
    free(i); // this way it ensures that each thread correcd fd is sent

    while(1){
        std::string command = getStringFromSocket(new_fd);
        if(command == "upload_file"){
            std::cout<<" peer wants to upload file "<<std::endl;
            acceptTorFileFromPeer(new_fd);
        }
        else if(command == "create_user"){
            std::cout<<"create user called "<<std::endl;
            //create_user(new_fd);
            create_new_user(new_fd);
        }

        else if(command == "login"){

        }

        else if(command == "connectionClosedBySender"){
            std::cout<<"Connection closed by a peer"<<std::endl;
            break;
        }
        else{
            std::cout<<"Not a valid command"<<std::endl;
        }
    }
    close(new_fd);
}

int main(int argc,char *argv[]){
    if(argc != 2){
        printf("./tracker tracker_info.txt");
        exit(1);
    }

    std::string tracker1,tracker2;
    std::ifstream MyReadFile(argv[1]);

    getline(MyReadFile,tracker1);
    std::string tracker1Port = tracker1.substr(tracker1.find_last_of(":") + 1);
    std::string tracker1IP = tracker1.substr(0,tracker1.find_last_of(":"));

    getline(MyReadFile,tracker2);
    std::string tracker2Port = tracker2.substr(tracker1.find_last_of(":") + 1);
    std::string tracker2IP = tracker2.substr(0,tracker1.find_last_of(":"));

    int sock_fd = makeServer(tracker1IP,tracker1Port);

    int new_fd;
    while(1){
        struct sockaddr_storage client_address;
        socklen_t sin_size = sizeof client_address;
        new_fd = accept(sock_fd,(struct sockaddr *)&client_address,&sin_size);
        if(new_fd == -1){
            perror("accept");
            continue;
        }
        char s[INET6_ADDRSTRLEN];
        inet_ntop(client_address.ss_family,&((struct sockaddr_in *)&client_address)->sin_addr,s,sizeof s);
        printf("Server got connection from %s\n",s);

        //should make another thread here to handle peer request individually
        int* fd =(int *)malloc(sizeof(*fd));
        //check malloc
        if(fd == NULL){
            std::cout<<"Malloc failed for creating thread "<<std::endl;
            close(new_fd);
            exit(1);
        }

        *fd = new_fd;
        pthread_t threadForServiceToPeer;
        pthread_create(&threadForServiceToPeer,NULL,serviceToPeer,fd);

    }
    close(new_fd);
}
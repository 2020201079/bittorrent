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
            std::string groupId;
            Peer(std::string id,std::string password,std::string groupId){
                this->id = id;
                this->password = password;
                this->groupId = groupId;
            }
};

std::vector<Peer> peerList;

std::string getStringFromSocket(int new_fd){
    char buf[MAXDATASIZE] = {0};
    int numbytes;

    if((numbytes = recv(new_fd,buf,MAXDATASIZE-1,0))==-1){
        printf("error recienving string");
        exit(1);
    }

    buf[numbytes] = '\0';
    for(int i=0;i<numbytes;i++){
        std::cout<<buf[i]<<" ";
    }
    std::cout<<std::endl;
    std::string recvString(buf);
    std::cout<<"Num of bytes recvd: "<<numbytes<<std::endl;
    std::cout<<"getString func : "<<recvString<<std::endl;
    fflush(stdout);
    return recvString;
}

int dummySend(int new_fd){
    int dummySize = 10;
    char buf[dummySize] ={0};
    if(send(new_fd,buf,dummySize,0) == -1){ //dummysend
        printf("sendind user_id failed \n");
        close(new_fd);
        exit(1);
    }
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
    fflush(stdout);
    return 0;
}

int create_new_user(int new_fd){

    dummySend(new_fd);

    std::string user_id = getStringFromSocket(new_fd);
    std::cout<<"user_id : "<<user_id<<std::endl;

    dummySend(new_fd);

    std::string passwd = getStringFromSocket(new_fd);
    std::cout<<"passwd: "<<passwd<<std::endl;

    dummySend(new_fd);

    Peer peer = Peer(user_id,passwd,"-1"); // setting group as -1 for now
    peerList.push_back(peer);
    std::cout<<"end of create_user"<<std::endl;
    fflush(stdout);
    return 0;
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

    int sock_fd,new_fd;
    struct addrinfo hints, *res;

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM; // for tcp


    if(getaddrinfo(tracker1IP.c_str(),tracker1Port.c_str(),&hints,&res) != 0 ){
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

    if(listen(sock_fd,10)==-1){
        printf("listen failed \n");
        exit(1);
    }

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

        std::string command = getStringFromSocket(new_fd);
        std::cout<<command<<std::endl;
        if(command == "upload_file"){
            std::cout<<" peer wants to upload file "<<std::endl;
            acceptTorFileFromPeer(new_fd);
        }
        else if(command == "create_user"){
            std::cout<<"create user called "<<std::endl;
            //create_user(new_fd);
            create_new_user(new_fd);
        }
        else{
            std::cout<<"Tracker got unknown request"<<std::endl;
        }

    }
    close(new_fd);
}


/*
now want to send this file name, file path, hash , size of file, number of chunks  to tracker

on recieving share request

should get the following details

1) file name
2) file path
3) hash
4) size of file -> can calculate the chunks and distribution
*/
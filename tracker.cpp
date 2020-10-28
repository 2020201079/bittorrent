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

#define MAXDATASIZE 300 // max number of bytes we can get at once

std::string getStringFromSocket(int new_fd){

    char buf[MAXDATASIZE];
    int numbytes;
    if((numbytes = recv(new_fd,buf,MAXDATASIZE-1,0))==-1){
        printf("error recienving string");
        exit(1);
    }
    
    buf[numbytes] = '\0';
    std::string recvString(buf);
    std::cout<<recvString<<std::endl;
    return recvString;
}

int acceptTorFileFromPeer(int new_fd){

    std::string fileName = getStringFromSocket(new_fd);
    std::cout<<fileName<<std::endl;

    std::string filePath = getStringFromSocket(new_fd);
    std::cout<<filePath<<std::endl;

    std::string countHashBlocksStr = getStringFromSocket(new_fd);
    std::cout<<"Number of blocks "<<countHashBlocksStr<<std::endl;
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

int main(int argc,char *argv[]){
    int sock_fd,new_fd;
    struct addrinfo hints, *res;

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM; // for tcp


    if(getaddrinfo("127.0.0.1","3600",&hints,&res) != 0 ){
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
        printf("bind failed dmm it\n");
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
            perror("accet");
            continue;
        }
        char s[INET6_ADDRSTRLEN];
        inet_ntop(client_address.ss_family,&((struct sockaddr_in *)&client_address)->sin_addr,s,sizeof s);
        printf("Server got connection from %s\n",s);

        acceptTorFileFromPeer(new_fd);

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
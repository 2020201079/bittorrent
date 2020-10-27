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

#define MAXDATASIZE 300 // max number of bytes we can get at once

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

        char bufFileName[MAXDATASIZE];
        int numbytesFileName;
        if((numbytesFileName = recv(new_fd,bufFileName,MAXDATASIZE-1,0))==-1){
            printf("error recienving file name");
            exit(1);
        }
        
        bufFileName[numbytesFileName] = '\0';
        std::string filename(bufFileName);
        printf("tracker recieved file name %s\n",bufFileName);
        std::cout<<filename<<std::endl;
        //free(bufFileName);

        char bufFilePath[MAXDATASIZE];
        int numbytesFilePath;
        if((numbytesFilePath = recv(new_fd,bufFilePath,MAXDATASIZE-1,0))==-1){
            printf("error recienving file path");
            exit(1);
        }

        bufFilePath[numbytesFilePath] = '\0';
        std::string filePath(bufFilePath);
        printf("tracker recieved file path %s",bufFilePath);
        std::cout<<filePath<<std::endl;

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
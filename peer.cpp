#include<string>
#include<sys/stat.h>
#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<openssl/sha.h>
#include<iostream>
#include<arpa/inet.h>
#include<netdb.h>


int blockSize = 512*1024;
int hashOutputSize = 20;

std::string getHashOfFile(std::string filePath){
    // open the file fd is file descriptor
    int fd = open(filePath.c_str(),O_RDONLY);
    if(fd==-1){
        perror("Error openning file ");
    }

    //getting size of the file
    struct stat fileStats;
    if(fstat(fd,&fileStats) == -1){
        perror("Error getting file stats ");
    }
    int fileSize = fileStats.st_size;

    //need to calculate number of blocks in file
    int numberOfBlocks;
    int sizeOfLastBlock;
    if(fileSize%blockSize == 0){
        numberOfBlocks = fileSize/blockSize;
        sizeOfLastBlock = 0;
    }
    else{
        numberOfBlocks = (fileSize/blockSize) +1;
        sizeOfLastBlock = fileSize%blockSize;
    }

    //calculate hash for each block and keep appending it to hashString
    std::string hashString = "";
    for(int i=0;i<numberOfBlocks;i++){
        if(i==numberOfBlocks-1){
            void *buf = malloc(sizeOfLastBlock);
            read(fd,buf,sizeOfLastBlock);
            unsigned char obuf[hashOutputSize];
            SHA1((unsigned char*)buf,sizeOfLastBlock,obuf);
            for(int i=0;i<20;i++){
                hashString += std::to_string((int) obuf[i]);
            }
        }
        else{
            void* buf = malloc(blockSize);
            read(fd,buf,blockSize);
            unsigned char obuf[hashOutputSize];
            SHA1((unsigned char*)buf,blockSize,obuf);
            for(int i=0;i<20;i++){
                hashString += std::to_string((int) obuf[i]);
            }
        }
    }
    close(fd);

    return hashString;
}

int upload_file(std::string filePath){
    
}
int main(int argc,char* argv[]){
    if(argc != 3){
        printf("./peer.c address portno");
        exit(1);
    }
    char* trackerIP = argv[1];
    char* portOfTracker = argv[2];
    
    //need to divide file into size of 512 kb and find hash of each part to get the complete string
    std::string filePath = "test1800K.txt";
    std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    std::string hashString = getHashOfFile(filePath);
    std::cout<<"hash string size :"<<hashString.size()<<std::endl;
    std::cout<<hashString<<std::endl;

    /*now want to send this file name, file path, hash , size of file, number of chunks  to tracker
    // things to share with tracker
    1) file name
    2) file path
    3) hash
    4) size of file -> can calculate the chunks and distribution
    5) Port on which client is listenning for other peers to download
    6) 
    */
    int sock_fd,new_fd;
    struct addrinfo  hints,*res;

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM; // for tcp

    if(getaddrinfo(trackerIP,portOfTracker,&hints,&res) != 0){
        printf("Get addr info failed \n");
        exit(1);
    }
    /*
    if(sock_fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol) == -1){
        printf("socket failed \n");
        exit(1);
    }*/

    sock_fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

    printf(" sock_fd is %d\n",sock_fd); 
    if(connect(sock_fd,res->ai_addr,res->ai_addrlen) == -1){
        close(sock_fd);
        printf("connect failed \n");
        exit(1);
    }
    
    
    char s[INET6_ADDRSTRLEN];
    inet_ntop(res->ai_family,&((struct sockaddr_in *)res->ai_addr)->sin_addr,s,sizeof s);
    printf("peer connected to tracker %s\n",s);

    freeaddrinfo(res); // no need of this struct after connecting

    //send fileName
    if (send(sock_fd,fileName.c_str(),fileName.size(),0) == -1){
        printf("sendind file name failed \n");
        close(sock_fd);
        exit(1);
    }

    // send filePath
    if (send(sock_fd,filePath.c_str(),filePath.size(),0) == -1){
        printf("sendind file path failed \n");
        close(sock_fd);
        exit(1);
    }

    close(sock_fd);

    return 0;

}
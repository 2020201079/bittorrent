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
#include<vector>
#include<pthread.h>
#include<fstream>


int blockSize = 512*1024;
int hashOutputSize = 20;
std::string portNoToShareFiles;

int makeConnectionToTracker(const char* trackerIP,const char* portOfTracker){
    std::cout<<"port of tracker is "<<std::string(portOfTracker)<<std::endl;
    int sock_fd,new_fd;
    struct addrinfo  hints,*res;

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM; // for tcp

    if(getaddrinfo(trackerIP,portOfTracker,&hints,&res) != 0){
        std::cout<<std::string(trackerIP)<<std::endl;
        std::cout<<std::string(trackerIP)<<std::endl;
        printf("Get addr info failed \n");
        exit(1);
    }

    sock_fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

    if(connect(sock_fd,res->ai_addr,res->ai_addrlen) == -1){
        close(sock_fd);
        printf("connect failed \n");
        exit(1);
    }

    char s[INET6_ADDRSTRLEN];
    inet_ntop(res->ai_family,&((struct sockaddr_in *)res->ai_addr)->sin_addr,s,sizeof s);
    printf("peer connected to tracker %s\n",s);

    freeaddrinfo(res); // no need of this struct after connecting

    return sock_fd;
}


std::vector<std::pair<int,std::string>> getHashOfFile(std::string filePath){
    std::vector<std::pair<int,std::string>> hashBlocks(0);
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
            std::string currHashString = "";
            for(int i=0;i<20;i++){
                currHashString += std::to_string((int) obuf[i]);
                hashString += std::to_string((int) obuf[i]);
            }
            hashBlocks.push_back({sizeOfLastBlock,currHashString});
            free(buf);
        }
        else{
            void* buf = malloc(blockSize);
            read(fd,buf,blockSize);
            unsigned char obuf[hashOutputSize];
            SHA1((unsigned char*)buf,blockSize,obuf);
            std::string currHashString = "";
            for(int i=0;i<20;i++){
                currHashString += std::to_string((int) obuf[i]);
                hashString += std::to_string((int) obuf[i]);
            }
            hashBlocks.push_back({blockSize,currHashString});
            free(buf);
        }
    }
    close(fd);

    return hashBlocks;
}

int upload_file(std::string filePath,const char* trackerIP,const char* portOfTracker,std::string peerAddr){
    std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    std::vector<std::pair<int,std::string>> hashBlocks = getHashOfFile(filePath);
    int noOfBlocks = hashBlocks.size();

    int sock_fd = makeConnectionToTracker(trackerIP,portOfTracker);
    //send command
    if (send(sock_fd,"upload_file",sizeof "upload_file",0) == -1){
        printf("sendind command upload_file failed \n");
        close(sock_fd);
        exit(1);
    }

    //send peerAddress
    if (send(sock_fd,peerAddr.c_str(),peerAddr.size(),0) == -1){
        printf("sendind peer addr failed \n");
        close(sock_fd);
        exit(1);
    }

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

    //send number of blocks
    if (send(sock_fd,std::to_string(noOfBlocks).c_str(),std::to_string(noOfBlocks).size(),0) == -1){
        printf("sendind number of blocks failed \n");
        close(sock_fd);
        exit(1);
    }

    for(auto p : hashBlocks){
        if (send(sock_fd,std::to_string(p.first).c_str(),std::to_string(p.first).size(),0) == -1){
            printf("sendind the block size failed \n");
            close(sock_fd);
            exit(1);
        }
        if (send(sock_fd,p.second.c_str(),p.second.size(),0) == -1){
            printf("sendind a hash block failed \n");
            close(sock_fd);
            exit(1);
        }
    }

    close(sock_fd);
    std::cout<<"file shared"<<std::endl;
    return 0;
}

int sendString(std::string msg,int sock_fd){
    //always sending strings of length 100 atleast
    int packetSize = 100; // this is same in both peer and tracker
    char buf[packetSize] = {0};
    for(int i=0;i<packetSize;i++){
        buf[i] = msg[i];
    }
    size_t total =0;
    int len = packetSize;
    while (total != len){
        int nb = send(sock_fd,buf+total,len-total,0);
        if(nb == -1){
            std::cout<<" sending failed"<<std::endl;
            exit(1);
        }
        total += nb;
    }
}

int create_user(const char* trackerIP,const char* portOfTracker,std::string user_id,std::string passwd){
    std::string filePath = "test.txt";
    //std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);
    //std::vector<std::pair<int,std::string>> hashBlocks = getHashOfFile(filePath);
    //int fd = open(filePath.c_str(),O_RDONLY);
    //int noOfBlocks = hashBlocks.size();

    int sock_fd = makeConnectionToTracker(trackerIP,portOfTracker);
    std::cout<<"sock_fd : "<<sock_fd<<std::endl;
    //send command

    //int status = sendString("create_user",sock_fd);

    
    if (send(sock_fd,"create_user",sizeof "create_user",0) == -1){
        printf("sending command create_user failed \n");
        close(sock_fd);
        exit(1);
    }

    int dummySize = 10;
    char buf[dummySize] = {0}; // dummy recieve
    int numbytes;
    if((numbytes = recv(sock_fd,buf,dummySize,0))==-1){ //dummy read
        printf("error reading data");
        exit(1);
    }

    //sendString(user_id,sock_fd);
    if(send(sock_fd,user_id.c_str(),user_id.size(),0) == -1){
        printf("sendind user_id failed \n");
        close(sock_fd);
        exit(1);
    }

    if((numbytes = recv(sock_fd,buf,dummySize,0))==-1){ //dummy read
        printf("error reading data");
        exit(1);
    }

    //send fileName
    //sendString(passwd,sock_fd);
    if (send(sock_fd,passwd.c_str(),passwd.size(),0) == -1){
        printf("sendind passwd failed \n");
        close(sock_fd);
        exit(1);
    }

    if((numbytes = recv(sock_fd,buf,dummySize,0))==-1){ //dummy read
        printf("error reading data");
        exit(1);
    }

    close(sock_fd);
    std::cout<<"user created"<<std::endl;
    return 0;
}


int main(int argc,char* argv[]){
    if(argc != 3){
        printf("./peer.c address:portno tracker_info.txt");
        exit(1);
    }
    std::string tracker1,tracker2;
    std::ifstream MyReadFile(argv[2]);
    std::cout<<"Going to call getline"<<std::endl;

    getline(MyReadFile,tracker1);
    std::string tracker1Port = tracker1.substr(tracker1.find_last_of(":") + 1);
    std::string tracker1IP = tracker1.substr(0,tracker1.find_last_of(":"));

    getline(MyReadFile,tracker2);
    std::string tracker2Port = tracker2.substr(tracker1.find_last_of(":") + 1);
    std::string tracker2IP = tracker2.substr(0,tracker1.find_last_of(":"));

    std::string peerAddr = argv[1];
    std::string peerPort = peerAddr.substr(peerAddr.find_last_of(":")+1);
    std::string peerIP = peerAddr.substr(0,peerAddr.find_last_of(":"));
    std::string portNoToShareFiles = peerPort;

    pthread_t threadToSendFile;
    //pthread_create(&threadToSendFile,NULL,fileSharer,NULL);

    std::cout<<"Thread created for listenning "<<std::endl;

    while(1){
        std::string command;std::cin>>command;
        std::cout<<"command is : "<<command<<std::endl;
        if(command == "upload_file"){
            std::string filePath;
            std::cin>>filePath;
            int groupid;std::cin>>groupid;
            upload_file(filePath,tracker1IP.c_str(),tracker1Port.c_str(),peerAddr);
        }

        else if(command == "download_file"){
            std::cout<<"called download file"<<std::endl;
        }

        else if(command == "create_user"){
            std::string user_id;std::cin>>user_id;
            std::string passwd; std::cin>>passwd;
            create_user(tracker1IP.c_str(),tracker1Port.c_str(),user_id,passwd);
        }

        else{
            std::cout<<"Not a valid command "<<std::endl;
        }
    }
    return 0;

}

    /*now want to send this file name, file path, hash , size of file, number of chunks  to tracker
    // things to share with tracker
    1) file name
    2) file path
    3) hash
    4) size of file -> can calculate the chunks and distribution
    5) Port on which client is listenning for other peers to download
    6) 
    */
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

#define MAXDATASIZE 100 // for passing strings as commands

int blockSize = 512*1024;
int hashOutputSize = 20;
std::string portNoToShareFiles;
std::string IPTolisten;
std::string delim = ">>=";

class PeerInfo{
    public:
        std::string user_id;
        std::string destinationPath;
        std::string address; // ip::port --> format
        PeerInfo(std::string user_id,std::string address,std::string filePath){
            this->user_id = user_id;
            this->destinationPath = filePath;
            this->address = address;
        }
};

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

std::string getStringFromSocket(int new_fd){ //after recieving also sends a dummySend
    char buf[MAXDATASIZE] = {0};
    int numbytes;

    if((numbytes = recv(new_fd,buf,MAXDATASIZE-1,0))==-1){
        printf("error recienving string");
        exit(1);
    }
    std::string recvString(buf);
    fflush(stdout);
    dummySend(new_fd);
    return recvString;
}

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

int upload_file(int sock_fd,std::string filePath,std::string group_id){
    std::string commandToSend = "upload_file";
    commandToSend.append(delim).append(filePath).append(delim).append(group_id);

    if (send(sock_fd,commandToSend.c_str(),commandToSend.length(),0) == -1){
        printf("sending command login failed \n");
        close(sock_fd);
        exit(1);
    }

    dummyRecv(sock_fd);

    //now have to send size+hashBlocks
    std::vector<std::pair<int,std::string>> hashBlocks = getHashOfFile(filePath); //uncessarily added int 
    int noOfBlocks = hashBlocks.size();
    int sizeOfFile = 0;
    for(auto p:hashBlocks)
        sizeOfFile += p.first;
    
    std::string sizePlusNoOfBlocks = std::to_string(sizeOfFile).append(delim).append(std::to_string(noOfBlocks));
    if (send(sock_fd,sizePlusNoOfBlocks.c_str(),sizePlusNoOfBlocks.length(),0) == -1){
        printf("sending size + hash failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);
    
    for(auto h: hashBlocks){
        if (send(sock_fd,h.second.c_str(),h.second.length(),0) == -1){
        printf("sending size + hash failed \n");
        close(sock_fd);
        exit(1);
        }
        dummyRecv(sock_fd);
        std::cout<<h.second<<std::endl;
    }

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;
}

int logout(int sock_fd){
    if (send(sock_fd,"logout",sizeof "login",0) == -1){
        printf("sending command logout failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;
}

int login(int sock_fd,std::string user_id,std::string passwd){
    std::string commandToSend = "login";
    commandToSend.append(delim).append(user_id).append(delim).append(passwd);

    if (send(sock_fd,commandToSend.c_str(),commandToSend.length(),0) == -1){
        printf("sending command login failed \n");
        close(sock_fd);
        exit(1);
    }

    dummyRecv(sock_fd);

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;
}

int join_group(int sock_fd,std::string group_id){
    std::string command = "join_group";
    command.append(delim).append(group_id);
    
    if (send(sock_fd,command.c_str(),command.size(),0) == -1){
        printf("sending command join group failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;
}

int create_group(int sock_fd,std::string group_id){
    std::string commandToSend = "create_group";
    commandToSend.append(delim).append(group_id);
    
    if (send(sock_fd,commandToSend.c_str(),commandToSend.length(),0) == -1){
        printf("sending command create_group failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;
}

int list_groups(int sock_fd){
    std::string commandToSend = "list_groups";

    if (send(sock_fd,commandToSend.c_str(),commandToSend.length(),0) == -1){
        printf("sending command create_group failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;
}

int create_user(int sock_fd,std::string user_id,std::string passwd){
    std::string commandToSend = "create_user";
    commandToSend.append(delim).append(user_id).append(delim).append(passwd).append(delim)\
    .append(IPTolisten).append(delim).append(portNoToShareFiles);

    if(send(sock_fd,commandToSend.c_str(),commandToSend.size(),0) == -1){
        printf("sending create_user command to client failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;
}

std::vector<PeerInfo> getPeersWithFile(int sock_fd,std::string group_id,std::string fileName){ //need to run this in a thread
    std::string commandToSend = "getPeersWithFile";
    commandToSend.append(delim).append(group_id).append(delim).append(fileName);

    std::cout<<"command send:"<<commandToSend<<std::endl;

    if(send(sock_fd,commandToSend.c_str(),commandToSend.size(),0) == -1){
        printf("sending get peer with file command to tracker failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    int noOfPeers = std::stoi(getStringFromSocket(sock_fd));
    std::cout<<"Number of peers with "<< fileName <<" is "<<noOfPeers<<std::endl;
    std::vector<PeerInfo> peerList;
    for(int i=0;i<noOfPeers;i++){
        std::string user_id = getStringFromSocket(sock_fd);
        std::string address = getStringFromSocket(sock_fd);
        std::string filePath = getStringFromSocket(sock_fd);
        peerList.push_back(PeerInfo(user_id,address,filePath));
    }

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return peerList;
}
int list_files(int sock_fd,std::string group_id){
    std::string commandToSend = "list_files";
    commandToSend.append(delim).append(group_id);

    if(send(sock_fd,commandToSend.c_str(),commandToSend.size(),0) == -1){
        printf("sending create_user command to client failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    int noOfFiles = std::stoi(getStringFromSocket(sock_fd));
    std::vector<std::string> fileNames;
    std::cout<<"Number of files in group is "<<noOfFiles<<std::endl;
    for(int i=0;i<noOfFiles;i++){
        fileNames.push_back(getStringFromSocket(sock_fd));
    }
    for(auto fn : fileNames){
        std::cout<<fn<<std::endl;
    }
    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;

}

int accept_request(int sock_fd,std::string group_id,std::string user_id){
    std::string commandToSend = "accept_request";
    commandToSend.append(delim).append(group_id).append(delim).append(user_id);

    if(send(sock_fd,commandToSend.c_str(),commandToSend.size(),0) == -1){
        printf("sending create_user command to client failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;
}

int list_requests(int sock_fd,std::string group_id){
    std::string commandToSend = "list_requests";
    commandToSend.append(delim).append(group_id);

    if(send(sock_fd,commandToSend.c_str(),commandToSend.size(),0) == -1){
        printf("sending create_user command to client failed \n");
        close(sock_fd);
        exit(1);
    }
    dummyRecv(sock_fd);

    std::string status = getStringFromSocket(sock_fd);
    std::cout<<status<<std::endl;
    return 0;

}

void* fileSharer(void* argv){
    int sock_fd = makeServer(IPTolisten,portNoToShareFiles);
    while(1){
        int new_fd;
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
        
        std::cout<<getStringFromSocket(new_fd)<<std::endl;
        dummySend(new_fd); 
        fflush(stdout);
    }
}

int main(int argc,char* argv[]){
    if(argc != 3){
        printf("./peer.c address:portno tracker_info.txt");
        exit(1);
    }
    std::string tracker1,tracker2;
    std::ifstream MyReadFile(argv[2]);

    getline(MyReadFile,tracker1);
    std::string tracker1Port = tracker1.substr(tracker1.find_last_of(":") + 1);
    std::string tracker1IP = tracker1.substr(0,tracker1.find_last_of(":"));

    getline(MyReadFile,tracker2);
    std::string tracker2Port = tracker2.substr(tracker1.find_last_of(":") + 1);
    std::string tracker2IP = tracker2.substr(0,tracker1.find_last_of(":"));

    std::string peerAddr = argv[1];
    std::string peerPort = peerAddr.substr(peerAddr.find_last_of(":")+1);
    std::string peerIP = peerAddr.substr(0,peerAddr.find_last_of(":"));
    portNoToShareFiles = peerPort;
    IPTolisten = peerIP;

    pthread_t threadToSendFile;
    pthread_create(&threadToSendFile,NULL,fileSharer,NULL);

    std::cout<<"Thread created for listenning "<<std::endl;
    int sock_fd = makeConnectionToTracker(tracker1IP.c_str(),tracker1Port.c_str());
    while(1){
        std::string command;std::cin>>command;
        std::cout<<"Command is "<<command<<std::endl;
        if(command == "upload_file"){
            std::string filePath;
            std::cin>>filePath;
            std::string group_id;std::cin>>group_id;
            int fd = open(filePath.c_str(),O_RDONLY);
            if(fd==-1){
                perror("Error openning file ");
            }
            else
                upload_file(sock_fd,filePath,group_id);
        }

        else if(command == "create_user"){
            std::string user_id;std::cin>>user_id;
            std::string passwd; std::cin>>passwd;
            create_user(sock_fd,user_id,passwd);
        }

        else if(command == "create_group"){
            std::string group_id;std::cin>>group_id;
            create_group(sock_fd,group_id);
        }
        else if(command =="join_group"){
            std::string group_id;std::cin>>group_id;
            join_group(sock_fd,group_id);    
        }
        else if(command == "login"){
            std::string user_id;std::cin>>user_id;
            std::string passwd; std::cin>>passwd;
            login(sock_fd,user_id,passwd);
        }

        else if(command =="logout"){
            logout(sock_fd);
        }

        else if(command =="list_requests"){
            std::string group_id;std::cin>>group_id;
            list_requests(sock_fd,group_id);
        }

        else if(command == "accept_request"){
            std::string group_id,user_id;std::cin>>group_id>>user_id;
            accept_request(sock_fd,group_id,user_id);   
        }

        else if(command == "list_groups"){
            list_groups(sock_fd);
        }

        else if(command == "list_files"){
            std::string group_id;std::cin>>group_id;
            list_files(sock_fd,group_id);
        }

        else if(command == "connect"){ // just for testing purposes
            std::string IP,port;std::cin>>IP>>port;
            int sock_fd = makeConnectionToTracker(IP.c_str(),port.c_str());
            if(send(sock_fd,"hello thread",sizeof "hello thread",0) == -1){
                printf("sending command create_user failed \n");
                close(sock_fd);
                exit(1);
            }
            dummyRecv(sock_fd);
        }

        else if(command == "download_file"){
            std::string group_id,file_name,destinationPath;std::cin>>group_id>>file_name>>destinationPath;
            std::vector<PeerInfo> peerList = getPeersWithFile(sock_fd,group_id,file_name);
            for(auto p:peerList)
                std::cout<<p.user_id<<std::endl;
            // should download file from each peer later
        }

        else{
            std::cout<<"Not a valid command "<<std::endl;
            std::cin.clear();
            std::cout.clear();
        }
    }
    return 0;
}
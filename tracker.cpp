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
            bool loggedIn = false;
            Peer(std::string id,std::string password,std::string ip,std::string port){
                this->id = id;
                this->password = password;
                this->IP = ip;
                this->port = port;
            }
};

class LocationInPeer{
    public:
        std::string user_id;
        std::string filePathInPeer;
};

class FileMetaData{
    public:
        std::string fileName; 
        std::string hash; // 
        std::vector<LocationInPeer> clients;
};

class Group{
    public:
        std::string id;
        std::string ownwer;
        std::vector<std::string> members;
        //filesName -> FileMetaData
        std::unordered_map<std::string,FileMetaData*> filesShared; 
};

std::unordered_map<std::string,Peer*> peerMap;

std::unordered_map<std::string,Group*> groupMap;

pthread_mutex_t lockPeerMap;
pthread_mutex_t lockGroupMap;

std::string delim = ">>=";


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
        free(peer);
    }
    pthread_mutex_unlock(&lockPeerMap);
    std::cout<<"Number of users :"<<peerMap.size()<<std::endl;
    return status;
}

std::string updateGroupMap(Group* group,std::string group_id){
    pthread_mutex_lock(&lockGroupMap);
    std::string status;
    if(groupMap.find(group_id) == groupMap.end()){
        groupMap[group_id] = group;
        status = "group created";
    }
    else{
        status ="group exists";
        free(group);
    }
    pthread_mutex_unlock(&lockGroupMap);
    std::cout<<"Number of groups :"<<groupMap.size()<<std::endl;
    return status;
}

int updatePeerMapLoginStatus(std::string user_id,bool b){
    pthread_mutex_lock(&lockPeerMap);
    peerMap[user_id]->loggedIn = b;
    pthread_mutex_unlock(&lockPeerMap);
    return 0;
}

int create_group(int new_fd,std::string user_id){
    std::string group_id = getStringFromSocket(new_fd);
    std::cout<<"group id :"<<group_id<<std::endl;

    Group* group = new Group();
    group->id = group_id;
    group->ownwer = user_id;
    group->members.push_back(user_id);

    std::string status;
    if(user_id==""){
        status = "First login to create group";
        free(group);
    }
    else{
        status = updateGroupMap(group,group_id);
        std::cout<<"status :"<<status<<std::endl;
    }

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending success signal failed in create group \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of create_group"<<std::endl;
    return 0;
}

int create_user(int new_fd,std::string user_id,std::string passwd,std::string IP,std::string port){
    Peer* peer = new Peer(user_id,passwd,IP,port);

    std::string status = updatePeerMap(peer,user_id);
    std::cout<<"status :"<<status<<std::endl;

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending success signal failed in create user \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of create_user"<<std::endl;
    return 0;
}

int logout(int new_fd,std::string user_id){
    updatePeerMapLoginStatus(user_id,false);
    std::string retStrPeer = "Logged out ";
    if(send(new_fd,retStrPeer.c_str(),retStrPeer.size(),0) == -1){
        printf("sending status signal failed in login \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    return 0;
}

/*std::string  join_group(std::string group_id,std::string user_id){
    std::string status;
    if(user_id == ""){
        status = "Need to login first";
    }
    else if(){

    }
}*/
std::string login(int new_fd,std::string user_id,std::string passwd,std::string& currUser){ // return user_id if success or nullstring
    std::string retStrPeer = "";
    if(peerMap.find(user_id) == peerMap.end()){
        retStrPeer = "User does not exists create one";
    }
    else if(peerMap[user_id]->password != passwd){
        retStrPeer = "Wrong Password";
    }
    else{
        retStrPeer = "login success";
        updatePeerMapLoginStatus(user_id,true);
        currUser = user_id;
    }

    if(send(new_fd,retStrPeer.c_str(),retStrPeer.size(),0) == -1){
        printf("sending status signal failed in login \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    return retStrPeer;
} 

std::vector<std::string> getTokens(std::string input){
    std::vector<std::string> result;
    
    size_t pos = 0;
    std::string token;
    while((pos = input.find(delim))!=std::string::npos){
        token = input.substr(0,pos);
        result.push_back(token);
        input.erase(0,pos+delim.length());
    }
    result.push_back(input);
    return result;
}

void* serviceToPeer(void* i){ //this runs in a separate thread
    int new_fd = *((int *)i);
    free(i); // this way it ensures that each thread correcd fd is sent
    std::string currUser = ""; // wanna save this here because want to know to which id this thread is assigned to

    while(1){
        std::string stringFromPeer = getStringFromSocket(new_fd);
        std::vector<std::string> tokens = getTokens(stringFromPeer);
        std::string command = tokens[0];
        std::cout<<"command is "<<command<<std::endl;
        
        if(command == "upload_file"){
            std::cout<<" peer wants to upload file "<<std::endl;
            acceptTorFileFromPeer(new_fd);
        }
        else if(command == "create_user"){
            if(tokens.size() != 5){
                std::cout<<"wrong format for create user "<<std::endl;
            }
            else{
                std::cout<<"create user called "<<std::endl;
                create_user(new_fd,tokens[1],tokens[2],tokens[3],tokens[4]);
            }
        }

        else if(command == "login"){
            if(tokens.size() != 3){
                std::cout<<"wrong format for login "<<std::endl;
            }
            else{
                if(currUser != ""){
                    std::cout<<"Already logged in as :"<<currUser;
                }
                else{
                    std::string status = login(new_fd,tokens[1],tokens[2],currUser);
                    std::cout<<status<<std::endl;
                }
            }
        }

        else if(command == "logout"){
            if(tokens.size() != 1){
                std::cout<<"wrong format for logout "<<std::endl;
            }
            else{
            if(currUser == "")
                std::cout<<"Didn't login to logout :"<<currUser;
            else{
                int status = logout(new_fd,currUser);
                if(status == 0){
                    std::cout<<"Logged out as : "<<currUser<<std::endl;
                    currUser = "";
                    }
                }
            }
        }

        else if(command == "create_group"){
            std::cout<<"create group called "<<std::endl;
            create_group(new_fd,currUser);
        }

        else if(command == "join_group"){
            if(tokens.size() != 2 ){
                std::cout<<"wrong format for join group "<<std::endl;
            }
            //join_group(tokens[1],user_id);// send token 1 here
            //lets implement later
        }

        else if(command == "connectionClosedBySender"){
            std::cout<<"Connection closed by a peer"<<std::endl;
            break;
        }
        else{
            std::cout<<"Not a valid command"<<std::endl;
            std::string ignore;
            std::getline(std::cin,ignore);
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
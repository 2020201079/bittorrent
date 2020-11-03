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
#include<algorithm>

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
            std::vector<std::pair<std::string,std::string>> groupJoinRequest; //(group_id,user_id) this user is owner
};

class PeerInfo{ // this object is in FileMetaData
    public:
        std::string user_id;
        std::string filePathInPeer;
};

class FileMetaData{
    public:
        std::string fileName;
        std::string fileSize;
        std::vector<std::string> hashOfChunks;
        std::vector<PeerInfo> clientsHavingThisFile;
};

class Group{
    private:
        pthread_mutex_t lockGroupFilesShared;
    public:
        std::string id;
        std::string ownwer;
        std::vector<std::string> members;
        //filesName -> FileMetaData
        std::unordered_map<std::string,FileMetaData*> filesShared;  //fileName,FileMetaData
        void updatePeerListInGroup(std::string fileName,PeerInfo peer){
            pthread_mutex_lock(&lockGroupFilesShared);
            this->filesShared[fileName]->clientsHavingThisFile.push_back(peer);
            pthread_mutex_unlock(&lockGroupFilesShared);
        };

        bool peerPresentInList(std::string fileName,std::string user_id){
            if(filesShared.find(fileName) == filesShared.end()){
                std::cout<<"filename is not present in group "<<id<<std::endl;
                return false;
            }
            else{
                for(auto p : filesShared[fileName]->clientsHavingThisFile){
                    if(p.user_id == user_id)
                        return true;
                }
                return false;
            }
        }
};

std::unordered_map<std::string,Peer*> peerMap;

std::unordered_map<std::string,Group*> groupMap;

pthread_mutex_t lockPeerMap;
pthread_mutex_t lockGroupMap;

std::string delim = ">>=";

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

int addGroupJoinReqToPeer(std::string owner,std::string user_id,std::string group_id){
    pthread_mutex_lock(&lockPeerMap);
    peerMap[owner]->groupJoinRequest.push_back({group_id,user_id});
    pthread_mutex_unlock(&lockPeerMap);
    std::cout<<"Added group join req to "<<owner<<" to allow "<<user_id<<" to join group "<<group_id<<std::endl;
    return 0;
}

int removeGroupJoinReq(std::string owner,std::string group_id,std::string user_id){
    pthread_mutex_lock(&lockPeerMap);
    auto& v = peerMap[owner]->groupJoinRequest;
    auto position = std::find(v.begin(),v.end(),std::make_pair(group_id,user_id));
    if(position != v.end()){
        v.erase(position);
    }
    pthread_mutex_unlock(&lockPeerMap);
    std::cout<<"removed join req "<<std::endl;
    return 0;
}

int addMemberToGroup(std::string user_id,std::string group_id){
    pthread_mutex_lock(&lockGroupMap);
    auto group = groupMap[group_id];
    group->members.push_back(user_id);
    pthread_mutex_unlock(&lockGroupMap);
    std::cout<<"Members in group :"<<group_id<<std::endl;
    for(auto m:group->members) //printing for verifying
        std::cout<<m<<std::endl;
    return 0;
}

bool isHashEqual(std::vector<std::string> h1,std::vector<std::string> h2){
    if(h1.size() != h2.size())
        return false;
    else{
        for(int i=0;i<h1.size();i++){
            if(h1[i] != h2[i])
                return false;
        }
        return true;
    }
}

void upload_file_exitHelper(int new_fd){
    //this part is added so that whatever peer sends get synnced with recv for later
    std::string sizePlusNoOfHashes = getStringFromSocket(new_fd);
    std::vector<std::string> tokens = getTokens(sizePlusNoOfHashes);
    std::string size = tokens[0];
    int noOfBlocks = std::stoi(tokens[1]);

    std::vector<std::string> ignore;
    for(int i=0;i<noOfBlocks;i++){
        ignore.push_back(getStringFromSocket(new_fd));
    }

}
std::string upload_file(int new_fd,std::string filePath,std::string group_id,std::string user_id){
    std::string status;
    if(user_id==""){
        status = "First login to upload file";
        upload_file_exitHelper(new_fd);
    }
    else{
        if(groupMap.find(group_id) == groupMap.end()){
            status = "Group does not exist with group_id :";
            status.append(group_id);
            upload_file_exitHelper(new_fd);
        }
        else{
            Group* group = groupMap[group_id];
            auto x=std::find(group->members.begin(),group->members.end(),user_id);
            if(x==group->members.end()){
                status = "User does not belong to group ";status.append(group_id);
                upload_file_exitHelper(new_fd);
            }
            else{
                //now user can upload file

                //get file name
                std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

                //get hash of each block and total size of file
                std::string sizePlusNoOfHashes = getStringFromSocket(new_fd);
                std::vector<std::string> tokens = getTokens(sizePlusNoOfHashes);
                std::string size = tokens[0];
                int noOfBlocks = std::stoi(tokens[1]);

                std::vector<std::string> hashOfChunks;
                for(int i=0;i<noOfBlocks;i++){
                    hashOfChunks.push_back(getStringFromSocket(new_fd));
                    std::cout<<hashOfChunks[i]<<std::endl;
                }

                //make the PeerInfo object
                auto peer = PeerInfo();
                peer.filePathInPeer = filePath;
                peer.user_id = user_id;

                //make Filemetadata obj
                auto fileMetaData = new FileMetaData();
                fileMetaData->fileSize = size;
                fileMetaData->hashOfChunks = hashOfChunks;
                fileMetaData->clientsHavingThisFile.push_back(peer);
                fileMetaData->fileName = fileName;

                //check if file is already shared 
                auto y = group->filesShared.find(fileName);
                if(y==group->filesShared.end()){ //not shared by any peer before
                    group->filesShared[fileName] = fileMetaData;
                    status = "File shared"; 
                }
                else{ // has been already shared by some peer before
                    //so now match hash and then add peer to list of peer in filemeta data
                    auto hashSaved = group->filesShared[fileName]->hashOfChunks;
                    if(!isHashEqual(hashSaved,hashOfChunks)){
                        status = "There exists a file with same name but diff hash(content). Change name for uploading";
                    }
                    else{
                        //now hashes are equal

                        //check is user_id is already in list of clients
                        auto peersSharedThisFile = group->filesShared[fileName]->clientsHavingThisFile;
                        if(group->peerPresentInList(fileName,user_id)){ 
                            status =fileName;
                            status.append("Already shared by this peer");
                        }
                        else{
                            //need to add peer to the list
                            group->updatePeerListInGroup(fileName,peer);
                            status = "File shared";
                        }
                    }
                }
            }
        }
    }

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending status signal failed in create group \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of upload file"<<std::endl;
    return status;
}

std::string create_group(int new_fd,std::string user_id,std::string group_id){

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
    }

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending status signal failed in create group \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of create_group"<<std::endl;
    return status;
}

std::string accept_request(int new_fd,std::string currUser,std::string group_id,std::string user_id){
    std::string status="";
    if(user_id==""){
        status = "First login to list requests";
    }
    else{
        if(groupMap.find(group_id) == groupMap.end()){
            status = "Group does not exist with group_id :";
            status.append(group_id);
            removeGroupJoinReq(currUser,group_id,user_id);
        }
        else if(peerMap.find(user_id) == peerMap.end()){
            status = "User does not exist with user_id :";
            status.append(user_id);
            removeGroupJoinReq(currUser,group_id,user_id);
        }
        else{
            Group* group = groupMap[group_id];
            if(group->ownwer != currUser){
                status="Only owner of group can accept request: Permission denied";
            }
            else{
                Group* group = groupMap[group_id];
                auto x=std::find(group->members.begin(),group->members.end(),user_id);
                if(x!=group->members.end()){ //user_id is already a member
                    status = "User is already a member of group :";status.append(group_id);
                    removeGroupJoinReq(currUser,group_id,user_id);
                }
                else{
                    //send req to owner
                    addMemberToGroup(user_id,group_id);
                    status = user_id;
                    status.append(" Added to group : ");status.append(group_id);
                    removeGroupJoinReq(currUser,group_id,user_id);
                }
            }
        }
    }

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending status signal failed in create group \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of accept request "<<std::endl;
    return status;
}

std::string list_requests(int new_fd,std::string user_id,std::string group_id){
    std::string status="";
    if(user_id==""){
        status = "First login to list requests";
    }
    else{
        if(groupMap.find(group_id) == groupMap.end()){
            status = "Group does not exist with group_id :";
            status.append(group_id);
        }
        else{
            Group* group = groupMap[group_id];
            if(group->ownwer != user_id){
                status="Only owner can list request: Permission denied";
            }
            else{
                if(peerMap[user_id]->groupJoinRequest.size() == 0){
                    status="No pending request";
                }
                else{
                    status.append("Group").append("    ").append("User ID").append("\n");
                    for(auto r:peerMap[user_id]->groupJoinRequest){
                        status.append(r.first).append("    ").append(r.second).append("\n");
                    }
                }
            }
        }
    }

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending status signal failed in list request \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of list request"<<std::endl;
    return status;
}

void getPeersWithFile_exitHelper(int new_fd){
    std::string peerListSizeStr = "0";
    peerListSizeStr.append(delim).append("0");
    if(send(new_fd,peerListSizeStr.c_str(),peerListSizeStr.size(),0) == -1){
        printf("sending no of peers in get peers with file failed \n");
        close(new_fd);
        exit(1);
    }
    dummyRecv(new_fd);
}

std::string getPeersWithFile(int new_fd,std::string group_id,std::string fileName,std::string currUser){
    std::string status;
    std::string noOfFilesStr = "0";
    if(currUser==""){
        status = "First login to list requests";
        getPeersWithFile_exitHelper(new_fd);
    }
    else if (groupMap.find(group_id) == groupMap.end()){
            status = "Group does not exists ";
            getPeersWithFile_exitHelper(new_fd);
        }
    else{
        auto group = groupMap[group_id];
        auto x=std::find(group->members.begin(),group->members.end(),currUser);
        if(x==group->members.end()){
            status = "User does not belong to group ";status.append(group_id);
            getPeersWithFile_exitHelper(new_fd);
        }
        else{
            if(group->filesShared.find(fileName) == group->filesShared.end()){
                status = fileName;
                status.append(" is not shared in the group ").append(group_id);
                getPeersWithFile_exitHelper(new_fd);
            }
            else{
                std::string peerListSizeStr = group->filesShared[fileName]->fileSize;
                peerListSizeStr.append(delim).append(std::to_string(group->filesShared[fileName]->clientsHavingThisFile.size()));
                status = "sending no of peers :";status.append(std::to_string(group->filesShared[fileName]->clientsHavingThisFile.size()));
                if(send(new_fd,peerListSizeStr.c_str(),peerListSizeStr.size(),0) == -1){
                    printf("sending no of peers in get peers with file failed \n");
                    close(new_fd);
                    exit(1);
                }
                dummyRecv(new_fd);

                for(auto peer: group->filesShared[fileName]->clientsHavingThisFile){
                    std::cout<<"Sending user id "<<peer.user_id<<std::endl;

                    if(send(new_fd,peer.user_id.c_str(),peer.user_id.size(),0) == -1){
                        printf("sending user id in get peers with files failed \n");
                        close(new_fd);
                        exit(1);
                    }
                    dummyRecv(new_fd);

                    std::string address = peerMap[peer.user_id]->IP;
                    address.append(":").append(peerMap[peer.user_id]->port);
                    std::cout<<"Sending address in peer "<<address<<std::endl;

                    if(send(new_fd,address.c_str(),address.size(),0) == -1){
                        printf("sending address in get peers with file failed \n");
                        close(new_fd);
                        exit(1);
                    }
                    dummyRecv(new_fd);

                    std::cout<<"Sending file path "<<peer.filePathInPeer<<std::endl;

                    if(send(new_fd,peer.filePathInPeer.c_str(),peer.filePathInPeer.size(),0) == -1){
                        printf("sending file path in get peers with files failed \n");
                        close(new_fd);
                        exit(1);
                    }
                    dummyRecv(new_fd);
                }
            }
        }
    }

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending status in get peers with files  \n");
        close(new_fd);
        exit(1);
    }
    dummyRecv(new_fd);    
    
    std::cout<<"end of get peers with files"<<std::endl;
    return status;
}

std::string list_files(int new_fd,std::string group_id){
    std::string status;
    std::string noOfFilesStr = "0";
    if(groupMap.find(group_id) == groupMap.end()){
        status = "Group does not exist with group_id :";
        status.append(group_id);

        if(send(new_fd,noOfFilesStr.c_str(),noOfFilesStr.size(),0) == -1){
            printf("sending no of files in list files failed \n");
            close(new_fd);
            exit(1);
        }
        dummyRecv(new_fd);
    }
    else{
        auto group = groupMap[group_id];
        int noOfFiles = group->filesShared.size();
        noOfFilesStr = std::to_string(noOfFiles);
        std::cout<<"Sending no of files :"<<noOfFilesStr<<std::endl;
        status = "No of files sent is :";status.append(noOfFilesStr);
        if(send(new_fd,noOfFilesStr.c_str(),noOfFilesStr.size(),0) == -1){
            printf("sending no of files in list files failed \n");
            close(new_fd);
            exit(1);
        }
        dummyRecv(new_fd);

        for(auto f : group->filesShared){
            std::cout<<"Sending file name :"<<f.first<<std::endl;
            if(send(new_fd,f.first.c_str(),f.first.size(),0) == -1){
                printf("sending file names in list files failed \n");
                close(new_fd);
                exit(1);
            }
            dummyRecv(new_fd);
        }
    }

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending status in list group \n");
        close(new_fd);
        exit(1);
    }
    dummyRecv(new_fd);

    std::cout<<"end of list files"<<std::endl;
    return status;
}

std::string list_groups(int new_fd){
    std::string status;
    if(groupMap.size() == 0){
        status = "No groups created yet ";
    }
    else{
        for(auto g : groupMap){
            status.append(g.first).append("\n");
        }
    }
    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending status in list group \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of list group"<<std::endl;
    return status;
}

std::string join_group(int new_fd,std::string user_id,std::string group_id){
    std::string status;
    if(user_id==""){
        status = "First login to create group";
    }
    else{
        if(groupMap.find(group_id) == groupMap.end()){
            status = "Group does not exist with group_id :";
            status.append(group_id);
        }
        else{
            Group* group = groupMap[group_id];
            auto x=std::find(group->members.begin(),group->members.end(),user_id);
            if(x!=group->members.end()){ //user_id is already a member
                status = "User is already a member of group :";status.append(group_id);
            }
            else{
                //send req to owner
                addGroupJoinReqToPeer(group->ownwer,user_id,group_id);
                status = "Requested group owner to join group : ";status.append(group_id);
            }
        }
    }

    if(send(new_fd,status.c_str(),status.size(),0) == -1){
        printf("sending status in join group \n");
        close(new_fd);
        exit(1);
    }

    dummyRecv(new_fd);
    std::cout<<"end of join_group"<<std::endl;
    return status;
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
            if(tokens.size() != 3){
                std::cout<<"wrong format for upload_file "<<std::endl;
            }
            else{
                std::cout<<"file_upload called "<<std::endl;
                std::string status = upload_file(new_fd,tokens[1],tokens[2],currUser);
                std::cout<<status<<std::endl;
            }
            //acceptTorFileFromPeer(new_fd);
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
            std::string status;
            if(tokens.size() != 2){
                std::cout<<"wrong format for create_group "<<std::endl;
            }
            else{
                
                std::cout<<"create group called "<<std::endl;
                status = create_group(new_fd,currUser,tokens[1]);
            }
            std::cout<<status<<std::endl;
        }

        else if(command == "join_group"){
            if(tokens.size() != 2 ){
                std::cout<<"wrong format for join group "<<std::endl;
            }
            std::cout<<join_group(new_fd,currUser,tokens[1])<<std::endl;
        }

        else if(command == "list_groups"){
            if(tokens.size() != 1 ){
                std::cout<<"wrong format for list group "<<std::endl;
            }
            std::cout<<list_groups(new_fd)<<std::endl;
        }

        else if(command == "list_files"){
            if(tokens.size() != 2 ){
                std::cout<<"wrong format for list files "<<std::endl;
            }
            std::cout<<list_files(new_fd,tokens[1])<<std::endl;
        }

        else if(command == "getPeersWithFile"){
            if(tokens.size() != 3 ){
                std::cout<<"wrong format for getPeersWithFils "<<std::endl;
            }
            std::cout<<getPeersWithFile(new_fd,tokens[1],tokens[2],currUser)<<std::endl;
        }

        else if(command == "list_requests"){
            if(tokens.size() != 2 ){
                std::cout<<"wrong format for lists requests "<<std::endl;
            }
            std::cout<<list_requests(new_fd,currUser,tokens[1])<<std::endl;
        }

        else if(command == "accept_request"){
            if(tokens.size() != 2 ){
                std::cout<<"wrong format for accept requests "<<std::endl;
            }
            std::cout<<accept_request(new_fd,currUser,tokens[1],tokens[2])<<std::endl;
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
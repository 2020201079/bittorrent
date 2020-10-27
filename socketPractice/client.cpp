/*
filename server_ipaddress portno

argv[0] filename
argv[1] server_ipaddress
argv[2] portno

*/

#include<stdio.h>
#include<stdlib.h> //atoi
#include<string.h>
#include<unistd.h> //read write close
#include<sys/socket.h> //structs needed for sockets eg sockaddr
#include<sys/types.h> // contains structures needed for internet domain addresses eg sockaddr_in
#include<netinet/in.h>
#include<netdb.h> // hostent structure (stores info of a host)
#include<arpa/inet.h>

void error(const char* msg){
    perror(msg);
    exit(1);
}

int main(int argc,char * argv[]){
    int sockfd,portno,n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[255];
    if(argc<3){
        fprintf(stderr,"usage %s hostname port\n",argv[0]);
        exit(0);
    }

    portno = atoi(argv[2]);
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0)
        error("Error opening socket");
    
    /*
    server = gethostbyname(argv[1]);
    if(server == NULL){
        error("No such host");
    }*/

    bzero((char *)&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 

    if(connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
        error("Connection failed");
    
    while(1){
        bzero(buffer,255);
        fgets(buffer,255,stdin);
        n = write(sockfd,buffer,strlen(buffer));
        if(n<0)
            error("Error on writing");
        
        bzero(buffer,255);
        n=read(sockfd,buffer,255);
        if(n<0)
            error("Error on reading");
        printf("Server: %s",buffer);

        int i=strncmp("Bye",buffer,3);
        if(i==0)
            break;
    }

    close(sockfd);
    return 0;
}
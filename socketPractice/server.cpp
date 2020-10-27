#include<stdio.h>
#include<stdlib.h> //atoi
#include<string.h>
#include<unistd.h>
#include<sys/socket.h> //structs needed for sockets eg sockaddr
#include<sys/types.h> // contains structures needed for internet domain addresses eg sockaddr_in
#include<netinet/in.h>

void error(const char *msg){
    perror(msg);
    exit(1);
}

int main(int argc,char * argv[ ]){
    if(argc<2){
        fprintf(stderr,"Port number not provided. Program terminated\n");
        exit(1);
    }
    int sockfd, newsockfd, portno;
    char buffer[255];

    struct sockaddr_in serv_addr,cli_addr;
    socklen_t clilen;

    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd<0){
        error("Error opening Socket.");
    }

    bzero((char *)&serv_addr,sizeof(serv_addr));
    portno = atoi(argv[1]);

    //google this part
    serv_addr.sin_family = AF_INET; // is an address family that is used to designate the type of addresses that your socket
    //communicate with.
    serv_addr.sin_addr.s_addr = INADDR_ANY; //binds with all interfaces, not just local 
    serv_addr.sin_port = htons(portno); //host to network short

    if(bind(sockfd,(struct sockaddr *) &serv_addr, sizeof(serv_addr))< 0){
        error("Binding failed");
    }

    listen(sockfd,5);

    clilen = sizeof(cli_addr);

    newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);

    if(newsockfd < 0){
        error("Error on accept");
    }

    while(1){
        bzero(buffer,255);
        int n = read(newsockfd,buffer,255);
        if(n<0)
            error("Error on read");
        printf("Client : %s\n",buffer);
        bzero(buffer,255);
        fgets(buffer,255,stdin);

        n = write(newsockfd,buffer,strlen(buffer));
        if(n<0)
            error("error on writing");
        int i=strncmp("Bye",buffer,3);
        if(i==0)
            break;
    }
    close(newsockfd);
    close(sockfd);
    return 0; 
}
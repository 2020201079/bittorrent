#include<stdio.h>
#include<stdlib.h> //atoi
#include<string.h>
#include<unistd.h>
#include<sys/socket.h> //structs needed for sockets eg sockaddr
#include<sys/types.h> // contains structures needed for internet domain addresses eg sockaddr_in
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>

int main(int argc,char *argv[]){
    int sock_fd,new_fd;
    struct addrinfo hints, *res;

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET; // ipv4
    hints.ai_socktype = SOCK_STREAM; // for tcp

    getaddrinfo("127.0.0.1","3490",&hints,&res);

    // make a socket
    sock_fd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

    connect(sock_fd,res->ai_addr,res->ai_addrlen);

    freeaddrinfo(res);

    char buf[100];
    int numbytes = recv(sock_fd,buf,99,0);
    buf[numbytes] = '\0';
    printf("client side %s",buf);
    close(sock_fd);
    return 0;
}
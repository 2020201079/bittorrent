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

    bind(sock_fd,res->ai_addr,res->ai_addrlen);

    freeaddrinfo(res);

    listen(sock_fd,5);

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

        char *buffer = "hello world";
        send(new_fd,buffer,strlen(buffer),0);
    }
    close(new_fd);
}

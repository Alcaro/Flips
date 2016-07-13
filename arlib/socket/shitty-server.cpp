#ifdef ARLIB_TEST_SERVER
//Shitty Server: a buggy echo server
//after the first 32 bytes, it drops your connection on the floor, without FIN or anything
//probably somewhat useful to test resilience against network failure
//it would be more useful to make it ignore the pings too, but I can't do that without fiddling with the firewall, and I'd rather not

//linux and root only because TCP_REPAIR requires that
//http://oroboro.com/dealing-with-network-port-abuse-in-sockets-in-c
//if you need to test a windows program against dropped sockets, run this on another machine, possibly a virtual machine

//most of the code stolen from http://www.thegeekstuff.com/2011/12/c-socket-programming/ because I'm lazy

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <netinet/tcp.h>

int main()
{
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr; 

    char sendBuff[1025];
    time_t ticks; 

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serv_addr, '0', sizeof(serv_addr));
    memset(sendBuff, '0', sizeof(sendBuff)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(168);

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)); 
    perror("bind");

    listen(listenfd, 10); 
    perror("listen");

    while(1)
    {
        connfd = accept(listenfd, (struct sockaddr*)NULL, NULL); 
        perror("accept");

        memset(sendBuff, 0, 32);
        read(connfd, sendBuff, 32);
        write(connfd, sendBuff, 32);
        sleep(1); // otherwise the ACK gives a RST

        int yes = 1;
        setsockopt(connfd, SOL_TCP, TCP_REPAIR, &yes, sizeof(yes));
        perror("TCP_REPAIR");

        close(connfd);
     }
}
#endif

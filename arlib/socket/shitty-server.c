/*
 * Shitty Server: an unusually agressive Discard Protocol server <https://en.wikipedia.org/wiki/Discard_Protocol>
 * it reads and discards all your data for at least 5 seconds; then it drops your connection on the floor, without FIN or anything
 * probably somewhat useful to test your program's resilience against network failure
 *
 * system requirements: linux only, requires root because TCP_REPAIR requires that
 * if you need to test a windows program against dropped sockets, run this on another machine, possibly a virtual machine
 *
 * license: WTFPL, any version
 *
 * further reading: http://oroboro.com/dealing-with-network-port-abuse-in-sockets-in-c
 */

/* feel free to replace with port 99 if you want to test in firefox or something */
/* (no idea why they'd block the port specifically defined to not parse your data) */
#define PORTNR 9

#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/tcp.h>
#include <sys/select.h>

int listen_create(int port)
{
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY);
	sa.sin_port = htons(port);
	
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	bind(fd, (struct sockaddr*)&sa, sizeof(sa));
	listen(fd, 10);
	
	return fd;
}

int main()
{
	int* fds = NULL;
	int nfds_prev = 0; /* whenever nextcycle arrives, discard this many sockets from the array */
	int nfds = 0;
	time_t nextcycle = time(NULL);
	
	int listen = listen_create(PORTNR);
	
	while (1)
	{
		int i;
		
		fd_set fdset;
		FD_ZERO(&fdset);
		FD_SET(listen, &fdset);
		for (i=0;i<nfds;i++) FD_SET(fds[i], &fdset);
		select(FD_SETSIZE, &fdset, NULL, NULL, NULL); /* don't bother with timeouts */
		
		if (FD_ISSET(listen, &fdset))
		{
			int newfd = accept4(listen, NULL, NULL, SOCK_NONBLOCK);
			if (newfd >= 0)
			{
				fds = realloc(fds, sizeof(int)*(nfds+1));
				fds[nfds] = newfd;
				nfds++;
			}
		}
		
		for (i=0;i<nfds;i++)
		{
			if (FD_ISSET(fds[i], &fdset))
			{
				static char dump[1024];
				recv(fds[i], dump, sizeof(dump), MSG_DONTWAIT);
			}
		}
		
		if (time(NULL) > nextcycle)
		{
			nextcycle = time(NULL)+5;
			for (i=0;i<nfds_prev;i++)
			{
				static int yes = 1;
				setsockopt(fds[i], IPPROTO_TCP, TCP_REPAIR, &yes, sizeof(yes));
				close(fds[i]);
			}
			memmove(fds, fds+nfds_prev, sizeof(int)*(nfds-nfds_prev));
			nfds -= nfds_prev;
			nfds_prev = nfds;
		}
	}
}

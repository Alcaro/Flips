#ifdef __linux__
#include "sandbox.h"
void sandbox_lockdown(int* allow_fd, int n_allow_fd);


void sandbox_cross_init(int* pfd, int* cfd);
void sandbox_cross_send(int socket, int fd_to_send, int errno);
int sandbox_cross_recv(int socket);

#endif

#ifdef __linux__
#include "sandbox-internal.h"
#undef bind
 #include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void sandbox_cross_init(int* pfd, int* cfd)
{
	int fds[2];
	socketpair(AF_LOCAL, SOCK_DGRAM, 0, fds);
	*pfd = fds[0];
	*cfd = fds[1];
}

//from http://blog.varunajayasiri.com/passing-file-descriptors-between-processes-using-sendmsg-and-recvmsg
//somewhat reformatted
void sandbox_cross_send(int socket, int fd_to_send, int errno_ret)
{
	//need at least one byte of data, otherwise recvmsg gets angry
	struct iovec iov = { &errno_ret, sizeof(errno_ret) };
	char ctrl_buf[CMSG_SPACE(sizeof(int))] = {};
	struct msghdr message = {
		.msg_name = NULL, .msg_namelen = 0,
		.msg_iov = &iov, .msg_iovlen = 1,
		.msg_control = ctrl_buf, .msg_controllen = sizeof(ctrl_buf),
		.msg_flags = 0,
	};
	
	cmsghdr* ctrl_msg = CMSG_FIRSTHDR(&message);
	ctrl_msg->cmsg_level = SOL_SOCKET;
	ctrl_msg->cmsg_type = SCM_RIGHTS;
	ctrl_msg->cmsg_len = CMSG_LEN(sizeof(int));
	*(int*)CMSG_DATA(ctrl_msg) = fd_to_send;
	
	if (fd_to_send >= 0) errno = 0;
	if (fd_to_send < 0) message.msg_controllen = 0;
	
	sendmsg(socket, &message, 0);
}

int sandbox_cross_recv(int socket)
{
	int errno_ret;
	struct iovec iov = { &errno_ret, sizeof(int) };
	char ctrl_buf[CMSG_SPACE(sizeof(int))] = {};
	struct msghdr message = {
		.msg_name = NULL, .msg_namelen = 0,
		.msg_iov = &iov, .msg_iovlen = 1,
		.msg_control = ctrl_buf, .msg_controllen = sizeof(ctrl_buf),
		.msg_flags = 0,
	};
	
	if (recvmsg(socket, &message, 0) <= 0) return -1;
	
	for (cmsghdr* ctrl_msg=CMSG_FIRSTHDR(&message);ctrl_msg!=NULL;ctrl_msg=CMSG_NXTHDR(&message, ctrl_msg))
	{
		if (ctrl_msg->cmsg_level == SOL_SOCKET && ctrl_msg->cmsg_type == SCM_RIGHTS)
		{
			return *(int*)CMSG_DATA(ctrl_msg);
		}
	}
	
	errno = errno_ret;
	return -1;
}


int sandbox_cross_request(int socket, const char * path, int flags, mode_t mode)
{
	struct iovec iov[3] = { { &flags, sizeof(flags) }, { &mode, sizeof(mode) }, { (char*)path, strlen(path) } };
	struct msghdr message = {
		.msg_name = NULL, .msg_namelen = 0,
		.msg_iov = iov, .msg_iovlen = 2,
		.msg_control = NULL, .msg_controllen = 0,
		.msg_flags = 0,
	};
	
	sendmsg(socket, &message, 0);
	return sandbox_cross_recv(socket);
}

void sandbox_cross_serve_request_full(int socket,
                                      int (*access)(const char * path, int flags, mode_t mode, void* userdata),
                                      void* userdata)
{
	struct {
		int flags;
		mode_t mode;
		char path[1024+1];
	} msg;
	
	struct iovec iov = { &msg, sizeof(msg)-1 };
	struct msghdr message = {
		.msg_name = NULL, .msg_namelen = 0,
		.msg_iov = &iov, .msg_iovlen = 1,
		.msg_control = NULL, .msg_controllen = 0,
		.msg_flags = 0,
	};
	
	int bytes = recvmsg(socket, &message, MSG_DONTWAIT);
	if (bytes <= 0) return;
	if (message.msg_flags & MSG_TRUNC)
	{
		sandbox_cross_send(socket, -1, ENAMETOOLONG);
		return;
	}
	
	((char*)&msg)[bytes] = '\0';
	int retfd = access(msg.path, msg.flags, msg.mode, userdata);
	sandbox_cross_send(socket, retfd, errno);
}

struct req_dat {
	bool (*access)(const char * path, bool write, void* userdata);
	void* userdata;
};
static int req_sub(const char * path, int flags, mode_t mode, void* userdata)
{
	req_dat* dat = (req_dat*)userdata;
	
	if (flags & O_CREAT)
	{
		if (mode & ~0777) goto deny;
	}
	else mode=0;
	
	static_assert(O_RDONLY==0);
	static_assert(O_WRONLY==1);
	static_assert(O_RDWR==2);
	
	static const int flag_ignore = 0; // allow these flags, but ignore them
	static const int flag_read = O_CLOEXEC|O_LARGEFILE; // allow these flags
	static const int flag_write = O_WRONLY|O_RDWR|O_APPEND|O_CREAT|O_EXCL|O_TRUNC; // allow these flags, but only if write access is fine
	flags &= ~flag_ignore;
	if ((flags & (O_WRONLY|O_RDWR)) == (O_WRONLY|O_RDWR)) goto deny; // invalid open mode
	if (flags & ~(flag_read | flag_write)) goto deny; // unacceptable flags
	
	if (!dat->access(path, (flags & flag_write), dat->userdata)) goto deny;
	
	return open(path, flags, mode);
	
deny:
	errno = EACCES;
	return -1;
}
void sandbox_cross_serve_request(int socket, bool (*access)(const char * path, bool write, void* userdata), void* userdata)
{
	req_dat dat = { access, userdata };
	sandbox_cross_serve_request_full(socket, req_sub, &dat);
}
#endif

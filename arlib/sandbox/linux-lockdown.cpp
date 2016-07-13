#ifdef __linux__
#include "sandbox-internal.h"
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <linux/futex.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/syscall.h>
#include <linux/futex.h>
       #include <sys/types.h>
       #include <sys/stat.h>
       #include <fcntl.h>


#include <sys/types.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/wait.h> 


static bool int_in(int* list, int len, int val)
{
	for (int i=0;i<len;i++)
	{
		if (list[i] == val) return true;
	}
	return false;
}

static void close_fds(int* allow_fd, int n_allow_fd)
{
	//based on http://www.opensource.apple.com/source/sudo/sudo-46/src/closefrom.c, license BSD-2, but fairly rewritten
	bool changed;
	
	do {
		changed = false;
		
		DIR* dirp = opendir("/proc/self/fd/");
		if (!dirp) _exit(0);
		
		while (true)
		{
			dirent* dent = readdir(dirp);
			if (dent == NULL) break;
			
			if (strcmp(dent->d_name, ".") == 0) continue;
			if (strcmp(dent->d_name, "..") == 0) continue;
			
			char* endp;
			long fd = strtol(dent->d_name, &endp, 10);
			if (dent->d_name==endp || *endp!='\0' || fd<0 || fd>=INT_MAX)
			{
				_exit(0);
			}
			
			if (fd != dirfd(dirp) && !int_in(allow_fd, n_allow_fd, (int)fd))
			{
				close((int)fd);
				changed = true;
			}
		}
		closedir(dirp);
	} while (changed);
}

void sandbox_lockdown(int* allow_fd, int n_allow_fd)
{
	prctl(PR_SET_DUMPABLE, false);
	prctl(PR_SET_TSC, PR_TSC_SIGSEGV);
	prctl(PR_SET_PDEATHSIG, SIGKILL);
	
	close_fds(allow_fd, n_allow_fd);
	
	//TODO: rlimit?
	
	prctl(PR_SET_NO_NEW_PRIVS, true);
	//TODO: seccomp
	//TODO: test vsyscall and vdso with seccomp enabled
}
#endif

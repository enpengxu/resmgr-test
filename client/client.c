/*
 * Message Client Process
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include "../common.h"

struct ioctl_noop {
	uint32_t foo;
};

#define MY_IOCTL_NOOP	_IOWR(0x01, 1, struct ioctl_noop)

static void *
io_send_thread(void *arg)
{
	int rc, num = 0;
	int fd = (int)(uintptr_t)arg;

	struct io_msg msg = { 0 };
	struct io_reply reply = { 0 };

    msg.msg_no = _IO_MAX + 1;
	msg.msg_tid= (uint32_t)pthread_self();
    /* Send the data to the server and get a reply */
	while(1) {
		fprintf(stderr, "client[%x]: sending %d \n", msg.msg_tid, num);

		if (msg.msg_tid % 2 ) {
			sprintf(msg.msg_data, "client 1 msg #%d", num++);
		} else {
			sprintf(msg.msg_data, "client 0 msg #%d", num++);
		}
#if 0
		msg.msg_len = strlen(msg.msg_data);

		rc = MsgSend(fd, &msg, sizeof(uint32_t)*2 + msg.msg_len,
				&reply, sizeof(reply));
		if(rc == -1) {
			fprintf( stderr, "clinet: error! Unable to MsgSend() to server: %s\n", strerror(errno));
			return NULL;
		}
#else
		{
			struct ioctl_noop noop = { 0 };
			rc = devctl(fd, MY_IOCTL_NOOP, &noop, sizeof(noop), NULL);
		}
#endif
		fprintf(stderr, "client[%x]: done!\n", msg.msg_tid);

		sleep(2);
	}
	return NULL;
}

int main( int argc, char **argv )
{
    int rc, fd1, fd2;

    fd1 = open( "/dev/myresmgr", O_RDWR );
    if( fd1 == -1 ) {
        fprintf( stderr, "Unable to open server connection: %s\n",
            strerror( errno ) );
        return EXIT_FAILURE;
    }

	fd2 = open( "/dev/myresmgr", O_RDWR );
    if( fd2 == -1 ) {
        fprintf( stderr, "Unable to open server connection: %s\n",
            strerror( errno ) );
        return EXIT_FAILURE;
    }

#define NUM 3
	pthread_t tid[NUM+1];
	int i;
	for(i=0; i<NUM; i++) {
		rc = pthread_create(&tid[i], NULL, io_send_thread, (void *)(uintptr_t)fd1);
		assert(rc == 0);
	}

	rc = pthread_create(&tid[i], NULL, io_send_thread, (void *)(uintptr_t)fd2);

	for(i=0; i<NUM; i++) {
		pthread_join(tid[i], NULL);
	}
	pthread_join(tid[i], NULL);

    close( fd1 );
    close( fd2 );
    return 0;
}

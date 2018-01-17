/*
 * Message Client Process
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>

typedef struct
{
    uint16_t msg_no;
    char msg_data[4096];
} client_msg_t;

int main( int argc, char **argv )
{
    int fd;
    int i, c;
    client_msg_t msg[64];
    long ret;
    char msg_reply[255];

	iov_t	s_iov[64];
	iov_t	r_iov[1];

	for(i=0; i< 64; i++){
		SETIOV(s_iov + i, &msg[i], sizeof msg[0]);
		memset( &msg[i], 0, sizeof( msg[0] ) );
		snprintf(msg[i].msg_data, 254, "%d: client %d requesting reply.", i, getpid() );
	}
	SETIOV(r_iov + 0, msg_reply, sizeof msg_reply);

    /* Open a connection to the server (fd == coid) */
    //fd = open( "/dev/drm/card0", O_RDWR );
    fd = open( "/dev/myresmgr", O_RDWR );
    if( fd == -1 )
    {
        fprintf( stderr, "Unable to open server connection: %s\n",
            strerror( errno ) );
        return EXIT_FAILURE;
    }

    /* Clear the memory for the msg and the reply */
    memset( &msg_reply, 0, sizeof( msg_reply ) );

    /* Set up the message data to send to the server */
    msg[0].msg_no = _IO_MAX + 1;

    fflush( stdout );

	SETIOV(s_iov + 1, (void *)0x12345, 64);
    /* Send the data to the server and get a reply */
    ret = MsgSendv(fd, s_iov, 64, r_iov, 1);
    if( ret == -1 )
    {
        fprintf( stderr, "Unable to MsgSend() to server: %s\n", strerror( errno ) );
        return EXIT_FAILURE;
    }

    /* Print out the reply data */
    printf( "client: server replied: %s\n", msg_reply );

    printf( "client: original send data: %s\n", msg[0].msg_data );

    close( fd );

    return EXIT_SUCCESS;
}

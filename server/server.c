/*
 * ResMgr and Message Server Process
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <sys/neutrino.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include "../common.h"

struct server_ocb {
	iofunc_ocb_t hdr;
	pthread_rwlock_t  rw_lock;
};

int io_message( message_context_t *ctp,
				int type, unsigned flags,void *handle )
{
    struct io_msg    msg;
	struct io_reply  reply = { .msg_reply = 1, .msg_data = ctp->info.tid }; /* return thread id to client */

	uint32_t tid = (uint32_t)pthread_self();

	if(-1 == MsgRead(ctp->rcvid, &msg, 2 * sizeof(uint32_t), 0)) {
		int err = errno;
		fprintf(stderr, "MsgRead failed, errno = %d\n", errno);
		MsgReply(ctp->rcvid, err, &reply, sizeof(reply));
		return -1;
	}

	if (msg.msg_len > sizeof(msg.msg_data)){
		fprintf(stderr, "MsgRead failed, errno = %d\n", errno);
		return -1;
	}
	if(-1 == MsgRead(ctp->rcvid, &msg, 2*sizeof(uint32_t) + msg.msg_len, 0)) {
		int err = errno;
		fprintf(stderr, "MsgRead failed, errno = %d\n", errno);
		MsgReply(ctp->rcvid, err, &reply, sizeof(reply));
		return -1;
	}
	msg.msg_data[msg.msg_len] = '\0';
	fprintf(stderr, "server[%x]: client %x (ctp->tid %x) sending msg: \n"
			"      len: %d\n"
			"      str: %s\n",
			tid,
			msg.msg_tid, ctp->info.tid,  msg.msg_len, msg.msg_data);
	if (tid % 2 ){
		fprintf(stderr, "server[%x]: I am going to sleep \n", (uint32_t)tid);
		sleep(1255);
	}
    /* Send a reply to the waiting (blocked) client */
    MsgReply(ctp->rcvid, EOK, &reply, sizeof(reply));

	fprintf(stderr, "server[%x]: done!\n", tid);
    return 0;
}


static int
io_devctl(resmgr_context_t *ctp, io_devctl_t *msg, RESMGR_OCB_T *ocb)
{
    uint32_t tid = (uint32_t)pthread_self();

	fprintf(stderr, "io_devctl     : server[%x]  client: %x\n",
			tid, (uint32_t)ctp->info.tid);

	if (tid % 2 ){
		fprintf(stderr, "server[%x]: I am going to sleep \n", (uint32_t)tid);
		sleep(1255);
	}

	msg->o.zero = 0;
	msg->o.zero2 = 0;

	msg->o.ret_val = 0;
	msg->o.nbytes = msg->i.nbytes;
	return (_RESMGR_PTR(ctp, &msg->o, sizeof(msg->o) + msg->o.nbytes));
}

static int
io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle, void *extra)
{
	int rc;
	iofunc_attr_t * attr = handle;
	struct server_ocb * ocb;

	rc = iofunc_attr_lock(attr);
	assert(rc == 0);

	rc = iofunc_open(ctp, msg, attr, 0, 0);
	assert(rc == 0);

	ocb = calloc(1, sizeof(*ocb));
	assert(ocb);

	rc = pthread_rwlock_init(&ocb->rw_lock, NULL);
	assert(rc == 0);

	rc  = iofunc_ocb_attach(ctp, msg, &ocb->hdr, attr, 0);
	assert(rc == 0);

	rc = iofunc_attr_unlock(attr);
	assert(rc == 0);

	return rc;
}


#define USE_RWLOCK_OCB

static int
server_iofunc_lock_ocb(resmgr_context_t *ctp,
		void *reserved, iofunc_ocb_t *ocb)
{
	fprintf(stderr, "io_lock_ocb   : ocb : %p, clinet:%x\n",
			ocb, (uint32_t)ctp->info.tid);
#ifdef USE_RWLOCK_OCB
	int rc;
	struct server_ocb * socb = ocb;
	rc = pthread_rwlock_rdlock(&socb->rw_lock);
	assert(rc == 0);
	return rc;
#else
	return 0; //iofunc_lock_ocb_default(ctp, reserved, ocb);
#endif
}
static int
server_iofunc_unlock_ocb(resmgr_context_t *ctp,
		void *reserved, iofunc_ocb_t *ocb)
{
	fprintf(stderr, "io_unlock_ocb : ocb : %p, client:%x\n",
			ocb, (uint32_t)ctp->info.tid);
#ifdef USE_RWLOCK_OCB
	int rc;
	struct server_ocb * socb = ocb;
	rc = pthread_rwlock_unlock(&socb->rw_lock);
	assert(rc == 0);

	return rc;
#else
	return 0; //iofunc_unlock_ocb_default(ctp, reserved, ocb);
#endif
}

static int
server_iofunc_close_ocb(resmgr_context_t *ctp,
		void *reserved, iofunc_ocb_t *ocb)
{
	int rc;
	struct server_ocb * socb = ocb;

	fprintf(stderr, "io_close_ocb [%p] reying to lock rwlock ...\n", ocb);
	rc = pthread_rwlock_wrlock(&socb->rw_lock);
	assert(rc == 0);
	fprintf(stderr, "io_close_ocb rwlocked ...\n");

	rc = iofunc_close_ocb_default(ctp, NULL, (iofunc_ocb_t*)ocb);
	assert(rc == 0);

	rc = pthread_rwlock_unlock(&socb->rw_lock);
	assert(rc == 0);

	rc = pthread_rwlock_destroy(&socb->rw_lock);
	assert(rc == 0);

	fprintf(stderr, "iofunc_close_ocb: ocb %p \n", ocb);

	return rc;
	//return iofunc_lock_ocb_default(ctp, reserved, ocb);
}

static void *
recv_thread(void *arg)
{
	dispatch_context_t * ctp_ret, * ctp = arg;

	while(1) {
		ctp_ret = dispatch_block(ctp);
		if(ctp_ret) {
			dispatch_handler(ctp);
		} else {
			fprintf(stderr, "dispatch_block() failed: %s\n",
					strerror( errno ) );
			break;
        }
	}
	return NULL;
}

int main( int argc, char **argv )
{
	iofunc_attr_t           iofunc_attr;
	resmgr_io_funcs_t       iofuncs;
	resmgr_connect_funcs_t  connect_funcs;
    resmgr_attr_t           resmgr_attr;
    message_attr_t          message_attr;
    dispatch_t            * dpp;
    dispatch_context_t    * ctp, *ctp_ret;
    int                     resmgr_id, message_id;

    /* Create the dispatch interface */
    dpp = dispatch_create();
    if( dpp == NULL ) {
        fprintf( stderr, "dispatch_create() failed: %s\n",
                 strerror( errno ) );
        return EXIT_FAILURE;
    }

    memset( &resmgr_attr, 0, sizeof( resmgr_attr ) );
    resmgr_attr.nparts_max = 2;
    resmgr_attr.msg_max_size = 2048;

    /* Setup the default I/O functions to handle open/read/write/... */
    iofunc_func_init( _RESMGR_CONNECT_NFUNCS, &connect_funcs,
                      _RESMGR_IO_NFUNCS, &iofuncs);
	iofuncs.devctl = io_devctl;
	iofuncs.lock_ocb = server_iofunc_lock_ocb;
	iofuncs.unlock_ocb = server_iofunc_unlock_ocb;
	iofuncs.close_ocb = server_iofunc_close_ocb;

	connect_funcs.open = io_open;

    /* Setup the attribute for the entry in the filesystem */
    iofunc_attr_init( &iofunc_attr, S_IFNAM | 0666, 0, 0 );

    resmgr_id = resmgr_attach(dpp, &resmgr_attr, "/dev/myresmgr", _FTYPE_ANY,
                               0, &connect_funcs, &iofuncs, &iofunc_attr);
    if( resmgr_id == -1 ) {
        fprintf( stderr, "resmgr_attach() failed: %s\n", strerror( errno ) );
        return EXIT_FAILURE;
    }

    /* Setup our message callback */
    memset( &message_attr, 0, sizeof( message_attr ) );
    message_attr.nparts_max = 2;
    message_attr.msg_max_size = 4096;

    /* Attach a callback (handler) for two message types */
    message_id = message_attach( dpp, &message_attr, _IO_MAX + 1,
			_IO_MAX + 2, io_message, NULL );
    if( message_id == -1 ) {
        fprintf( stderr, "message_attach() failed: %s\n", strerror( errno ) );
        return EXIT_FAILURE;
    }

    /* Setup a context for the dispatch layer to use */
    ctp = dispatch_context_alloc(dpp);
    if( ctp == NULL ) {
        fprintf( stderr, "dispatch_context_alloc() failed: %s\n",
                 strerror( errno ) );
        return EXIT_FAILURE;
    }
	pthread_t tid[2];
	int rc = pthread_create(&tid[0], NULL, recv_thread, (void*)ctp);
	assert(rc == 0);

    ctp = dispatch_context_alloc(dpp);
    if( ctp == NULL ) {
        fprintf( stderr, "dispatch_context_alloc() failed: %s\n",
                 strerror( errno ) );
        return EXIT_FAILURE;
    }

	rc = pthread_create(&tid[1], NULL, recv_thread, (void*)ctp);
	assert(rc == 0);

#if 0
    /* The "Data Pump" - get and process messages */
    while(1) {
        ctp_ret = dispatch_block(ctp);
        if( ctp_ret ) {
            dispatch_handler( ctp );
        }
        else {
            fprintf( stderr, "dispatch_block() failed: %s\n",
                     strerror( errno ) );
            return EXIT_FAILURE;
        }
    }
#endif
	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);
    return EXIT_SUCCESS;
}

#ifndef __RESMGR_TEST_COMMON_H
#define  __RESMGR_TEST_COMMON_H

struct io_msg {
    uint16_t msg_no;
    uint16_t msg_len;
	uint32_t msg_tid; /* client thread id */
    char     msg_data[4096];
};
struct io_reply {
	uint32_t msg_reply;
	uint32_t msg_data;
};

#endif

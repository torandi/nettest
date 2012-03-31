#ifndef PROTOCOL_H
#define PROTOCOL_H

#define PORT 7412

#define FRAME_SIZE 64
#define HASH_SIZE 4
#define PAYLOAD_SIZE 60

enum nw_var_type_t {
	NW_VAR_FLOAT,
	NW_VAR_UINT16,
	NW_VAR_UINT32,
	NW_VAR_CHAR,
	NW_VAR_STR
};

enum nw_cmd_t {
	NW_CMD_INVALID = 0,
	NW_CMD_TEST,
	NW_CMD_HAI,
	NW_CMD_BAI,
	NW_CMD_START_SEND,
	NW_CMD_START_RECV, 
	NW_CMD_START_RECV_READY,
	NW_CMD_SEND_DONE,
	NW_CMD_RESULT,
	NW_CMD_DOWNLOAD,
	NW_CMD_DOWNLOAD_COMPLETE,
	NW_CMD_ERROR
};

struct nw_var_t;

struct frame_t {
	nw_cmd_t cmd;
	int num_vars;
	nw_var_type_t var_types[PAYLOAD_SIZE-1]; //There can't be more than PAYLOAD_SIZE-1 num of variables in a frame
	void print(nw_var_t * vars);
};

extern frame_t protocol[];

#endif

#define __STDC_LIMIT_MACROS
#include <stdint.h>

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <math.h>
#include <time.h>

#include "sha1.h"
#include "network_lib.h"
#include "socket.h"
#include "protocol.h"

#define DEBUG 0

/*****
 * Contains network function shared by client and server
 *****/


static bool cmp_hash(char hash[HASH_SIZE],char * str,int len);
static void get_hash(char hash[HASH_SIZE],char * str,int len);

static bool get_frame(int sock, network_data_t * nd);

static int ftnw(float f, void * nw);
static int nwtf(void * nw, float * f);
static int strtnw(char * str, void * nw);
static int nwtstr(char * nw, char ** str);

addr_t no_addr;

/**
 * Sends a frame on the network. 
 * Returns false if the socket has closed or the write failed for some other reason
 */
bool send_frame(int sock, const addr_t &target, nw_cmd_t cmd, nw_var_t * vars) {
	char * nw = (char*)malloc(FRAME_SIZE+1);
	int pos = HASH_SIZE;	
	uint16_t nwi16;
	uint32_t nwi32;

	/**
	 * Select protocol cmd and write cmd to first pos
	 */
	if(protocol[cmd].cmd != cmd)
		fprintf(stderr, "WARNING: Protocol cmd #%d does not match value in frame_t struct\n",cmd);

	const int num_vars = protocol[cmd].num_vars;
	const nw_var_type_t * var_types = protocol[cmd].var_types;
	nw[pos] = cmd;
	pos+=sizeof(char);

	for(int i=0; i < num_vars; ++i) {
		switch(var_types[i]) {
			case NW_VAR_FLOAT:
				pos += ftnw(vars[i].f,nw+pos);
				break;
			case NW_VAR_UINT16:
				nwi16 = htons((uint16_t)vars[i].i);
				memcpy(nw+pos,&nwi16,sizeof(uint16_t));
				pos += sizeof(uint16_t);
				break;
			case NW_VAR_UINT32:
				nwi32 = htonl(vars[i].i);
				memcpy(nw+pos,&nwi32,sizeof(uint32_t));
				pos += sizeof(uint32_t);
				break;
			case NW_VAR_CHAR:
				nw[pos] = vars[i].c;
				pos += sizeof(char);
				break;
			case NW_VAR_STR:
				pos += strtnw(vars[i].str, nw+pos);
				break;
		}
		if(pos >= FRAME_SIZE) {
			fprintf(stderr, "Network frames are to small!\n");
			exit(2);
		}
	}

	char hash[41];
	get_hash(hash,nw+HASH_SIZE,PAYLOAD_SIZE);
	memcpy(nw,hash,HASH_SIZE);
	bool ret = send_raw(sock, nw, target);
	free(nw);
	return ret;
}

/**
 * Reads a frame from network and writes the result to vars.
 * Returns the frame (with data about protocol cmd and var types)
 * Fills addr with relevant address data about src
 */ 
frame_t read_frame(int sock, nw_var_t * vars, addr_t * addr) {
	network_data_t nw;
	if(get_frame(sock,&nw)) {
		int pos = 0;
		uint16_t nwi16;
		uint32_t nwi32;
		int cmd;

		//Read protocol cmd:
		cmd = nw[0];
		pos += sizeof(char);

		const nw_var_type_t * var_types = protocol[cmd].var_types;
		const int num_vars = protocol[cmd].num_vars;

#if DEBUG
		nw.print();
		printf("Recived cmd #%i\n", cmd);
#endif

		for(int i=0;i<num_vars;++i) {
			switch(var_types[i]) {
				case NW_VAR_FLOAT:
					pos += nwtf((char*)nw.data+pos, &vars[i].f);
					break;
				case NW_VAR_UINT16:
					memcpy(&nwi16, (char*)nw.data+pos,sizeof(uint16_t));
					vars[i].i = (uint32_t)ntohs(nwi16);
					pos += sizeof(uint16_t);
					break;
				case NW_VAR_UINT32:
					memcpy(&nwi32, (char*)nw.data+pos,sizeof(uint32_t));
					vars[i].i = ntohl(nwi32);
					pos += sizeof(uint32_t);
					break;
				case NW_VAR_CHAR:
					vars[i].c = ((char*)nw.data)[pos];
					pos += sizeof(char);
					break;
				case NW_VAR_STR:
					pos += nwtstr((char*)nw.data+pos, &vars[i].str);
					break;
			}
		}

		if(addr != NULL) {
			*addr = nw.addr;
		}
		frame_t f = {(nw_cmd_t)cmd, num_vars, {}};
		memcpy(f.var_types,var_types,num_vars*sizeof(nw_var_type_t));
		return f;
	} else {
		frame_t f = {NW_CMD_INVALID, 0, {}};
		return f;
	}
}

/***************************
 * static functions
 **************************/


/**
 * Recvs next frame from socket
 */
static bool get_frame(int sock, network_data_t * nd) {

	char buffer[FRAME_SIZE+1];

	int r = read_raw(sock,buffer,FRAME_SIZE, 0, &nd->addr);
	if(r != FRAME_SIZE) {
		fprintf(stderr,"Invalid frame size recived\n");
		nd->invalidate();
		return false;
	}
	if(cmp_hash(buffer,buffer+HASH_SIZE,PAYLOAD_SIZE)) {
		memcpy(nd->data,buffer+HASH_SIZE,PAYLOAD_SIZE);
		return true;
	} else {
		fprintf(stderr,"Incorrect hash for frame.\n");
		nd->invalidate();
		return false; 
	}
}

static void get_hash(char * hexstring,char * str, int len) {
	unsigned char hash[20];
	sha1::calc(str,len,hash);
	sha1::toHexString(hash, hexstring);
}

static bool cmp_hash(char hash[HASH_SIZE],char * str,int len) {
	char hexstring[41];
	get_hash(hexstring,str,len);
	return (strncmp(hash,hexstring,HASH_SIZE) == 0);
}


/** 
 * Converts the float to a network format and write it to nw, returns the number of bytes written (probably 3)
 */
static int ftnw(float f, void * nw) {
	memcpy(nw,&f, sizeof(f));
	return sizeof(f);
}

static int nwtf(void * nw, float * f) {
	*f = *((float*)nw);
	return sizeof(float);
}

static int strtnw(char * str, void * nw) {
	int len = strlen(str);
	((char*)nw)[0] = len;
	memcpy((char*)nw+1,str,len);
	return len+1;
}

/**
 * Reads a string from the network buffer and writes to str.
 */ 
static int nwtstr(char * nw, char ** str) {
	int len = nw[0];
	*str = (char*)malloc(len+1);
	(*str)[len] = 0x00;
	memcpy(*str,nw+1,len);
	return len+1;
}

void test_network() {
	void * nw = malloc(10);
	printf("Testing converting positive floats back and forth:\n");
	srand(time(NULL));
	int errors = 0;
	for(int i = 0; i<1000;++i) {
		int n = rand() % UINT16_MAX;
		float f = (float)n/(float)100.0;
		ftnw(f, nw);
		float b;
		nwtf(nw,&b);
		if(fabs(f-b) > 0.019) {
			printf("Error converting float %f: diff: %f (b: %f)\n", f, fabs(f-b),b);
			++errors;
		}
	}

	printf("Testing converting negative floats back and forth:\n");
	srand(time(NULL));
	for(int i = 0; i<1000;++i) {
		int n = rand() % UINT16_MAX;
		float f = (float)n/(float)100.0;
		f *=-1;
		ftnw(f, nw);
		float b;
		nwtf(nw, &b);
		if(fabs(f-b) > 0.019) {
			printf("Error converting float %f: diff: %f (b: %f)\n", f, fabs(f-b),b);
			++errors;
		}
	}
	float test[]={13.37};
	int count = 1;
	for(int i=0; i < count; ++i) {
		float f=test[i];
		ftnw(f, nw);
		float b;
		nwtf(nw, &b);
		if(fabs(f-b) > 0.019) {
			printf("Error converting float %f: diff: %f (b: %f)\n", f, fabs(f-b),b);
			++errors;
		}
	}
	printf("Number of errors: %d/%d\n",errors,2000+count);
	free(nw);

	printf("Setting up broadcast test on port 4711\n");
	int sock1 = create_udp_socket(4711, true);
	int sock2 = create_udp_socket(4711, true);
	nw_var_t vars[5];
	nw_var_t vars_r[5];
	vars[0].i = 17;
	vars[1].f = 13.371;
	vars[2].c = 'z';
	vars[3].i = (uint32_t)4000000000;

	vars[4].set_str("hailol");
	send_frame(sock1, broadcast_addr(4711), NW_CMD_TEST, vars);
	printf("Waiting\n");
	count=0;
	while(!data_available(sock2, 0,0 )) {
		++count;
		if(count > 1000) {
			printf("Broadcast recive est failed, no data recieved\n");
			exit(-1);
		}
		usleep(100);
	}
	printf("\n");
	addr_t addr;
	frame_t f = read_frame(sock2, vars_r, &addr);
	if(f.cmd == NW_CMD_INVALID) {
		printf("Invalid package recived\n");
	} else {
		if(f.cmd == NW_CMD_TEST)
			printf("Recived correct package:\n");
		else
			printf("Recived wrong package:\n");
		f.print(vars);
	}
	printf("%s\n", vars[4].str);
	close_socket(sock1);
	close_socket(sock2);
}


/***************
 * nw_var_t
 **************/

nw_var_t::nw_var_t() : str(NULL) {};

nw_var_t::~nw_var_t() {
	if(str!=NULL)
		free(str);
}

void nw_var_t::set_str(const char * string) {
	str = new char[strlen(string)];
	strcpy(str, string);
}

/****************
 * network_data_t
 ***************/


network_data_t::network_data_t() {
	_valid = true;
	data = malloc(PAYLOAD_SIZE);
}

network_data_t::network_data_t(network_data_t &nd) {
	data = nd.data;
	addr = nd.addr;
	_valid = nd._valid;
	nd._valid = false;
	nd.data = NULL;
	_valid = true;
}

network_data_t::~network_data_t() {
	if(_valid)
		free(data);
}

char &network_data_t::operator[] (int index) {
	if(_valid && index < PAYLOAD_SIZE ) {
		return *((char*)data + index);
	} else if(!_valid) {
		throw "Reading from invalidated network data";
	} else  {
		throw "Index out of bounds";
	}
}

void network_data_t::invalidate() {
	if(_valid) {
		_valid = false;
		free(data);
		data = NULL;
	}
}

bool network_data_t::valid() {
	return _valid;
}

void network_data_t::print() {
	printf("network_data_t: { ");
	for(int i=0;i<PAYLOAD_SIZE; ++i) {
		if((*this)[i] >= 32 && (*this)[i] <=126) {
			printf("[%i=%c] ", (*this)[i], (*this)[i]);
		} else {
			printf("%i ", (*this)[i]);
		}
	}
	printf("}\n");
}

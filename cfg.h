#ifndef cfgh
#define cfgh

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h> 

#include <signal.h>
#include "util.h"

#define kfootsize ( 2048 * 1024 )

struct txconf_s;  // forward decl to permit inception 
struct rxconf_s;  // forward decl to permit inception 

#define preamble_cannon_ul 0xa5a5a5a55a5a5a5a 
	// opcodes for signaling oob status to remote end
#define feed_more 0xcafe 
#define end_of_millipede 0xdead  
// the bearer channel header
struct millipacket_s { 
	unsigned long preamble;  // shoudl always be preamble_cannon_ul constant
	unsigned long leg_id; //  leg_id= ( streampos  % leg_size ) is the main sequencer for the whole session
	unsigned long size;
	int	checksum; 
	int 	opcode; 
	
};
struct txworker_s {
	int id; 
	struct txconf_s *txconf_parent;
	char state;
	pthread_mutex_t mutex;
	pthread_t thread; 
	int sockfd ;
	u_char * buffer; 
	struct millipacket_s pkt;
	int writeremainder;
	FILE * pip;
	
};

struct target_port_s {
	char * name; 
	unsigned short port ; 
};
struct txconf_s {
	int worker_count;
	struct timespec ticker; 
	u_long stream_total_bytes; 
	struct txworker_s workers[16];	//XXX make dynamic??? 
	int target_port_count; 	
	struct target_port_s target_ports[16]; 
	pthread_mutex_t mutex;
};



struct rxworker_s {
	int id;
	int sockfd; 
	int socknum; 
	struct rxconf_s *rxconf_parent;
	pthread_mutex_t mutex; 
	pthread_t thread;
};
struct rxconf_s {
	int workercount; 
	struct sockaddr_in sa;  // reusable bound sa for later accepts
	int socknum ;  // reusable bound socket number  later accepts
	unsigned short port; 
	struct rxworker_s workers[17]; 
	int next_leg ;  // main sequencer to monotonically order legs to stdout
	int done_mbox; 
} ;

	
#endif

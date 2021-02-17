#ifndef cfgh
#define cfgh

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "util.h"
#include <signal.h>

#define kfootsize                                                              \
  (2048 * 1024) // 2M is experimentally nice; however a PMC driven cache/use
                // study  would be great to inform this
#define kthreadmax 16
#define kreconnectlimit 512

struct txconf_s; // forward decl to permit inception
struct rxconf_s; // forward decl to permit inception
struct ioconf_s; // forward decl to permit inception

#define preamble_cannon_ul 0xa5a5a5a55a5a5a5a
// opcodes for signaling oob status to remote end
#define feed_more 0xcafe
#define end_of_millipede 0xdead
// the bearer channel header
struct millipacket_s {
  u_long preamble; // == preamble_cannon_ul constant,  mostly
                          // superstition against getting alien transmissions
  u_long leg_id;   // leg_id= ( streampos  % leg_size ) is the main
                          // sequencer for the whole session
  // this may result in max transmission size of klegsize * ( unsigned int max )
  // XXX debug  sequencer rollover condition if this is a problem
  u_long size; // <= kfootsize;  how much user payload is in this packet
  u_long
      checksum; //  checksum = mix ( leg_id, opcode, sample ( payload) );
  u_long  opcode;
};
struct txworker_s {
  int id;
  struct txconf_s *txconf_parent;
  char state;
  pthread_mutex_t mutex;
  pthread_t thread;
  int sockfd;
  u_char *buffer;
  struct millipacket_s pkt;
  int writeremainder;
};

struct target_port_s {
  char *name;
  unsigned short port;
};
struct txconf_s {
  int worker_count;
  struct timespec ticker;
  u_long stream_total_bytes;
  struct txworker_s workers[kthreadmax]; // XXX make dynamic???
  int target_port_count;
  int target_port_cursor;
  struct target_port_s target_ports[kthreadmax];
  pthread_mutex_t mutex;
  pthread_t ingest_thread;
  int input_eof;
  int done;
  int input_fd;
  u_long waits;
  struct ioconf_s *ioconf;
};

struct rxworker_s {
  int id;
  char state;
  u_long leg;
  u_long legop;
  int sockfd;
  int socknum;
  struct rxconf_s *rxconf_parent;
  pthread_mutex_t mutex;
  pthread_t thread;
};
struct rxconf_s {
  int workercount;
  struct sockaddr_in sa;    // reusable bound sa for later accepts
  pthread_mutex_t sa_mutex; //
  int socknum;              // reusable bound socket number  later accepts
  unsigned short port;
  struct rxworker_s workers[kthreadmax];
  volatile u_long
      next_leg; // main sequencer to monotonically order legs to stdout
  volatile int done_mbox;
  pthread_mutex_t rxmutex;
  pthread_cond_t seq_cv;
  int output_fd;
  struct ioconf_s *ioconf;
};

struct ioconf_s {
  unsigned terminate_port;
  int terminate_socket;
  char *initiate_host;
  unsigned short initiate_port;
  int initiate_socket;
};
#include "VMPDTRACEpolyfill.h"
#endif

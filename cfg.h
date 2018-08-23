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
#include <unistd.h>

#include "util.h"
#include <signal.h>

#define kfootsize                                                              \
  (2048 * 1024) // 2M is experimentally nice; however a PMC driven cache/use
                // study  would be great to inform this
#define kthreadmax 16

struct txconf_s; // forward decl to permit inception
struct rxconf_s; // forward decl to permit inception

#define preamble_cannon_ul 0xa5a5a5a55a5a5a5a
// opcodes for signaling oob status to remote end
#define feed_more 0xcafe
#define end_of_millipede 0xdead
// the bearer channel header
struct millipacket_s {
  unsigned long preamble; // == preamble_cannon_ul constant,  mostly
                          // superstition against getting alien transmissions
  unsigned long leg_id;   // leg_id= ( streampos  % leg_size ) is the main
                          // sequencer for the whole session
  // this may result in max transmission size of klegsize * ( unsigned int max )
  // XXX debug  sequencer rollover condition if this is a problem
  unsigned long size; // <= kfootsize
  unsigned long
      checksum; //  checksum = mix ( leg_id, opcode, sample ( payload) );
  int opcode;
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
  int done;
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
  struct sockaddr_in sa;    // reusable bound sa for later accepts
  pthread_mutex_t sa_mutex; //
  int socknum;              // reusable bound socket number  later accepts
  unsigned short port;
  struct rxworker_s workers[kthreadmax];
  int next_leg; // main sequencer to monotonically order legs to stdout
  int done_mbox;
  pthread_mutex_t rxmutex;
};

#endif

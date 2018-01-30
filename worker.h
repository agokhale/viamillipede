#ifndef workerh
#define workerh

#include "cfg.h"
void tx (struct txconf_s*); 
void txstatus (struct txconf_s *, int log_level); 
void rx ( struct rxconf_s*); 
int tcp_connect (  char * host, int port );
int tcp_recieve_prep ( struct sockaddr_in * sa, int * socknum,  int inport);
int tcp_accept (struct sockaddr_in * sa, int  socknum );

#endif

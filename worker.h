#ifndef workerh
#define workerh

#include "cfg.h"
unsigned long *prbs_gen(unsigned long *payload, unsigned long permutation,
                        size_t size);
int prbs_verify(unsigned long *payload, unsigned long permutation, size_t size);
int terminate(struct txconf_s *txconf, struct rxconf_s *rxconf,
              struct ioconf_s *ioconf);
int initiate(struct txconf_s *txconf, struct rxconf_s *rxconf,
             struct ioconf_s *ioconf);
void txingest(struct txconf_s *txconf);
void tx(struct txconf_s *);
void tx_rate_report();
char tx_state(struct txworker_s *txworker);
char tx_state_set(struct txworker_s *txworker, char instate);
int tx_poll(struct txconf_s *);
void txstatus(struct txconf_s *, int log_level);
void rx(struct rxconf_s *);
int rx_poll(struct rxconf_s *);
void rxinfo(struct rxconf_s *);
int tcp_connect(char *host, int port);
int tcp_receive_prep(struct sockaddr_in *sa, int *socknum, int inport);
int tcp_accept(struct sockaddr_in *sa, int socknum);
void tcp_dump_sockfdparams(int sockfd);
int initiate_relay();
int tcp_getsockinfo1( int si,int whatsel );

void tcp_dumpinfo(int sfd );
#endif

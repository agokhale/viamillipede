#include "util.h"
#include "worker.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#ifdef CHAOS
extern unsigned long gchaos;
extern unsigned long gchaoscounter;
int chaos_fail() {
  // give up sometimes
  if ((gchaoscounter-- == 1) && gchaos) {
    gchaoscounter = gchaos;
    whisper(1, "chaos inserted");
    return (1);
  }
  return (0);
}
#elseif
#define chaos_fail err;
#endif

ssize_t bufferfill(int fd, u_char *__restrict dest, size_t size, int charmode) {
  /** read until a buffer completes or EOF
  charmode: be forgiving for small reads if 1
  return: the output size
  */
  int remainder = size;
  u_char *dest_cursor = dest;
  ssize_t accumulator = 0;
  ssize_t readsize;
  int sleep_thief = 0;
  assert(dest != NULL);
  do {
    if (errno != 0) {
      whisper(3, "ignoring err %i\n", errno);
      checkperror("bufferfill nuisance err");
      errno = 0;
    }
    // reset nuisances
    readsize = read(fd, dest_cursor, MIN(MAXBSIZE, remainder));
    if (errno != 0) {
      checkperror("bufferfill read err");
      whisper(3, "errno: %i  readsize %ld requestedsiz: %d  fd:%i dest:%p \n",
              errno, readsize, MIN(MAXBSIZE, remainder), fd, dest_cursor);
    }
    if (readsize < 0) {
      whisper(2, "negative read");
      perror("negread");
      break;
    } else {
      remainder -= readsize;
      assert(remainder >= 0);
      accumulator += readsize;
      dest_cursor += readsize;
      if (readsize < MAXBSIZE && !charmode) {
        // discourage tinygrams - they just beat us up and chew the cpu
        // XXX histogram the readsize and use ema to track optimal effort
        sleep_thief++;
        usleep(sleep_thief);
      } else {
        sleep_thief = 0;
      }
      if (readsize < 1 || charmode) {
        // short reads  are the end
        // alternately, exit if we are in char mode
        break;
      }
    }
  } while (remainder > 0);
  return (accumulator);
}
void stopwatch_start(struct timespec *t) {
  assert(clock_gettime(CLOCK_UPTIME, t) == 0);
}
u_long stopwatch_stop(struct timespec *t) {
  //  stop the timer started at t
  // returns usec resolution diff of time
  int retc = 1;
  struct timespec stoptime;
  while (retc != 0) {
    retc = clock_gettime(CLOCK_UPTIME, &stoptime);
    // this can fail errno 4 EINTR
  }
  if (errno == EINTR) {
    errno = 0;
  }
  time_t secondsdiff = stoptime.tv_sec - t->tv_sec;
  long nanoes = stoptime.tv_nsec - t->tv_nsec;
  // microoptimization trick to avoid a branch
  // borrow a billion seconds
  nanoes += 1000000000;
  secondsdiff--;
  // rely on truncation to correct the seconds place
  secondsdiff += nanoes / 1000000000;
  /* simple way relies on a branch
  if (nanoes < 0) {
    // borrow billions place nanoseconds to come up true
    nanoes += 1000000000;
    secondsdiff--;
  }
  */
  u_long ret = (secondsdiff * 1000000) + (nanoes / 1000); // in usec
  return ret;
}
/*https://fossies.org/linux/iperf/src/tcp_info.c*/
#define W8(A) printf ("\t"#A":\t %d \n",(int)linfo.A);
#define W32(A) printf ("\t"#A":\t %x \n",(unsigned int)linfo.A);
void tcp_dumpinfo(int sfd )   {
  struct tcp_info linfo; 
  socklen_t infolen = sizeof ( linfo); 
  getsockopt(sfd, IPPROTO_TCP, TCP_INFO, &linfo, &infolen);
  checkperror("__FUNCTION__");
  whisper ( 6, "tcpinfo: fd %x \n",sfd);
  whisper ( 6, "  tcpi_snd_mss: %d \n",linfo.tcpi_snd_mss);
  W8(tcpi_state)
  W8(__tcpi_ca_state)
  W8(__tcpi_retransmits)
  W8(__tcpi_probes)
  W8(__tcpi_backoff)
  W8(tcpi_options)
  W8(tcpi_snd_wscale)
  W8 (tcpi_rcv_wscale )
  W32       (tcpi_rto              )
  W32       (__tcpi_ato)
  W32       (tcpi_snd_mss         )
  W32       (tcpi_rcv_mss        )
  W32       (__tcpi_unacked)
  W32       (__tcpi_sacked)
  W32       (__tcpi_lost)
  W32       (__tcpi_retrans)
  W32       (__tcpi_fackets)
  W32       (__tcpi_last_data_sent)
  W32       (__tcpi_last_ack_sent   )
  W32       (tcpi_last_data_recv   )
  W32       (__tcpi_last_ack_recv)
  W32       (__tcpi_pmtu)
  W32       (__tcpi_rcv_ssthresh)
  W32       (tcpi_rtt               )
  W32       (tcpi_rttvar           )
  W32       (tcpi_snd_ssthresh    )
  W32       (tcpi_snd_cwnd       )
  W32       (__tcpi_advmss)
  W32       (__tcpi_reordering)
  W32       (__tcpi_rcv_rtt)
  W32       (tcpi_rcv_space        )
        /* FreeBSD extensions to tcp_info. */
  W32       (tcpi_snd_wnd           )
  W32       (tcpi_snd_bwnd          )
  W32       (tcpi_snd_nxt          )
  W32       (tcpi_rcv_nxt         )
  W32       (tcpi_toe_tid           )
  W32       (tcpi_snd_rexmitpack   )
  W32       (tcpi_rcv_ooopack     )
  W32       (tcpi_snd_zerowin    )

  
}
int tcp_geterr( int sfd) {
    int sockerr;
    u_int sockerrsize = sizeof(sockerr); // uhg
    getsockopt(sfd, SOL_SOCKET, SO_ERROR, &sockerr, &sockerrsize);
    checkperror("__FUNCTION__");
    return sockerr;
}
int tcp_nowait( int si) {
	int val=1;
	socklen_t outsiz=sizeof(val);
	setsockopt( si,IPPROTO_TCP, TCP_NODELAY, &val, outsiz);
	checkperror(__FUNCTION__);
	return val;
}
int tcp_setbufsize( int si) {
#define MEGA_BYTES (1024*1024)
	int val= 1 * MEGA_BYTES;
	socklen_t vsiz=sizeof(val);
	setsockopt( si,SOL_SOCKET, SO_RCVBUF, &val, vsiz); //nice to have
	setsockopt( si,SOL_SOCKET, SO_SNDBUF, &val, vsiz);  // absolutely necessary or single host lo0 use will lock up viamillipede
	checkperror("nowait");
	
	tcp_geterr(si);
	return val;
}
int tcp_getsockinfo1( int si,int whatsel ) {
	int outval=0;
	socklen_t outsiz=sizeof(outval);
	getsockopt( si,SOL_SOCKET, whatsel, &outval ,&outsiz);
	return outval;
}
void tcp_dump_sockfdparams ( int sfd) {
  if ( sfd > 0 ){ 
    checkperror(" wat?"); 
    whisper( 16, "%s:%x ","RCVBUF", tcp_getsockinfo1( sfd,SO_RCVBUF));
    checkperror(" RCVwat?"); 
    whisper( 16, "%s:%x ","SO_SNDBUF", tcp_getsockinfo1( sfd,SO_SNDBUF));
    checkperror(" SNDBUFwat?"); 
    whisper( 16, "%s:%x ","SO_SNDLOWAT", tcp_getsockinfo1( sfd,SO_SNDLOWAT));
    checkperror(" LOWATwat?"); 
    whisper( 16, "%s:%x ","SO_RCVLOWAT", tcp_getsockinfo1( sfd,SO_RCVLOWAT));
    checkperror(" RCVlowatwat?"); 
    whisper( 16, "\nsocketfd:%i\n ",sfd);
  }
}

// return a connected socket fd
int tcp_connect(char *host, int port) {
  int ret_sockfd = -1;
  int retcode;
  struct hostent *lhostent;
  struct addrinfo laddrinfo;
  struct sockaddr_in lsockaddr;
  // struct sockaddr lsockaddr;
  lhostent = gethostbyname(host);
  if (h_errno != 0) {
    herror("gethostenterror");
    whisper (1,"hostent error for host:%s", host );
  }
  assert(h_errno == 0 && "hostname fishy");
  lsockaddr.sin_family = AF_INET;
  lsockaddr.sin_port = htons(port);
  memcpy(&(lsockaddr.sin_addr), lhostent->h_addr_list[0],
         sizeof(struct in_addr)); // y u hate c?
  ret_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  checkperror ( "socket()");
  assert(ret_sockfd > 0 && "socket fishy");
  retcode = connect(ret_sockfd, (struct sockaddr *)&(lsockaddr),
                    sizeof(struct sockaddr));
  checkperror ( "connect()");
  if (retcode != 0) {
    whisper(1, "tx: connect failed to %s:%d fd: %i \n", host, port, ret_sockfd);
    // our only output is the socketfd, so trash it
    ret_sockfd = -1;
  } else {
    assert ( 0 == tcp_geterr(ret_sockfd));
    checkperror ( "geterr");
  }
  if (retcode != 0) {
    whisper(8, "        connected to %s:%i\n", host, port);
    tcp_nowait( ret_sockfd); 
    tcp_setbufsize( ret_sockfd); 
    tcp_dump_sockfdparams(ret_sockfd);
    checkperror("sopt");
  }
  checkperror ( "tcp_connection ?? ");
  return (ret_sockfd);
}
int tcp_receive_prep(struct sockaddr_in *sa, int *socknum, int inport) {
  int one = 1;
  int retcode;
  int lsock = -1;
  *socknum = socket(AF_INET, SOCK_STREAM, 0);
  sa->sin_family = AF_INET;
  sa->sin_addr.s_addr = htons(INADDR_ANY);
  sa->sin_port = htons(inport);
  whisper(7, "bind sockid: %i\n", *socknum);
  setsockopt(*socknum, SOL_SOCKET, SO_REUSEPORT, (char *)&one, sizeof(one));
  retcode = bind(*socknum, (struct sockaddr *)sa, sizeof(struct sockaddr_in));
  if (retcode != 0) {
    perror("bind failed");
    assert(0);
  }
  checkperror("bindfail");
  whisper(9, "      listen sockid: %i\n", *socknum);
  retcode = listen(*socknum, 6);
  if (retcode != 0) {
    perror("listen fail:\n");
    assert(0);
  };
  return retcode;
}

int tcp_accept(struct sockaddr_in *sa, int socknum) {
  int out_sockfd;
  socklen_t socklen = sizeof(struct sockaddr_in);
  whisper(17, "   accept sockid: %i\n", socknum);
  out_sockfd = accept(socknum, (struct sockaddr *)sa, &socklen);
  whisper(13, "   socket %i accepted to fd:%i \n", socknum, out_sockfd);
  DTRACE_PROBE1(viamillipede, worker__connected, out_sockfd);
  checkperror("acceptor");
  tcp_dump_sockfdparams(out_sockfd);
  return (out_sockfd);
}

unsigned long mix(unsigned int seed, void *data, unsigned long size) {

  unsigned acc = seed;
  for (unsigned long cursor = 0; cursor <= size;
       cursor +=
       sizeof(unsigned long)) { // XX we miss the tail for unaligned sizes
    acc += *(unsigned long *)data + cursor;
  }
  return (acc);
}

#include "util.h"
#include "worker.h"
#include <sys/socket.h>
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

ssize_t bufferfill(int fd, u_char *__restrict dest, size_t size, int charmode) {
  /** read until a buffer completes or EOF
  charmode: be forgiving for small reads if 1
  return: the output size
  */
  int remainder = size;
  u_char *dest_cursor = dest;
  ssize_t accumulator = 0;
  ssize_t readsize;
  ///fuseint fuse = 55; // don't spin  forever
  int sleep_thief = 0;
  assert(dest != NULL);
  do {
    if (errno != 0) {
      whisper(3, "ignoring sir %i\n", errno);
      checkperror("bufferfill nuiscance err");
      errno = 0;
    }
    // reset nuiscances
    readsize = read(fd, dest_cursor, MIN(MAXBSIZE, remainder));
    if (errno != 0) {
      checkperror("bufferfill read err");
      whisper(3, "erno: %i  readsize %ld requestedsiz: %d  fd:%i dest:%p \n",
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
        // XXX histgram the readsize and use ema to track optimal effort
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
  //} while ((remainder > 0) && (fuse-- > 0));
  //assert((fuse > 1) && "fuse blown");
  return (accumulator);
}
void stopwatch_start(struct timespec *t) {
  assert(clock_gettime(CLOCK_UPTIME, t) == 0);
}
int stopwatch_stop(struct timespec *t, int whisper_channel) {
  //  stop the timer started at t
  // returns usec resolution diff of time
  struct timespec stoptime;
  assert(clock_gettime(CLOCK_UPTIME, &stoptime) == 0);
  time_t secondsdiff = stoptime.tv_sec - t->tv_sec;
  long nanoes = stoptime.tv_nsec - t->tv_nsec;
  // microoptimization trick to avoid a branch 
  // borrow a billion seconds
  nanoes += 1000000000;
  secondsdiff--;
  //rely on truncation to correct the seconds place
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
// return a connected socket fd
int tcp_connect(char *host, int port) {
  int ret_sockfd = -1;
  int retcode;
  struct hostent *lhostent;
  struct addrinfo laddrinfo;
  struct sockaddr_in lsockaddr;
  // struct sockaddr lsockaddr;
  lhostent = gethostbyname(host);
  if (h_errno != 0)
    herror("gethostenterror");
  assert(h_errno == 0 && "hostname fishy");
  lsockaddr.sin_family = AF_INET;
  lsockaddr.sin_port = htons(port);
  memcpy(&(lsockaddr.sin_addr), lhostent->h_addr_list[0],
         sizeof(struct in_addr)); // y u hate c?
  ret_sockfd = socket(AF_INET, SOCK_STREAM, 0);
  assert(ret_sockfd > 0 && "socket fishy");
  retcode = connect(ret_sockfd, (struct sockaddr *)&(lsockaddr),
                    sizeof(struct sockaddr));
  if (retcode != 0) {
    whisper(1, "tx: connect failed to %s:%d fd: %i \n", host, port, ret_sockfd);
    // our only output is the socketfd, so trash it
    ret_sockfd = -1;
    whisper(19, "trashing fd to fail  %s:%d fd: %i \n", host, port, ret_sockfd);
  } else {
    int sockerr;
    u_int sockerrsize = sizeof(sockerr); // uhg
    getsockopt(ret_sockfd, SOL_SOCKET, SO_ERROR, &sockerr, &sockerrsize);
    checkperror("connect");
    assert(sockerr == 0);
    whisper(8, "        connected to %s:%i\n", host, port);
  }
  return (ret_sockfd);
}
int tcp_recieve_prep(struct sockaddr_in *sa, int *socknum, int inport) {
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

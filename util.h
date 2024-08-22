#ifndef utilh
#define utilh
#ifdef VMPDTRACE
  #include <sys/sdt.h>
#endif
#ifndef MAXBSIZE
  //Freebsd defines a civilized largest single transaction; 
  //do something elsewhere
  #define MAXBSIZE 16384
  //#warning "MAXBSIZE unsupported, using 16k"
#endif
#ifndef CLOCK_UPTIME
  //Polyfill clock
  #define CLOCK_UPTIME CLOCK_REALTIME
  //#notify "CLOCK_UPTIME unsupported"
#endif
#ifndef EDOOFUS
  #define EDOOFUS ENETDOWN
#endif
#ifndef EBADRPC
  #define EBADRPC EKEYEXPIRED
#endif

#include <sys/types.h>
extern int gverbose;
ssize_t bufferfill(int fd, u_char *__restrict dest, size_t size, int charmode);
void stopwatch_start(struct timespec *t);
u_long stopwatch_stop(struct timespec *t);
#ifdef CHAOS
int chaos_fail();
#endif
unsigned long mix(unsigned int seed, void *data, unsigned long size);
#define whisper(level, ...)                                                    \
  {                                                                            \
    if (level < gverbose)                                                      \
      fprintf(stderr, __VA_ARGS__);                                            \
  }
#define checkperror(...)                                                       \
  do {                                                                         \
    if (errno != 0) {                                                          \
      perror(__VA_ARGS__);                                                     \
      whisper(1,"checkperror %s:%i",__FUNCTION__,__LINE__);                           \
    }                                                                          \
  } while (0);
#endif

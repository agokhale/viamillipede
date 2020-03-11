#ifndef utilh
#define utilh
#ifdef _dtrace
	#include <sys/sdt.h>
#else
	#define DTRACE_PROBE(...)  
	#define DTRACE_PROBE1(...)  
	#define MAXBSIZE 4096
	//hereticalXXXX
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
      whisper(1,"checperror %s:%i",__FUNCTION__,__LINE__);                           \
    }                                                                          \
  } while (0);
#endif

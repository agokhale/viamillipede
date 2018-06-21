#ifndef utilh
#define utilh
#include <sys/types.h>
extern int gverbose;
ssize_t bufferfill(int fd, u_char *__restrict dest, size_t size);
void stopwatch_start(struct timespec *t);
int stopwatch_stop(struct timespec *t, int whisper_channel);
int chaos_fail();
unsigned long mix(unsigned int seed, void *data, unsigned long size);
#define whisper(level, ...)                                                    \
  {                                                                            \
    if (level < gverbose)                                                      \
      fprintf(stderr, __VA_ARGS__);                                            \
  }
#define checkperror(...)                                                       \
  do {                                                                         \
    if (errno != 0)                                                            \
      perror(__VA_ARGS__);                                                     \
  } while (0);
#endif

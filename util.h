#ifndef utilh
#define utilh
extern int gverbose;
ssize_t bufferfill ( int fd, u_char * dest, size_t size );
void stopwatch_start (struct  timespec * t ); 
int stopwatch_stop ( struct  timespec * t  , int  whisper_channel);
#define whisper( level, ...)  { if ( level < gverbose ) fprintf(stderr,__VA_ARGS__); }
#define checkperror( ... )   do {if (errno != 0) perror ( __VA_ARGS__ );  } while (0); 
#endif


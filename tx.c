#include "worker.h"
extern int gchecksums;
extern int gcharmode;
extern char *gcheckphrase;

int dispatch_idle_worker(struct txconf_s *txconf) {
  int retcode = -1;
  txstatus(txconf, 5);
  int sleep_thief = 0;
  while (retcode < 0) {
    pthread_mutex_lock(&(txconf->mutex));
    for (int worker_cursor = 0;
         (worker_cursor < txconf->worker_count) && (retcode < 0);
         worker_cursor++) {
      if (txconf->workers[worker_cursor].state == 'i') {
        // we have a idle volunteer. winner!
        retcode = worker_cursor;
        // if there are any idle workers, erase memory of sleeping
        sleep_thief = 0;
      }
    }
    assert(retcode < kthreadmax);
    pthread_mutex_unlock(&(txconf->mutex));
    if (retcode < 0) {
      sleep_thief++;
      // investigate backoffs that are smartr. probably not helpful
      // sleep_thief <<= 1;
      // sleeping here indicates we do not have enough workers
      //   or enough througput on the network
      usleep(sleep_thief);
    }
  } // we have a winner, return it
  assert(retcode < kthreadmax);
  return (retcode);
}

void start_worker(struct txworker_s *txworker) {
  assert(txworker->id < kthreadmax);
  pthread_mutex_lock(&(txworker->txconf_parent->mutex));
  txworker->state = 'd';
  pthread_mutex_unlock(&(txworker->txconf_parent->mutex));
  txstatus(txworker->txconf_parent, 6);
}

// dump <source>/stdin in chunks
// this is single thread
void txingest(struct txconf_s *txconf) {
  int readsize;
  int foot_cursor = 0;
  unsigned long saved_checksum = 0xff;
  int ingest_leg_counter = 0;
  txconf->stream_total_bytes = 0;
  checkperror("nuisancse ingest err");
  pthread_mutex_lock(&(txconf->mutex));
  while (!txconf->done) {
    pthread_mutex_unlock(&(txconf->mutex));
    int worker = dispatch_idle_worker(txconf);
    assert((txconf->workers[worker].buffer) != NULL);
    readsize =
        bufferfill(txconf->input_fd, (u_char *)(txconf->workers[worker].buffer),
                   kfootsize, gcharmode);
    whisper(8, "\ntxw:%02i read leg %i : fd:%i siz:%i\n", worker,
            ingest_leg_counter, txconf->input_fd, kfootsize);
    assert(readsize <= kfootsize);
    if (readsize > 0) {
      // find the idle worker , lock it and dispatch as separate calls --
      // perhaps
      txconf->workers[worker].pkt.size = readsize;
      txconf->workers[worker].pkt.leg_id = ingest_leg_counter;
      txconf->workers[worker].pkt.opcode = feed_more;
      if (gchecksums) {
        // expensive this code should only be used for development
        txconf->workers[worker].pkt.checksum =
            mix(saved_checksum + ingest_leg_counter,
                txconf->workers[worker].buffer, readsize);
        saved_checksum = txconf->workers[worker].pkt.checksum;
      } else {
        txconf->workers[worker].pkt.checksum = 0;
      }
      start_worker(&(txconf->workers[worker]));
      txconf->stream_total_bytes += readsize;
    } else {
      whisper(4, "txingest no more stdin, send shutdown");
      pthread_mutex_lock(&(txconf->mutex));
      txconf->done = 1;
      txconf->input_eof = 1;
      pthread_mutex_unlock(&(txconf->mutex));
      int worker = dispatch_idle_worker(txconf);
      txconf->workers[worker].pkt.size = 0;
      txconf->workers[worker].pkt.leg_id = ingest_leg_counter;
      txconf->workers[worker].pkt.opcode = end_of_millipede;
      start_worker(&(txconf->workers[worker]));
    }
    ingest_leg_counter++;
    pthread_mutex_lock(&(txconf->mutex));
  }
  txconf->done = 1;
  pthread_mutex_unlock(&(txconf->mutex));
  whisper(4, "tx:ingest complete for %lu(bytes) in \n\n",
          txconf->stream_total_bytes);
  txstatus(txconf, 5);
  u_long usecbusy = stopwatch_stop(&(txconf->ticker), 4);
  // bytes per usec - thats interesting
  whisper(2, " %8.4f MBps in  %ld(us)\n",
          (txconf->stream_total_bytes ) / (1.0 * usecbusy), usecbusy);
}

int txpush(struct txworker_s *txworker) {
  // push this buffer out the socket
  int retcode = 1;
  int writelen = -1;
  int cursor = 0;
  txworker->state = 'a'; // preAmble
  int pktwrret =
      write(txworker->sockfd, &txworker->pkt, sizeof(struct millipacket_s));
  assert(pktwrret == sizeof(struct millipacket_s) &&
         "millipacket not written ");
  txworker->writeremainder = txworker->pkt.size;
  assert(txworker->writeremainder <= kfootsize);
  while (txworker->writeremainder && retcode) {
    int minedsize = MIN(MAXBSIZE, txworker->writeremainder);
    txworker->state = 'P'; // 'P'ush
    if (chaos_fail()) {
      close(txworker->sockfd); // things fail sometimes
    }
    writelen =
        write(txworker->sockfd, ((txworker->buffer) + cursor), minedsize);
    if (errno != 0) {
      retcode = 0;
    };
    checkperror(" write to socket");
    txworker->state = 'p'; // 'p'ush complete
    txworker->writeremainder -= writelen;
    cursor += writelen;
    whisper(101, "txw:%02i psh leg:%lu.(+%i -%i)  ", txworker->id,
            txworker->pkt.leg_id, writelen, txworker->writeremainder);
  }
  checkperror("writesocket");
  // assert ( writelen );
  pthread_mutex_lock(&(txworker->txconf_parent->mutex));
  if (retcode == 1) {
    assert(txworker->writeremainder == 0);
    txworker->pkt.size = 0; /// can't distroythis  un less  we are successful
    txworker->state = 'i';  // idle ok
  } else {
    DTRACE_PROBE(viamillipede, leg__drop);
    txworker->state =
        'x'; // dead  do not transmit more; save state and do it again
    whisper(7, "rxw:%02i is dead\n", txworker->id);
  }
  pthread_mutex_unlock(&(txworker->txconf_parent->mutex));
  return retcode;
}
int tx_tcp_connect_next(struct txconf_s *txconf) {
  // pick a port/host from the list, cycleing through  them
  int chosen_target = txconf->target_port_cursor++;
  txconf->target_port_cursor %= txconf->target_port_count;
  assert(txconf->target_port_cursor < txconf->target_port_count);
  whisper(5, "tx: chosen target %s %d\n",
          txconf->target_ports[txconf->target_port_cursor].name,
          txconf->target_ports[txconf->target_port_cursor].port);
  DTRACE_PROBE2(viamillipede, worker__connect,
                txconf->target_ports[txconf->target_port_cursor].name,
                txconf->target_ports[txconf->target_port_cursor].port);
  return (tcp_connect(txconf->target_ports[chosen_target].name,
                      txconf->target_ports[chosen_target].port));
}

extern char *gcheckphrase;
int tx_start_net(struct txworker_s *txworker) {
  const char okphrase[] = "ok";
  int retcode;
  char readback[2048];

  txworker->state = 'c'; // connecting
  txworker->sockfd = tx_tcp_connect_next(txworker->txconf_parent);
  unsigned int reconnect_fuse = kthreadmax;
  while ((txworker->sockfd == -1) && (reconnect_fuse)) {

    pthread_mutex_lock(&(txworker->txconf_parent->mutex));
    if (txworker->txconf_parent->done) {
      pthread_mutex_unlock(&(txworker->txconf_parent->mutex));
      txworker->state = 'f';
      whisper(2, "txw:%02d ingest done reconnect not required anymore; giving "
                 "up thread\n",
              txworker->id);
      pthread_exit(0);
    }
    pthread_mutex_unlock(&(txworker->txconf_parent->mutex));

    pthread_mutex_lock(&(txworker->mutex));
    usleep((300 * 1000) << (kthreadmax - reconnect_fuse));
    whisper(5, "txw:%02ireconnecting\n", txworker->id);
    txworker->sockfd = tx_tcp_connect_next(txworker->txconf_parent);
    // detect a dead connection and move on to the next port in the target map
    // scary place to stall holding a lock, looking for a connect()
    if (txworker->sockfd > 0) {
      // checkperror ("clearing nuisance error after recovery\n");
      whisper(5, "txw:%02i reconnect success fd:%i\n", txworker->id,
              txworker->sockfd);
      errno = 0;
      reconnect_fuse = kthreadmax;
    }
    reconnect_fuse--;
  }
  if (reconnect_fuse == 0) {
    // die, we were unable to work thorugh the list and get a grip
    txworker->state = 'f';
    whisper(2, "txw:%02d reconnect fuse popped; giving up thread\n",
            txworker->id);
    pthread_exit(0);
  }
  /*
  starting sockets in parallel  can flood the remote end trivially on high bw
  networks resulting in
  this syndrome and a partially connnected graph
  it looks like this on the remote side:
  sonewconn: pcb 0xfffff8014ba261a8: Listen queue overflow: 10 already in queue
  awaiting acceptance (2 occurrences)
  pid 4752 (viamillipede), uid 1001: exited on signal 6 (core dumped)
  and refused connections on the Local
  can the remote end talk?
  this is lame  but rudimentary q/a session will assert that the tcp stream is
  able to bear traffic
  */
  whisper(18, "txw:%i send checkphrase:%s\n", txworker->id, gcheckphrase);
  retcode = write(txworker->sockfd, gcheckphrase, 4);
  checkperror("tx: phrase write fail");
  whisper(18, "txw:%i expect ok \n", txworker->id);
  retcode = read(txworker->sockfd, &readback, 2);
  checkperror("tx: read fail");
  if (bcmp(okphrase, readback, 2) != 0) {
    whisper(5, "txw: %i expected ok failure readlen:%i", txworker->id, retcode);
  }
  assert(bcmp(okphrase, readback, 2) == 0);
  whisper(13, "txw:%i online fd:%i\n", txworker->id, txworker->sockfd);
  txworker->writeremainder = -88;
  txstatus(txworker->txconf_parent, 7);
  pthread_mutex_unlock(&(txworker->mutex));
  return retcode;
}
void txworker_sm(struct txworker_s *txworker) {
  int done = 0;
  int retcode = -1;
  char local_state = 0;
  int sleep_thief = 0;
  pthread_mutex_init(&(txworker->mutex), NULL);
  retcode = tx_start_net(txworker);
  assert(retcode > 0);

  whisper(11, "txw:%i idling fd:%i\n", txworker->id, txworker->sockfd);
  pthread_mutex_lock(&(txworker->mutex));
  txworker->state = 'i'; // idle
  pthread_mutex_unlock(&(txworker->mutex));
  txworker->pkt.size = 0;
  txworker->pkt.leg_id = 0;
  txworker->pkt.preamble = preamble_cannon_ul;
  checkperror("worker buffer allocator");
  while (!done) {
    pthread_mutex_lock(&(txworker->txconf_parent->mutex));
    assert((txworker->id >= 0) && (txworker->id < kthreadmax));
    if ((txworker->txconf_parent->done)) {
      /*p  *((*((struct txworker_s *)txworker)).txconf_parent)
       y cant lld inspect:

      */
      whisper(4, "txw:%02i nothing left to do\n", txworker->id);
      done = 1;
    }
    local_state = txworker->state;
    pthread_mutex_unlock(&(txworker->txconf_parent->mutex));
    switch (local_state) {
    /* valid states:
    E: uninitialized
    f: faulted; unble to connect
    a: preamble; we think we are talking to  a villipede server
    c: connecting; idle when connected; die if we are done or can't connect
    d: dispatched buffer is loaded; now send it
    Pp: pushing
    i: idle but connected
    x: disconnected, still bearing a buffer. attempt reconnection
    n: not yet connected,  new no buffer ??? connect => idle

    */
    case 'i':
      break; // idle
    restartcase:
    case 'd':
      DTRACE_PROBE(viamillipede, leg__tx)
      if (txpush(txworker) == 0) {
        // retry  this
        whisper(3, "txw:%02i socket failed, scheduling retry leg:%lu\n",
                txworker->id, txworker->pkt.leg_id);
      }
      sleep_thief = 0;
      break;
    case 'x':
      // reinitialize socket and retry a dead worker
      whisper(6, "txw:02%i starting recovery, preserved leg %lu \n",
              txworker->id, txworker->pkt.leg_id);
      errno = 0;
      tx_start_net(txworker);
      goto restartcase;
      break;
    default:
      assert(-1 && "bad zoot");
    }
    sleep_thief++; // this Looks crazy ; but it's good for 30%
    // XXX back off tuning tbd
    // this works by allowing a context switch and permitting a dispatch ready
    // thread to push
    usleep(sleep_thief);
  } // while !done
  whisper(8, "txw:%02i worker done", txworker->id);
} //  txworker

void txlaunchworkers(struct txconf_s *txconf) {
  int worker_cursor = 0;
  int ret;
  checkperror("nuisance before launch");
  while (worker_cursor < txconf->worker_count) {
    txconf->workers[worker_cursor].state = '0'; // unitialized
    txconf->workers[worker_cursor].txconf_parent =
        txconf;                                    // allow inpection/inception
    txconf->workers[worker_cursor].pkt.leg_id = 0; //
    txconf->workers[worker_cursor].pkt.size = -66; //
    txconf->workers[worker_cursor].sockfd = -66;   //
    whisper(16, "txw:%i launching ", worker_cursor);
    txconf->workers[worker_cursor].state = 'L';
    txconf->workers[worker_cursor].id = worker_cursor;
    // allocate before thread launch
    txconf->workers[worker_cursor].buffer = calloc(1, (size_t)kfootsize);
    assert(txconf->workers[worker_cursor].buffer != NULL &&
           "insufficient memory up front");
    // digression: pthreads murders all possible kittens stored in argument
    // types
    checkperror("nuisance pthread error launch");
    ret =
        pthread_create(&(txconf->workers[worker_cursor].thread), NULL,
                       (void *)&txworker_sm, &(txconf->workers[worker_cursor]));
    checkperror("pthread launch");
    assert(ret == 0 && "pthread launch");
    worker_cursor++;
    usleep(10 * 1000);
    // 10ms standoff  to increase the likelyhood that PCBs are available on the
    // rx side
  }
  ret = pthread_create(&(txconf->ingest_thread), NULL,
                       (void *)/*puppied killed */ &txingest, txconf);
  checkperror("ingest thread launch");
  whisper(15, "tx workers launched ");
  txstatus(txconf, 5);
}

void txstatus(struct txconf_s *txconf, int log_level) {
  whisper(log_level, "\nstate:leg-remainder(k)");
  for (int i = 0; i < txconf->worker_count; i++) {
    if (i % 8 == 0) {
      whisper(log_level, "\n");
    }
    whisper(log_level, "%c:%lu(%i)\t", txconf->workers[i].state,
            txconf->workers[i].pkt.leg_id,
            (txconf->workers[i].writeremainder) >> 10 // kbytes are sufficient
            );
  }
  whisper(log_level, "\n");
}
int tx_poll(struct txconf_s *txconf) {
  // wait until all legs are pushed; called after ingest is complete
  // if there are launche/dispatched /pushing workers; hang here
  int done = 0;
  int busy_cycles = 0;
  char instate = 'E'; // error uninitialized
  while (!done) {
    usleep(1000); // e^n backoff?
    // if ((busy_cycles % 100) == 0)
    //  txstatus(txconf, 4);
    busy_cycles++;
    int busy_workers = 0;
    for (int i = 0; i < txconf->worker_count; i++) {
      // XXXXXpthread_mutex_lock (  &(txconf->workers[i].mutex) );
      instate = txconf->workers[i].state;
      // pthread_mutex_unlock (  &(txconf->workers[i].mutex) );
      if ((instate != 'i') && (instate != 'f')) {
        busy_workers++;
        break; // get out of here if there are busy workers
      }
    }
    done = (busy_workers == 0);
  }
  whisper(18, "\ntx: all workers idled after %i spins\n", busy_cycles);
  if (done && txconf->input_eof) {
    return 1;
  }

  return 0;
}
struct txconf_s *gtxconf;
void wat() {
  struct txconf_s *txconf = gtxconf;
  whisper(1, "\n%lu(mbytes) in ", txconf->stream_total_bytes >> 20);
  u_long usecbusy = stopwatch_stop(&(txconf->ticker), 2);
  whisper(1, "\n");
  txstatus(txconf, 1);
  // bytes per usec - thats interesting  bytes to mb
  whisper(1, "\n %8.4f MBps\n",
          (txconf->stream_total_bytes / (1.0 * usecbusy)));
}

void partingshot() {
  struct txconf_s *txconf = gtxconf;
  wat();
  whisper(2, "exiting after signal");
  checkperror(" signal caught");
  exit(-6);
}
void init_workers(struct txconf_s *txconf) {
  int wcursor = kthreadmax;
  while (wcursor--) {
    txconf->workers[wcursor].id = wcursor;
    txconf->workers[wcursor].txconf_parent = txconf;
    txconf->workers[wcursor].state = 'E';
    pthread_mutex_init(&(txconf->workers[wcursor].mutex), NULL);
    txconf->workers[wcursor].sockfd = 0;
    txconf->workers[wcursor].pkt.size = 0;
    txconf->workers[wcursor].pkt.leg_id = 0;
    txconf->workers[wcursor].writeremainder = 0;
  }
}

void tx(struct txconf_s *txconf) {
  int retcode;
  gtxconf = txconf;
  checkperror(" nuisance  starting tx");
  // start control channel
  stopwatch_start(&(txconf->ticker));
  signal(SIGINFO, &wat); // XXX this gets weird in fdx mode
  signal(SIGINT, &partingshot);
  signal(SIGHUP, &partingshot);
  checkperror("nuisance setting signal");
  pthread_mutex_init(&(txconf->mutex), NULL);
  pthread_mutex_lock(&(txconf->mutex));
  checkperror("nuicance  locking txconf");
  txconf->done = 0;
  txconf->input_eof = 0;
  pthread_mutex_unlock(&(txconf->mutex));
  init_workers(txconf);
  checkperror("nuicance tx initializing workers");
  txlaunchworkers(txconf);
}

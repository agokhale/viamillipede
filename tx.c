#include "worker.h"
extern int gchecksums;
extern int gcharmode;
extern int gleg_limit;
extern unsigned long gprbs_seed;
extern char *gcheckphrase;
extern u_long gdelay_us;

void txshutdown(struct txconf_s *txconf, int worker, u_long leg);
char tx_state(struct txworker_s *txworker) {
  // wrap the state with a locking primitive
  pthread_mutex_lock(&txworker->mutex);
  char ret_tmp = txworker->state;
  pthread_mutex_unlock(&txworker->mutex);
  return ret_tmp;
}

char tx_state_set(struct txworker_s *txworker, char instate) {
  // wrap the state with a locking primitive
  pthread_mutex_lock(&txworker->mutex);
  char ret_tmp = txworker->state = instate;
  pthread_mutex_unlock(&txworker->mutex);
  return ret_tmp;
}

#define ksndbuflimit 0x10000 // 64k
int dispatch_idle_worker(struct txconf_s *txconf) {
  int retcode = -1;
  txstatus(txconf, 5);
  int sleep_thief = 0;
  while (retcode < 0) {
    pthread_mutex_lock(&(txconf->mutex));
    for (int worker_cursor = 0;
         (worker_cursor < txconf->worker_count) && (retcode < 0);
         worker_cursor++) {
      if (tx_state(&txconf->workers[worker_cursor]) == 'i') {
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
      // investigate backoffs that are smarter. probably not helpful
      // sleep_thief <<= 1;
      // sleeping here indicates we do not have enough workers
      //   or enough throughput on the network
      usleep(sleep_thief);
      txconf->waits++;
    }
  } // we have a winner, return it
  assert(retcode < kthreadmax);
  return (retcode);
}

void start_worker(struct txworker_s *txworker) {
  assert(txworker->id < kthreadmax);
  tx_state_set(txworker, 'd');
  txstatus(txworker->txconf_parent, 6);
}

// dump <source>/stdin in chunks
// this is single thread
void txingest(struct txconf_s *txconf) {
  int readsize;
  int foot_cursor = 0;
  unsigned long saved_checksum = 0xff;
  u_long ingest_leg_counter = 0;
  txconf->stream_total_bytes = 0;
  whisper(16, "tx:ingest started on fd:%d", txconf->input_fd);
  setvbuf( stdin, NULL, _IONBF, 0); 
  checkperror("nuisance ingest err");
  pthread_mutex_lock(&(txconf->mutex));
  while (!txconf->done) {
    pthread_mutex_unlock(&(txconf->mutex));
    int worker = dispatch_idle_worker(txconf);
    if (gprbs_seed > 0) {
      // we are generating prbs, don't do any work
      readsize = kfootsize;
    } else {
      readsize = bufferfill(txconf->input_fd,
                            (u_char *)(txconf->workers[worker].buffer),
                            kfootsize, gcharmode);
      checkperror("ingest read");
      whisper(19, "\ntxw:%02i read leg %lx : fd:%i reqsiz:%x rsiz:%x\n", worker,
              ingest_leg_counter, txconf->input_fd, kfootsize, readsize);
    }
    if ((gleg_limit > 0) && (gleg_limit == ingest_leg_counter)) {
      whisper(5, "txingest: leg limit reached  sending shutdown");
      txshutdown(txconf, worker, ingest_leg_counter);
    } else if (readsize > 0) {
      // find first idle worker , lock it and dispatch as separate calls --
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
      whisper(5, "txingest: stdin exhausted. sending shutdown");
      txshutdown(txconf, worker, ingest_leg_counter);
    }
    ingest_leg_counter++;
    pthread_mutex_lock(&(txconf->mutex));
  }
  pthread_mutex_unlock(&(txconf->mutex));
  whisper(4, "tx:ingest complete for %lx(bytes) in \n\n",
          txconf->stream_total_bytes);
  txstatus(txconf, 5);
  tx_rate_report();
}

void txshutdown(struct txconf_s *txconf, int worker, u_long leg) {
  pthread_mutex_lock(&(txconf->mutex));
  txconf->done = 1;
  txconf->input_eof = 1;
  pthread_mutex_unlock(&(txconf->mutex));
  txconf->workers[worker].pkt.size = 0;
  txconf->workers[worker].pkt.leg_id = leg;
  txconf->workers[worker].pkt.opcode = end_of_millipede;
  start_worker(&(txconf->workers[worker]));
}
int txpush(struct txworker_s *txworker) {
  /** push this buffer out the socket
  return 1 on success, 0 if retry is needed
  */
  int retcode = 1;
  int writelen = -1;
  int cursor = 0;
  tx_state_set(txworker, 'a'); // preAmble
  struct iovec writevec[2];
  writevec[0].iov_base=&(txworker->pkt);
  writevec[0].iov_len=sizeof(struct millipacket_s);
  txworker->writeremainder = txworker->pkt.size;
  assert(txworker->writeremainder <= kfootsize);
  if (gprbs_seed > 0) {
    prbs_gen((unsigned long *)txworker->buffer,
             gprbs_seed + txworker->pkt.leg_id, kfootsize);
  }
  writevec[1].iov_base=txworker->buffer;
  writevec[1].iov_len=txworker->pkt.size;
  if (gdelay_us) {
    tx_state_set(txworker, 'W'); // 'W'wait
    usleep(gdelay_us);
  }
  tx_state_set(txworker, 'P'); // 'P'ush
  int writeret=0;
  writeret=writev(txworker->sockfd, &writevec[0], 2);
  whisper(13, "txw:%02i psh leg:%lx.(+%x -%x)  ", txworker->id,
          txworker->pkt.leg_id, writeret, txworker->writeremainder);
  checkperror("aiowrite");
  assert ( txworker->writeremainder + sizeof (struct millipacket_s ) == writeret);
  txworker->writeremainder=0;

  checkperror("writesocket");
  pthread_mutex_lock(&(txworker->txconf_parent->mutex));
  if (retcode == 1) {
    // if we are successful, drop the lease on this leg and become idle
    assert(txworker->writeremainder == 0 && "unpushed data remains");
    txworker->pkt.size = 0;
    tx_state_set(txworker, 'i'); // idle ok
  } else {
    DTRACE_PROBE(viamillipede, leg__drop);
    tx_state_set(txworker,
                 'x'); // dead  do not transmit more; save state and do it again
    whisper(4, "rxw:%02i is dead\n", txworker->id);
  }
  pthread_mutex_unlock(&(txworker->txconf_parent->mutex));
  return retcode;
}
int tx_tcp_connect_next(struct txconf_s *txconf) {
  /** pick a port/host from the list, cycling through  them
  this will bias the two lowest target port entries are favored.
  If they are busy and more workers are available;use more target ports
  monitoring txstatus() output will reveal the distribution
  returns: a tcp connection attempt.
  */
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
  tx_state_set(txworker, 'c'); // connecting
  txworker->sockfd = tx_tcp_connect_next(txworker->txconf_parent);
  unsigned int reconnect_fuse = kreconnectlimit;
  while ((txworker->sockfd == -1) && (reconnect_fuse)) {

    pthread_mutex_lock(&(txworker->txconf_parent->mutex));
    if (txworker->txconf_parent->done) {
      pthread_mutex_unlock(&(txworker->txconf_parent->mutex));
      tx_state_set(txworker, 'f');
      whisper(2,
              "txw:%02d ingest done reconnect not required anymore; giving "
              "up thread\n",
              txworker->id);
      pthread_exit(0);
    }
    pthread_mutex_unlock(&(txworker->txconf_parent->mutex));

    pthread_mutex_lock(&(txworker->mutex));
    usleep((300 * 1000) << (kreconnectlimit - reconnect_fuse));
    whisper(5, "txw:%02ireconnecting\n", txworker->id);
    // scary place to stall holding a lock, looking for a connect()
    txworker->sockfd = tx_tcp_connect_next(txworker->txconf_parent);
    // detect a dead connection and move on to the next port in the target map
    if (txworker->sockfd > 0) {
      // clearing nuisance error after recovery
      whisper(5, "txw:%02i reconnect success fd:%i\n", txworker->id,
              txworker->sockfd);
      errno = 0;
      reconnect_fuse = kreconnectlimit;
    }
    reconnect_fuse--;
  }
  if (reconnect_fuse == 0) {
    // die, we were unable to work through the list and get a grip
    tx_state_set(txworker, 'f');
    whisper(2, "txw:%02d reconnect fuse popped; giving up thread\n",
            txworker->id);
    pthread_exit(0);
  }
  /*
  starting sockets in parallel  can flood the remote end trivially on high bw
  networks resulting in
  this syndrome and a partially connected graph
  it looks like this on the remote side:
  sonewconn: pcb 0xfffff8014ba261a8: Listen queue overflow: 10 already in queue
  awaiting acceptance (2 occurrences)
  pid 4752 (viamillipede), uid 1001: exited on signal 6 (core dumped)
  and refused connections on the Local
  can the remote end talk?
  this is lame q/a session will assert that the tcp stream is
  able to bear traffic, and that we are talking to viamillipede on the other
  side
  */
  whisper(18, "txw:%i send checkphrase:%s\n", txworker->id, gcheckphrase);
  retcode = write(txworker->sockfd, gcheckphrase, 4);
  checkperror("tx: phrase write fail");
  assert(retcode == 4 && "tx: failed to write checkphrase to network");
  whisper(18, "txw:%i expect ok \n", txworker->id);
  retcode = read(txworker->sockfd, &readback, 2);
  checkperror("tx: read fail");
  if (bcmp(okphrase, readback, 2) != 0) {
    whisper(5, "txw: %i expected ok failure readlen:%i", txworker->id, retcode);
  }
  assert(bcmp(okphrase, readback, 2) == 0 &&
         "ok phrase mismatch, is the other side a compatible viamillipede?");
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
  retcode = tx_start_net(txworker);
  assert(retcode > 0);
  whisper(11, "txw:%i idling fd:%i\n", txworker->id, txworker->sockfd);
  tx_state_set(txworker, 'i');
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
      whisper(6, "txw:%02i nothing left to do\n", txworker->id);
      done = 1;
    }
    local_state = tx_state(txworker);
    pthread_mutex_unlock(&(txworker->txconf_parent->mutex));
    switch (local_state) {
    /* valid states:
    E: uninitialized
    f: faulted; unable to connect
    a: preamble; we think we are talking to  a viamillipede server
    c: connecting; idle when connected; die if we are done or can't connect
    d: dispatched buffer is loaded; now send it
    Pp: pushing
    i: idle but connected
    W: waiting(delay_us)
    x: disconnected, still bearing a buffer. attempt reconnection
    n: not yet connected,  new no buffer ??? connect => idle

    */
    case 'i':
      break; // idle
    restartcase:
    case 'd':
      DTRACE_PROBE(viamillipede, leg__tx)
      if (txpush(txworker) == 0) {
        whisper(2, "txw:%02i push failed, marking for retry on leg:%lx\n",
                txworker->id, txworker->pkt.leg_id);
      }
      sleep_thief = 0;
      break;
    case 'x':
      // reinitialize socket and retry a dead worker
      whisper(4, "txw:02%i starting recovery, preserved leg %lx \n",
              txworker->id, txworker->pkt.leg_id);
      errno = 0;
      tx_start_net(txworker);
      goto restartcase;
      break;
    default:
      whisper(1, "bad zoot");
      exit(EDOOFUS);
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
    tx_state_set(&txconf->workers[worker_cursor], '-'); // uninitialized
    txconf->workers[worker_cursor].txconf_parent =
        txconf;                                    // allow inspection/inception
    txconf->workers[worker_cursor].pkt.leg_id = 0; //
    txconf->workers[worker_cursor].pkt.size = -66; //
    txconf->workers[worker_cursor].sockfd = -66;   //
    whisper(16, "txw:%i launching ", worker_cursor);
    tx_state_set(&txconf->workers[worker_cursor], 'L'); // launched
    txconf->workers[worker_cursor].id = worker_cursor;
    // allocate before thread launch
    txconf->workers[worker_cursor].buffer = calloc(1, (size_t)kfootsize);
    assert(txconf->workers[worker_cursor].buffer != NULL &&
           "insufficient memory up front");
    checkperror("nuisance pthread error launch");
    ret =
        pthread_create(&(txconf->workers[worker_cursor].thread), NULL,
                       (void *)&txworker_sm, &(txconf->workers[worker_cursor]));
    checkperror("pthread launch");
    assert(ret == 0 && "pthread launch error");
    worker_cursor++;
    usleep(10 * 1000);
    // 10ms standoff  to increase the likelihood that PCBs are available on the
    // rx side to answer requests
  }
  /*ret =
      pthread_create(&(txconf->ingest_thread), NULL, (void *)&txingest, txconf);
  checkperror("ingest thread launch");
  assert(ret == 0 && "tx: pthread_create error");
*/
  whisper(15, "all tx workers launched ");
  txstatus(txconf, 5);
}

void txstatus(struct txconf_s *txconf, int log_level) {
  //whisper(log_level, "\nstate:leg-remainder(k)");
  for (int i = 0; i < txconf->worker_count; i++) {
    int sfd =  txconf->workers[i].sockfd;
    //tcp_dump_sockfdparams( sfd );
    checkperror("tcp_dump_sockfdparams");
    if ( gverbose > log_level ) { 
      //tcp_dumpinfo( sfd);
    }
    whisper(log_level, "{t:%c:%lx}\t", tx_state(&txconf->workers[i]),
            txconf->workers[i].pkt.leg_id
    );
    if (i % 8 == 7) {
      whisper(log_level, "\n");
    }

  }
  whisper(log_level, "\n");
}
int tx_poll(struct txconf_s *txconf) {
  // wait until all legs are pushed; called after ingest is complete
  // if there are launched/dispatched/pushing workers; hang here
  int done = 0;
  int busy_cycles = 0;
  char instate = 'E'; // error uninitialized
  while (!done) {
    usleep(1000); // e^n backoff?
    busy_cycles++;
    int busy_workers = 0;
    for (int i = 0; i < txconf->worker_count; i++) {
      instate = tx_state(&txconf->workers[i]);
      if ((instate != 'i') && (instate != 'f')) {
        busy_workers++;
        break; // get out of here if there are busy workers
      }
    }
    done = (busy_workers == 0);
  }
  if (done && txconf->input_eof) {
    return 1;
  }
  return 0;
}
struct txconf_s *gtxconf;
void tx_rate_report() {
  struct txconf_s *txconf = gtxconf;
  u_long usecbusy = stopwatch_stop(&(txconf->ticker));
  whisper(1, "\n%lu(MiBytes)/%lu(s)=%8.4f(MiBps)\n",
          txconf->stream_total_bytes >> 20, usecbusy/1000000,
          (txconf->stream_total_bytes / (1.0 * usecbusy)));
  txstatus(txconf, 1);
}
void partingshot() {
  tx_rate_report();
  whisper(2, "exiting after signal");
  checkperror("signal caught");
  exit(EINTR);
}
void init_workers(struct txconf_s *txconf) {
  int wcursor = kthreadmax;
  while (wcursor--) {
    txconf->workers[wcursor].id = wcursor;
    txconf->workers[wcursor].txconf_parent = txconf;
    pthread_mutex_init(&(txconf->workers[wcursor].mutex), NULL);
    tx_state_set(&txconf->workers[wcursor], 'E'); // error if you ever see this
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
  checkperror("nuisance setting signal");
  pthread_mutex_init(&(txconf->mutex), NULL);
  pthread_mutex_lock(&(txconf->mutex));
  checkperror("nuisance  locking txconf");
  txconf->done = 0;
  txconf->input_eof = 0;
  pthread_mutex_unlock(&(txconf->mutex));
  init_workers(txconf);
  checkperror("nuisance tx initializing workers");
  txlaunchworkers(txconf);
}

#include "cfg.h"
#include "worker.h"

void verbose_plus() { gverbose++; }
void verbose_minus() { gverbose--; }

void usage() {
  printf("viamillipede scatter gather multiplexed tcp for pipe transport "
         "between hosts usage: \n");
  printf("loopback+stress:  viamillipede tx l27.0.0.1 12323 "
         "threads 3 verbose 3 rx 12323 "
         " leglimit 50 prbs 0x5aa5  delayus 50 \n");
  printf("receiver:  viamillipede rx 12323 \n");
  printf("prbs:  viamillipede prbs 0xfeed \n");
#ifdef CHAOS
  printf("add repeatable  failures:  viamillipede rx 12323  chaos 180002 \n");
#endif
}

#define MODE_TX 2
#define MODE_RX 4
#define MODE_PRBS 8
int mode = 0;
int gverbose = 0;
#ifdef CHAOS
unsigned long gchaos = 0;
unsigned long gchaoscounter = 0;
#endif
unsigned long gprbs_seed = 0;
unsigned long gdelay_us = 0; // per buffer ingest delay
int gchecksums = 0;
int gcharmode = 0;
int ginitiator_oneshot = 0;
int gleg_limit = 0;
char *gcheckphrase;
struct txconf_s txconf;
struct rxconf_s rxconf;
struct ioconf_s ioconf;

void siginfohandle() {
  printf("mode %x\b",mode); 
  if (mode & MODE_TX) tx_rate_report();
  if (mode & MODE_RX) rxinfo(&rxconf );
}
int initiate_relay() {
  // thunk in globals, called from rx's relay  startup packet
  if (ginitiator_oneshot > 0)
    return 0;
  ginitiator_oneshot++;
  whisper(6, "initiator oneshot starting ");
  int retcode = initiate(&txconf, &rxconf, &ioconf);
  assert(retcode >= 1);
  // we have delayed ingesting until there is a real fd
  int ret =
      pthread_create(&(txconf.ingest_thread), NULL, (void *)&txingest, &txconf);
  checkperror("initiator delayed ingest thread launch");
  assert(ret == 0 && "tx: pthread_create error");
  return retcode;
}

int main(int argc, char **argv) {

  int arg_cursor = 0;
  int users_input_port;
  signal(SIGUSR1, &verbose_plus);
  signal(SIGUSR2, &verbose_minus);
#ifdef SIGINFO
  signal(SIGINFO, &siginfohandle);
#endif

  gcheckphrase = "yoes";
  (argc > 1) ?: usage();
  txconf.worker_count = 16;
  txconf.waits = 0;
  txconf.target_port_count = 0;
  txconf.target_port_cursor = 0;
  ioconf.terminate_port = 0;
  ioconf.initiate_port = 0;
  errno = 0; // why, really why?!!!
  checkperror(" main nuisance: no possible reason, bad zoot! ");
  while (arg_cursor < argc) {
    whisper(19, "  arg: %d, %s\n", arg_cursor, argv[arg_cursor]);
    checkperror("main arg proc");
    assert(errno == 0);
    if (strcmp(argv[arg_cursor], "rx") == 0) {
      /* rx <portnumber> */
      ++arg_cursor;
      assert(arg_cursor < argc &&
             " rx requires a  <portnumber> from 0-SHRT_MAX");
      users_input_port = atoi(argv[arg_cursor]);
      assert(0 < users_input_port && users_input_port < USHRT_MAX &&
             "port number should be 0-USHRT_MAX");
      rxconf.port = (short)users_input_port;
      whisper(3, " being a server at port %i \n\n ", rxconf.port);
      mode |= MODE_RX;
      checkperror(" main nuisance: rx init error ");
    }
    if (strcmp(argv[arg_cursor], "tx") == 0) {
      /* tx may be defined multiple times to achieve a balance across multiple L3 routes
       ei:  tx localhost 12323 tx tardis.co.ac.uk 12323 tx otherhost 123232
       but they all must the same machine behind the ip address set */
      assert(++arg_cursor < argc && "tx needs <host> and <port> arguments");
      assert(strlen(argv[arg_cursor]) > 0 && "hostname seems fishy");
      checkperror(" main nuisance : hostname err");
      txconf.target_ports[txconf.target_port_count].name = argv[arg_cursor];
      arg_cursor++;
      assert(arg_cursor < argc &&
             " tx  requires a  <portnumber> from 0-SHRT_MAX");
      users_input_port = atoi(argv[arg_cursor]);
      assert(0 < users_input_port && users_input_port < USHRT_MAX &&
             "port number should be 0-USHRT_MAX");
      txconf.target_ports[txconf.target_port_count].port =
          (short)users_input_port;
      checkperror(" main nuisance:  port err2 ");
      whisper(2, "tx host: %s port:%i \n",
              txconf.target_ports[txconf.target_port_count].name,
              txconf.target_ports[txconf.target_port_count].port);
      txconf.target_port_count++;
      mode |= MODE_TX;
    }
    if (strcmp(argv[arg_cursor], "threads") == 0) {
      ++arg_cursor;
      assert(arg_cursor < argc && "threads needs <number> arguments");
      txconf.worker_count = atoi(argv[arg_cursor]);
      checkperror(" main nuisance: thread count parse ");
      assert(txconf.worker_count <= 16 &&
             "it's unlikely that a large threadcount is beneficial");
    }
    if (strcmp(argv[arg_cursor], "verbose") == 0) {
      ++arg_cursor;
      assert(arg_cursor < argc &&
             "verbose  needs <level ( 0 - 19) > argument");
      gverbose = atoi(argv[arg_cursor]);
      whisper(11, "verbose set to %i\n", gverbose);
    }
    if (strcmp(argv[arg_cursor], "checksums") == 0) {
      assert(arg_cursor < argc && "checksums is a flag ");
      gchecksums = 1;
      checkperror(" main checksum -3 ");
      whisper(11, "checksum set to %d", gchecksums);
    }
    if (strcmp(argv[arg_cursor], "delayus") == 0) {
      arg_cursor++;
      assert(arg_cursor < argc && "delayus needs a uint ");
      gdelay_us = atoi(argv[arg_cursor]);
      checkperror(" main delay -3 ");
      whisper(11, "gdelay_us set to %lu", gdelay_us);
    }
#ifdef CHAOS
    if (strcmp(argv[arg_cursor], "chaos") == 0) {
      assert(++arg_cursor < argc && "chaos  needs  ( 0 - max-ulong) ");
      gchaos = atoi(argv[arg_cursor]);
      gchaoscounter = gchaos;
      checkperror(" main chaos -3 ");
      whisper(11, "chaos set to %lu", gchaos);
    }
#endif
    if (strcmp(argv[arg_cursor], "checkphrase") == 0) {
      assert(++arg_cursor < argc && "checkphrase  needs  char[] ");
      gcheckphrase = argv[arg_cursor];
      whisper(11, "checkphrase set to %s", gcheckphrase);
    }
    // terminate 5656
    // command setup listen for connection at port
    if (strcmp(argv[arg_cursor], "terminate") == 0) {
      assert(++arg_cursor < argc && "terminate  needs  port ");
      ioconf.terminate_port = atoi(argv[arg_cursor]);
      whisper(11, "termport set to %d", ioconf.terminate_port);
    }
    // initiate hathor 5656
    // command setup start connection at port
    if (strcmp(argv[arg_cursor], "initiate") == 0) {
      arg_cursor++;
      assert(strlen(argv[arg_cursor]) > 0 && "hostname seems fishy");
      ioconf.initiate_host = argv[arg_cursor];
      arg_cursor++;
      ioconf.initiate_port = atoi(argv[arg_cursor]);
      whisper(11, "initiator port set to %d", ioconf.initiate_port);
    }
    if (strcmp(argv[arg_cursor], "charmode") == 0) {
      gcharmode = 1;
      whisper(11, "charmode active");
    }
    if (strcmp(argv[arg_cursor], "prbs") == 0) {
      mode |= MODE_PRBS;
      arg_cursor++;
      assert ( arg_cursor < argc && 
             "prbs requires an argument eg: 0xa5a55a5a");
      gprbs_seed = strtoul(argv[arg_cursor], NULL, 0);
      whisper(11, "prbs mode seed %lu", gprbs_seed);
    }
    if (strcmp(argv[arg_cursor], "leglimit") == 0) {
      mode |= MODE_PRBS;
      arg_cursor++;
      assert ( arg_cursor < argc && 
             "leglimit requires an argument integer number of legs");
      gleg_limit = strtoul(argv[arg_cursor], NULL, 0);
      whisper(11, "legs limited to %lu", gprbs_seed);
    }
    arg_cursor++;
  }
  checkperror("main nuisance: unspecified");
  assert(!(ioconf.terminate_port > 0 && ioconf.initiate_port > 0) &&
         "can't initiate and terminate in parallel");
  DTRACE_PROBE(viamillipede, init);
  if (mode & MODE_RX)
    rx(&rxconf); // rx must precede
  if (mode & MODE_TX)
    tx(&txconf);
  assert(
      terminate(&txconf, &rxconf, &ioconf) >=
      0); // STDIN == 0 and is valid this should block , as workers are launched
  // assert(initiate(&txconf, &rxconf, &ioconf) >= 1); // can't happen now must
  // delay until terminate happened On the remote
  // ingest must be delayed after initiate
  if ((mode & MODE_TX) && (ioconf.initiate_port == 0)) {
    initiate_relay();
  }
  int rxpoll_done = 0;
  int txpoll_done = 0;
  int done_conditions = 2;
  // if ( ioconf.initiate_port > 0 || ioconf.terminate_port > 0) done_conditions
  // =1;
  while (txpoll_done + rxpoll_done < done_conditions) {
    if (mode & MODE_TX)
      txpoll_done = tx_poll(&txconf);
    else
      txpoll_done = 1;
    if (mode & MODE_RX)
      rxpoll_done = rx_poll(&rxconf);
    else
      rxpoll_done = 1;
    usleep(333);
  }
  whisper(15, "finished normally");
  exit(0);
}

#include "cfg.h"
#include "worker.h"

void usage() {
  printf("viamillipede scatter gather multiplexed tcp for pipe transport "
         "between hosts usage: \n");
  printf("transmitter:  vimillipede tx 192.168.0.2 12323  tx 192.168.0.3 12323 "
         "threads 3 verbose 3\n");
  printf("receiver:  vimillipede rx 12323 \n");
  printf("add repeatable  failures:  vimillipede rx 12323  chaos 180002 \n");
}

int gverbose = 0;
unsigned long gchaos = 0;
unsigned long gchaoscounter = 0;
int gchecksums = 0;
int gcharmode = 0;
char *gcheckphrase;

int main(int argc, char **argv) {

  int arg_cursor = 0;
#define MODE_TX 2
#define MODE_RX 4
  int mode = 0;
  int users_input_port;
  struct txconf_s txconf;
  struct rxconf_s rxconf;
  struct ioconf_s ioconf;
  gcheckphrase = "yoes";
  (argc > 1) ?: usage();
  txconf.worker_count = 16;
  txconf.target_port_count = 0;
  txconf.target_port_cursor = 0;
  ioconf.terminate_port = 0;
  ioconf.initiate_port = 0;
  checkperror(" main nuiscance -0 ");
  while (arg_cursor < argc) {
    whisper(30, "  arg: %d, %s\n", arg_cursor, argv[arg_cursor]);
    checkperror("main arg proc");
    assert(errno == 0);
    if (strcmp(argv[arg_cursor], "rx") == 0) {
      /* rx <portnumber> */
      assert(++arg_cursor < argc &&
             " rx requires a  <portnumber> from 0-SHRT_MAX");
      users_input_port = atoi(argv[arg_cursor]);
      assert(0 < users_input_port && users_input_port < USHRT_MAX &&
             "port number should be 0-USHRT_MAX");
      rxconf.port = (short)users_input_port;
      whisper(3, " being a server at port %i \n\n ", rxconf.port);
      mode |= MODE_RX;
      checkperror(" main nuiscance -1 ");
    }
    if (strcmp(argv[arg_cursor], "tx") == 0) {
      //
      assert(++arg_cursor < argc && "tx needs <host> and <port> arguments");
      assert(strlen(argv[arg_cursor]) > 0 && "hostname seems fishy");
      checkperror(" main nuiscance  port err0 ");
      txconf.target_ports[txconf.target_port_count].name = argv[arg_cursor];
      checkperror(" main nuiscance  port err1 ");
      arg_cursor++;
      assert(arg_cursor < argc &&
             " tx  requires a  <portnumber> from 0-SHRT_MAX");
      users_input_port = atoi(argv[arg_cursor]);
      assert(0 < users_input_port && users_input_port < USHRT_MAX &&
             "port number should be 0-USHRT_MAX");
      txconf.target_ports[txconf.target_port_count].port =
          (short)users_input_port;
      checkperror(" main nuiscance  port err2 ");
      whisper(2, "tx host: %s port:%i \n",
              txconf.target_ports[txconf.target_port_count].name,
              txconf.target_ports[txconf.target_port_count].port);
      txconf.target_port_count++;
      checkperror(" main nuiscance  port err ");
      mode |= MODE_TX;
    }
    if (strcmp(argv[arg_cursor], "threads") == 0) {
      assert(++arg_cursor < argc && "threads needs <numeber> arguments");
      txconf.worker_count = atoi(argv[arg_cursor]);
      checkperror(" main nuiscance -3 ");
      assert(txconf.worker_count <= 16 &&
             "it's unlikely that a large threadcount is beneficial");
    }
    if (strcmp(argv[arg_cursor], "verbose") == 0) {
      /// XXXX NDEBUG will break
      assert(++arg_cursor < argc &&
             "verbose  needs <level ( 0 - 19) > argument");
      gverbose = atoi(argv[arg_cursor]);
      whisper(5, "verbose set to %i\n", gverbose);
    }
    if (strcmp(argv[arg_cursor], "checksums") == 0) {
      assert(arg_cursor < argc && "checksums is a flag ");
      gchecksums = 1;
      checkperror(" main checksum -3 ");
      whisper(11, "checksum set to %lu", gchaos);
    }
    if (strcmp(argv[arg_cursor], "chaos") == 0) {
      assert(++arg_cursor < argc && "chaos  needs  ( 0 - max-ulong) ");
      gchaos = atoi(argv[arg_cursor]);
      gchaoscounter = gchaos;
      checkperror(" main chaos -3 ");
      whisper(11, "chaos set to %lu", gchaos);
    }
    if (strcmp(argv[arg_cursor], "checkphrase") == 0) {
      assert(++arg_cursor < argc && "checkphrase  needs  char[] ");
      gcheckphrase = argv[arg_cursor];
      whisper(11, "checkphrase set to %s", gcheckphrase);
    }
    // terminate 5656
    // command setup listen for connecton at port
    if (strcmp(argv[arg_cursor], "terminate") == 0) {
      assert(++arg_cursor < argc && "terminate  needs  port ");
      ioconf.terminate_port = atoi(argv[arg_cursor]);
      whisper(11, "termport set to %d", ioconf.terminate_port);
    }
    // initiate hathor 5656
    // command setup start connecton at port
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
    arg_cursor++;
  }
  checkperror("main nuiscance");
  assert(!(ioconf.terminate_port > 0 && ioconf.initiate_port > 0) &&
         "can't initiate and termimate in parallel");
  DTRACE_PROBE(viamillipede, init);
  assert(terminate(&txconf, &rxconf, &ioconf) >= 0); // STDIN == 0 and is valid
  assert(initiate(&txconf, &rxconf, &ioconf) >= 1);
  if (mode & MODE_RX)
    rx(&rxconf); // rx must preceed
  if (mode & MODE_TX)
    tx(&txconf);
  int rxpoll_done = 0;
  int txpoll_done = 0;
  while (txpoll_done + rxpoll_done < 2) {
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
  exit(0);
}

#include "worker.h"

int terminate(struct txconf_s *txconf, struct rxconf_s *rxconf,
              struct ioconf_s *ioconf) {
  /** terminate accepts a tcp connection and fixes up the ioconf structure  for
   ingest the tcp socket returns the file descripter chosen
  */
  int retc = -6;
  if (ioconf->terminate_port > 0) {
    struct sockaddr_in sa;
    tcp_recieve_prep(&sa, &(ioconf->terminate_socket), ioconf->terminate_port);
    whisper(6, "term: accepting on %d", ioconf->terminate_port);
    txconf->input_fd =
        tcp_accept(&sa, ioconf->terminate_socket); // XX should we block?
    assert(txconf->input_fd > 2);
    stopwatch_start(
        &(txconf->ticker)); // reset the timer to exclude waiting time
    retc = rxconf->output_fd = txconf->input_fd;
    whisper(7, "txterm: accepted fd %d", txconf->input_fd);
  } else if (ioconf->initiate_port > 0) {
    // Dont clobber input_fd
    retc = 1;
  } else {
    // Revert to pipe io
    whisper(10, "setting input to STDIN");
    retc = txconf->input_fd = STDIN_FILENO;
  }
  return retc;
}
int initiate(struct txconf_s *txconf, struct rxconf_s *rxconf,
             struct ioconf_s *ioconf) {
  int retc = -6;
  if (ioconf->initiate_port > 0) {
    retc = txconf->input_fd = rxconf->output_fd =
        tcp_connect(ioconf->initiate_host, ioconf->initiate_port);
    whisper(6, "initiate: host:%s port:%d, fd:%d", ioconf->initiate_host,
            ioconf->initiate_port, retc);
  } else if (ioconf->terminate_port > 0) {
    // dont clobber output_fd;
    retc = 1;
  } else {
    whisper(10, "initiate: setting output to STDOUT");
    retc = rxconf->output_fd = STDOUT_FILENO;
  }

  return retc;
}

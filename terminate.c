#include "worker.h"

int terminate(struct txconf_s *txconf, struct rxconf_s *rxconf,
              struct ioconf_s *ioconf) {
  int retc = -6;
  if (ioconf->terminate_port) {
    struct sockaddr_in sa;
    tcp_recieve_prep(&sa, &(ioconf->terminate_socket), ioconf->terminate_port);
    whisper(8, "term: accepting on %d", ioconf->terminate_port);
    txconf->input_fd =
        tcp_accept(&sa, ioconf->terminate_socket); // XX should we block?
    assert(txconf->input_fd > 2);
    rxconf->output_fd = txconf->input_fd;
    whisper(7, "txterm: accepted fd %d", txconf->input_fd);
  } else {
    txconf->input_fd = STDIN_FILENO;
  }
  return retc;
}
int initiate(struct txconf_s *txconf, struct rxconf_s *rxconf,
             struct ioconf_s *ioconf) {
  int retc = -6;

  return retc;
}

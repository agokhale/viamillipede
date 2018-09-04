provider viamillipede {
  probe init();
  probe worker__connect(int ip, int port);
  probe worker__connected(int fd);
  probe leg__drop();
  probe leg__tx();
  probe leg__rx();
};

#!/bin/sh -x

echo asdf | ./viamillipede rx 4545 tx localhost 4545 verbose 5 threads 3
dd if=/dev/zero bs=1g count=3 | cat - > /dev/null
dd if=/dev/zero bs=1g count=3 | ./viamillipede  threads 2  verbose 3 rx 4545 tx localhost 4545  > /dev/null

#tunnel ssh over an fdx viamillipede; this is probably a bad idea as ssh issues tinygrams and the system shoe shines around buffer sync trouble
ver="3"
./viamillipede charmode threads 2 initiate localhost 22  tx localhost 4546 rx 4545  verbose $ver &
nitpid=$!
./viamillipede charmode threads 2 terminate 6622  tx localhost 4545 rx 4546  verbose $ver &
termpid=$!
sleep 3
ssh  -p 6622 localhost "dd if=/dev/zero bs=10m count=1" > /dev/null


#tunnel viamillipede over multiple ssh tunnels; this works handily and will happily boil your cpus doing parallel ssh
ssh -N  -L 6661:localhost:4545 localhost  & 
sshpis=$!
ssh -N  -L 6662:localhost:4545 localhost  & 
sshpis=$!
viamillipede verbose $ver  rx 4545 > /dev/null &
rxpid=!
sleep 1
dd if=/dev/zero bs=1g count=30 | viamillipede tx localhost 6661 tx localhost 6662 verbose $ver threads 2

#!/bin/sh -x

echo asdf:
echo asdf | ./viamillipede rx 4545 tx localhost 4545 verbose 15 threads 3

nau=`date +"%s`
./viamillipede prbs 1 tx localhost 3434 verbose 3 threads 2 rx 3434 &
prbpid=$!
sudo dtrace -qn'profile-333hz /execname == "viamillipede"/ { @[ustack()]=count();}' -o /tmp/viaustac$nau.ustacks &
dtpid=$!
sleep 4
kill $dtpid
kill -INFO  $prbpid
dtstackcollapse_flame.pl < /tmp/viaustac$nau.ustacks | flamegraph.pl > /net/delerium/zz/pub/viam.svg


echo tunnel ssh over an fdx viamillipede this is probably a bad idea as ssh issues tinygrams and the system shoe shines around buffer sync trouble
ver="3"
./viamillipede charmode threads 2 initiate localhost 22  tx localhost 4546 rx 4545  verbose $ver &
nitpid=$!
./viamillipede charmode threads 2 terminate 6622  tx localhost 4545 rx 4546  verbose $ver &
termpid=$!
sleep 3
ssh  -p 6622 localhost "dd if=/dev/zero bs=10m count=1" > /dev/null


echo tunnel viamillipede over multiple ssh tunnels this works handily and will happily boil your cpus doing parallel ssh expect 340mbps
ssh -N  -L 6661:localhost:4545 localhost  & 
sshpis=$!
ssh -N  -L 6662:localhost:4545 localhost  & 
sshpis=$!
./viamillipede verbose $ver  rx 4545 > /dev/null &
rxpid=!
sleep 1
echo zeros via twin parallel ssh port forward
dd if=/dev/zero bs=1g count=10 | ./viamillipede tx localhost 6661 tx localhost 6662 verbose $ver threads 2

echo zeros over viamillipede, hot path test
dd if=/dev/zero bs=1g count=30 | ./viamillipede threads 2 verbose 3 rx 4545 tx localhost 4545  > /dev/null

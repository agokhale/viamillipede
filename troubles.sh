#!/bin/sh 
set -x
#testcase for the pathological loopback/ very high rate lockup

foom() {
	kill $txpid
	kill $rxpid
}
trap foom INT

thread=4
./viamillipede verbose 5 rx 12323 prbs 0x5aa5 > /dev/null     & 
rxpid=$!
./viamillipede tx localhost 12323 threads $thread verbose 5  prbs 0x5aa5      &
txpid=$!


echo "gdb ./viamillipede $rxpid"
echo "gdb ./viamillipede $txpid"

wat() {
	netstat -an | grep 12323
}

wait $txpid

#!/bin/sh

verboarg="2"

fini() {
echo "foom, cleaning up"
rm -f  $allcollateralfiles
echo "escaped prisoners still running:"
pgrep vmpdbin

kill $prisoners
exit 0
}
trap fini  KILL INT TERM
#____________________________________________________________________________
#build it, stash the binary somewhere dangerous
make clean 
make  || exit  -1
dutbin=`mktemp "/tmp/vmpdbin.XXX"` || exit -11
allcollateralfiles=$dutbin
mv viamillipede $dutbin
#____________________________________________________________________________
t_est_loopback_trivial() {
echo loopback:
output=`echo asdf | $dutbin rx 4545 tx localhost 4545  threads 3`
case $output in
 "asdf") break ;;
 *) exit -4;;
esac

}
t_est_loopback_trivial
#________________________________________________________________________
t_est_prbs() {
 echo prbs:
 $dutbin prbs 0xd00f tx localhost 3434 verbose 4 threads 2 rx 3434 &
 prbpid=$!
 sleep 3 
 kill -INFO $prbpid
 sleep 0.2
kill  $prbpid
}
t_est_prbs
#________________________________________________________________________
dtflame()  {
nau=`date +"%s"`
$dutbin prbs 1 tx localhost 3434 verbose 3 threads 2 rx 3434 &
prbpid=$!
dtrace  -p`pgrep viamillipede` -qn'profile-333hz /execname == "viamillipede"/ { @[ustack()]=count();}' -o /tmp/viaustac$nau.ustacks &
dtpid=$!
sleep 4
kill $dtpid
sleep 0.3
dtstackcollapse_flame.pl < /tmp/viaustac$nau.ustacks | flamegraph.pl > /net/delerium/zz/pub/viam.svg
}
#________________________________________________________________________
t_bearer_for_ssh() {
	echo tunnel ssh over an fdx viamillipede this is probably a bad idea as ssh issues tinygrams and the system shoe shines around buffer sync trouble
	$dutbin charmode threads 2 terminate 16622  tx localhost 4545 rx 4546  verbose $verboarg &
	termpid=$!
	sleep 1
	$dutbin charmode threads 2 initiate localhost 22  tx localhost 4546 rx 4545  verbose $verboarg &
	nitpid=$!
	prisoners="$prisoners $nitpid $termpid"
	sleep 3
	ssh  -p 16622 localhost "dd if=/dev/zero bs=10m count=1" > /dev/null
	prisoners="$prisoners $nitpid $termpid"
}
t_bearer_for_ssh
#________________________________________________________________________
t_ssh_as_bearer()  {
echo tunnel viamillipede over multiple ssh tunnels this works handily and will happily boil your cpus doing parallel ssh expect 340mbps
ssh -N  -L 16661:localhost:14545 localhost  & 
sshpid1=$!
ssh -N  -L 16662:localhost:14545 localhost  & 
sshpid2=$!
$dutbin verbose $verboarg  rx 14545 > /dev/null &
rxpid=!
sleep 1
dd if=/dev/zero bs=1g count=10 | $dutbin tx localhost 16661 tx localhost 16662 verbose $verboarg threads 2
prisoners="$sshpid1 $sshpid2 $rxpid $prisoners"
}
t_ssh_as_bearer

#________________________________________________________________________
echo zeros over viamillipede, hot path test
dd if=/dev/zero bs=1g count=30 | $dutbin threads 2 verbose 3 rx 4545 tx localhost 4545  > /dev/null
#________________________________________________________________________

fini

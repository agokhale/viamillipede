#!/bin/sh 

morinfo() {
	echo moar
	set -x
}
trap morinfo  INFO

start_epochtime=`date +"%s"`
odir=`mktemp  -d  /tmp/viamillipede-smoke-${start_epochtime}.XXX`
verboarg="5"
timesource="HPET"
timerfreq=`sysctl -n kern.timecounter.tc.${timesource}.frequency`
threadcount=`sysctl -n hw.ncpu`

test_setup() { 
	test_name=$1
	size_mb=$2
	starttime=`sysctl -n kern.timecounter.tc.${timesource}.counter`
	testoutrdirfile="${odir}/${test_name}.testout"
	echo " ____________________________________________________________________________${test_name}________" 
}


get_testname() {
  echo $(test_name)
}
test_fin ( ) {
#args: size_mb starttime timerfreq
	echo end
	endtime=`sysctl -n kern.timecounter.tc.${timesource}.counter`
	thrpt=`dc -e "3k ${size_mb}  ${endtime} ${starttime}  -  ${timerfreq} /  /  p"`
	echo "__________________________________________________________________${test_name} throughput:${thrpt} (MBps)"
}

fini() {
	echo "foom, cleaning up"
	echo "escaped prisoners still running:"
	pgrep vmpdbin
	pkill vmpdbin
	kill $prisoners
	exit 0
}
trap fini  KILL INT TERM
#____________________________________________________________________________
#build it, stash the binary somewhere safe
make clean 
make  || exit  -101
git diff >  ${odir}/diff
dutbin=`mktemp "${odir}/vmpdbin.XXX"` || exit -11
allcollateralfiles=$dutbin
mv viamillipede $dutbin

#____________________________________________________________________________
t_est_dummy(){
test_setup dummy 10
sleep 1
test_fin dummy 
}

#____________________________________________________________________________
t_est_zeros_reference() {
test_setup  referencezeros10g 10000
#output=`time dd if=/dev/zero  bs=2m count=5000 2>- |  dd of=/dev/null bs=2m `
output=`time dd if=/dev/zero  bs=2m count=5000 2>- |  pv -s 10g > /dev/null `
echo $output
test_fin
}
t_est_zeros_reference
#____________________________________________________________________________

t_est_loopback_trivial() {
test_setup loopback 0.001
output=`echo asdf | $dutbin rx 4545 tx localhost 4545  threads  ${threadcount}`
case $output in
 "asdf") break ;;
 *) exit -4;;
esac
test_fin
}
t_est_loopback_trivial

#________________________________________________________________________
t_est_prbs() {
test_setup prbs 4096
$dutbin charmode verbose 0 prbs 0xd00f tx localhost 3434  threads ${threadcount} \
	rx 3434  leglimit 0x800  delayus 20 \
	 | pv -s4096 > /dev/null
prbpid=$!
#sleep 3 
#kill -INFO $prbpid
#sleep 0.2
#kill  $prbpid
echo
test_fin
}
t_est_prbs
#________________________________________________________________________
t_est_delay() {
test_setup delay 8
$dutbin charmode prbs 0xd00f tx localhost 3434 verbose 2 threads ${threadcount} \
	rx 3434  leglimit 4 delayus 500000  \
	 | pv -s8m > /dev/null
prbpid=$!
echo
test_fin
}
t_est_delay
exit
#________________________________________________________________________
dtflame()  {
nau=`date +"%s"`
$dutbin prbs 1 tx localhost 3434 verbose 3 threads ${threadcount} rx 3434 > /dev/null &
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
echo tunnel ssh over an fdx viamillipede this is probably a bad idea 
echo as ssh issues tinygrams and the system shoe shines around buffer sync trouble
$dutbin charmode threads ${threadcount} terminate 16622  tx localhost 4545 rx 4546  verbose $verboarg &
termpid=$!
sleep 1
$dutbin charmode threads ${threadcount} initiate localhost 22  tx localhost 4546 rx 4545  verbose $verboarg &
nitpid=$!
prisoners="$prisoners $nitpid $termpid"
sleep 3
test_setup t_bearer_for_ssh 64
ssh  -p 16622 localhost "dd if=/dev/urandom bs=1m count=64"  |  pv -s 64 > /dev/null
test_fin
prisoners="$prisoners $nitpid $termpid"
}
t_bearer_for_ssh
#________________________________________________________________________
t_ssh_as_bearer()  {
echo tunnel viamillipede over multiple ssh tunnels this works handily 
echo and will happily boil your cpus doing parallel ssh expect 340mbps
ssh -N  -L 16661:localhost:14545 localhost  & 
sshpid1=$!
ssh -N  -L 16662:localhost:14545 localhost  & 
sshpid2=$!
$dutbin verbose 5  rx 14545  | pv -s10g  /dev/null &
rxpid=!
sleep 1
test_setup ssh_as_bearer 10000
dd if=/dev/zero bs=1g count=10 | $dutbin tx localhost 16661 tx localhost 16662 verbose $verboarg threads 2
prisoners="${sshpid1} ${sshpid2} ${rxpid} ${prisoners} "
test_fin
}
t_ssh_as_bearer
#________________________________________________________________________
t_hotpath() {
test_setup hotpath 30000
echo zeros over viamillipede, hot path test
dd if=/dev/zero bs=1g count=30 | $dutbin threads ${threadcount} verbose 3 rx 4545 tx localhost 4545  |
	pv -s30g > /dev/null
test_fin
}
t_hotpath
#________________________________________________________________________

fini

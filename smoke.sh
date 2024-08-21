#!/bin/sh 
#set -ex

moreinfo() {
	set -x
	for ipid in $prisoners; do
		echo more info from $ipid
		kill -s USR1  $ipid
	done
}
trap moreinfo  INFO

start_epochtime=`date +"%s"`
gpayloadgen="dd if=/dev/zero bs=1m count=1000 | openssl enc -aes-128-cbc -nosalt -k swordfiiish -iter 1"
gpayloadmd5=` eval $gpayloadgen | md5 -q `

odir=`mktemp  -d  /tmp/viamillipede-smoke-${start_epochtime}.XXX`
verboarg="3"
timesource="HPET"
timerfreq=`sysctl -n kern.timecounter.tc.${timesource}.frequency`
threadcount=`sysctl -n hw.ncpu`

test_setup() { 
	test_name=$1
	size_mb=$2
	starttime=`sysctl -n kern.timecounter.tc.${timesource}.counter`
	testoutlog="${odir}/${test_name}.testout"
	echo -n "${test_name}:" 
}

get_testname() {
  echo $(test_name)
}
test_fin ( ) {
#args: size_mb starttime timerfreq
	retcode=$?
	endtime=`sysctl -n kern.timecounter.tc.${timesource}.counter`
	thrpt=`dc -e "3k ${size_mb}  ${endtime} ${starttime}  -  ${timerfreq} /  /  p"`
	echo "__________________________________________________________________${test_name} throughput:${thrpt} (MBps) retc:${retcode}"
	if [ $retcode != 0 ]; then
		echo "*************************************************Err"
		exit 
	fi
}

fini() {
	echo "foom, cleaning up"
	echo "escaped prisoners still running:"
	pgrep vmpdbin
	pkill $dutbin
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
mv obj/viamillipede $dutbin

#____________________________________________________________________________
t_est_dummy(){
test_setup dummy 10
sleep 1
test_fin dummy 
}
t_err() {
test_setup error 1
echo this should fail
[ "a" = "b" ]
test_fin error
}

#____________________________________________________________________________
t_est_zeros_reference() {
test_setup  referencezeros10g 10000
#output=`time dd if=/dev/zero  bs=2m count=5000 2>- |  dd of=/dev/null bs=2m `
output=`time dd if=/dev/zero  bs=2m count=5000   > /dev/null `
echo $output
test_fin
}
#____________________________________________________________________________

t_est_loopback_trivial() {
test_setup loopback 0.00001
output=`echo asdf | $dutbin rx 4545 tx localhost 4545  threads  ${threadcount}`
case $output in
 "asdf") break ;;
 *) exit -4;;
esac
test_fin
}

#________________________________________________________________________
t_est_prbs() {
test_setup prbs 4096
$dutbin charmode verbose $verboarg prbs 0xd00f tx localhost 3434  threads ${threadcount} \
	rx 3434  leglimit 0x800   \
	 > /dev/null &
prbpid=$!
prisoners="$prisoners $prbpid"
wait $prbpid
test_fin
}
#________________________________________________________________________
t_est_delay() {
test_setup delay 8
$dutbin charmode prbs 0xd00f tx localhost 3434 verbose $verboarg threads ${threadcount} \
	rx 3434  leglimit 4 delayus 5000000  \
	 > /dev/null &
prbpid=$!
prisoners="$prisoners $prbpid"
wait $prbpid
test_fin
}
#________________________________________________________________________
dtflame()  {
test_setup dtflame 1
nau=`date +"%s"`
$dutbin prbs 1 tx localhost 3434 verbose ${verboarg} threads ${threadcount} rx 3434 > /dev/null &
prbpid=$!
prisoners="$prisoners $prbpid"
sleep 0.3
sudo dtrace  -p $prbpid -qn'profile-1333hz /pid == $target/ { @[ustack()]=count();}  tick-5s{ exit(0);}' -o /tmp/viaustac$nau.ustacks
dtstackcollapse_flame.pl < /tmp/viaustac$nau.ustacks | flamegraph.pl > viamflame.svg
kill $prbpid
test_fin
}
#________________________________________________________________________
t_bearer_for_ssh() {
#echo tunnel ssh over an fdx viamillipede this is probably a bad idea 
#echo as ssh issues tinygrams and the system shoe shines around buffer sync trouble
$dutbin charmode threads ${threadcount} terminate 16622  tx localhost 4545 rx 4546  verbose $verboarg &
termpid=$!


$dutbin charmode threads ${threadcount} initiate localhost 22  tx localhost 4546 rx 4545  verbose $verboarg &
nitpid=$!
prisoners="$prisoners $nitpid $termpid"
sleep 3
test_setup t_bearer_for_ssh 1000
testmd5=`ssh  -p 16622 localhost " ${gpayloadgen} " | tee $odir/t_bearer_for_ssh.payload | md5 -q` 
#testmd5=`ssh  -p 16622 localhost " ${gpayloadgen} " | md5 -q` 
prisoners="$prisoners $nitpid $termpid"
[ ${gpayloadmd5} = ${testmd5} ]
test_fin
}
#________________________________________________________________________
t_ssh_as_bearer()  {
#echo tunnel viamillipede over multiple ssh tunnels this works handily 
#echo and will happily boil your cpus doing parallel ssh expect 340mbps
#threads must be set explicitly for 
ssh -N  -L 16661:localhost:14545 localhost  & 
sshpid1=$!
ssh -N  -L 16662:localhost:14545 localhost  & 
sshpid2=$!
dutbinmd5=`md5 -q $dutbin`
#set up termination 
$dutbin verbose  ${verboarg} rx 14545 | tee $odir/ypayloadbroke |  md5 -q > $odir/md5out  &
rxpid=$!
sleep 1
test_setup ssh_as_bearer 1000
eval $gpayloadgen | $dutbin tx localhost 16661 tx localhost 16662 verbose $verboarg threads 2
sleep 3
kill $sshpid2
kill $sshpid1

prisoners="${sshpid1} ${sshpid2} ${rxpid} ${prisoners} "
testmd5=`cat $odir/md5out`
[ ${gpayloadmd5} =  ${testmd5} ]
test_fin
}
#________________________________________________________________________
t_checksums() {
test_setup checksums 2000
dd if=/dev/zero bs=1m count=2000 | \
	$dutbin tx localhost 12323 threads ${threadcount} verbose  ${verboarg}  rx 12323  \
	checksums \
	2>&1  >  /dev/null
test_fin
}
t_hotpath() {
test_setup hotpath 10000
dd if=/dev/zero bs=1m count=10000 | \
	$dutbin tx localhost 12323 threads ${threadcount} verbose  ${verboarg}  rx 12323  \
	2>&1  >  /dev/null
test_fin
}
#________________________________________________________________________
t_est_dummy
t_est_zeros_reference
t_est_loopback_trivial
t_checksums
t_hotpath
t_est_delay
t_est_prbs
t_bearer_for_ssh
t_ssh_as_bearer
t_err
#________________________________________________________________________
fini

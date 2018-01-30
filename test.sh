#!/bin/sh 
verb=${1:-4}
howmuchpain=${2:-5} 
txhost="kaylee.a.aeria.net"
txpool="dozer"
txdataset="payloads"
txsnapshot="intial"
rxhost="mal.a.aeria.net"
rxhost2="192.168.238.1"
rxhost3="192.168.238.2"
rxhost4="192.168.238.3"
rxhost5="192.168.238.4"
rxhost6="192.168.238.5"
rxhost7="192.168.238.6"
rxhost8="192.168.238.8"
rxhost9="192.168.238.9"
rxport=12323
rxhost_graph="tx $rxhost2 $rxport tx $rxhost3 $rxport tx $rxhost4 $rxport \
	tx $rxhost5 $rxport tx $rxhost6 $rxport tx $rxhost7 $rxport tx $rxhost8 $rxport \
        tx $rxhost9 $rxport tx $rxhost $rxport "
txrsh="ssh root@$txhost "
rxrsh="ssh root@$rxhost "



dddriver() {
	# sizeinMB outfile infile
	siz=${1:-"1"}
	infile=${2:-"/dev/random"}
	outfile=${3:-"/$txpool/$txdataset/rand$siz.MB.payload"}
	echo "	  creating  random payload: $outfile"
	remfilestatus=`$txrsh  "ls $outfile.md5"  | wc -c`
	if [ '0'  -ne $remfilestatus  ]; then
		echo skipping  existing$outfile
	else
		$txrsh "dd  if=$infile of=$outfile bs=1m count=$siz &&\
		 	md5 $outfile  > $outfile.md5"
	fi
}

makepayloadds () {
	$txrsh " zfs create $txpool/payloads"
}
cleanpayloadds () {
	$txrsh " zfs destroy -r $txpool/payloads "
}

makepayloads () {
	makepayloadds
	echo  " creating $howmuchpain painful inputs"
	seq=`jot $howmuchpain 0  `  
	for cursor in $seq 
	do
		siz=`dc -e "2 $cursor 4 * ^ p"` 
		dddriver  $siz 
	done
	$txrsh "zfs snapshot  $txpool/$txdataset@initial"
}

zstreamref () {
	ds="dozer/bbone@c"
	target_ds="zz/sampletarget"
	user="root"
	rsh="ssh $user@$host "
	$rsh  " zfs destroy -r  $target_ds"
	#zfs send dozer/bbone@c | dd bs=8k | ssh root@delerium "zfs recv zz/bbt
	zfs send $ds  | dd bs=8k | $rsh "zfs recv $target_ds" 
}

zstreamremote () {
	ds="dozer/bbone@c"
	target_ds="zz/sampletarget"
	user="root"
	rsh="ssh $user@$host "
	
	$rsh "pkill viamillipede-sample "  
	pkill viamillipede
	$rsh  " zfs destroy -r  $target_ds"
	cat viamillipede |  $rsh "cat - > /tmp/viamillipede-sample" 
	$rsh "chmod 700 /tmp/viamillipede-sample"
 	$rsh "cd /tmp; ./viamillipede-sample rx $port verbose $verb  | zfs recv $target_ds  " 2> /tmp/verr &
	sshpid=$!
	sleep 1.8
	zfs send $ds  |  ./viamillipede  verbose $verb $targets  $threads
	#vmpid=$!
	sleep 1.8
	#wait $vmpid
	sudo kill $sshpid $tdpid $vmpid
	#cat /tmp/verr
	}
remotetest () {
	pwd
	pkill viamillipede
	$rsh "pkill viamillipede-sample " 
	cat viamillipede |  $rsh "cat - > /tmp/viamillipede-sample" &
	$rsh "rm  -f /tmp/junk" &
	#sudo tcpdump -i em0 -s0 -w /tmp/mili.pcap host $host and port $port  &
	tdpid=$!
	$rsh "chmod 700 /tmp/viamillipede-sample"
 	$rsh "cd /tmp; setenv millipede_deserialiser ' /bin/cat - ';   ./viamillipede-sample rx $port verbose $verb  > /tmp/junk  " 2> /tmp/verr &
	sshpid=$!
	#there is a splitsecond race while listeners are onlined; please excuse the gap
	sleep 1.7 
	viamillipede  verbose $verb $targets threads 9  < $sample &
	vmpid=$!
	sleep 1.7 
	#kill -s INFO $vmpid
	#sleep 10.7 
	wait $vmpid
	sudo kill $sshpid $tdpid $vmpid
	cat /tmp/verr
	set rem_md=`$rsh " md5 -q /tmp/junk"`
	set     md=`md5 -q $sample`
	if [ $md  -eq $rem_md ]; then 
		banner -w 40  pass
	else 
		banner -w 40 fail

	fi
	
	
}

ncref () {
	sample=$1	
	$rxrsh "pkill -f nc -l 12323 "
	$rxrsh "  nc -l 12323 > /dev/null " &
	sshpid=$!
	sleep 1.3;
	time_start 	
	$txrsh "zfs send $txpool/$txdataset@initial |  nc -N $rxhost2 12323  "
	time_stop nc_ref
	wait $sshpid

}
install_bin () {
	echo cleaning bins
	$txrsh "pkill viamillipede"
	$rxrsh "pkill viamillipede"
	cat viamillipede | $txrsh " cat -  > /tmp/viamillipede"
	cat viamillipede | $rxrsh " cat -  > /tmp/viamillipede"
	$txrsh "chmod 700 /tmp/viamillipede"
	$rxrsh "chmod 700 /tmp/viamillipede"
}

smoke() { 
	payloadstream=$1
	verb=$2 
	threads="threads $3"
	#$rxrsh "/tmp/viamillipede rx $rxport verbose $verb | md5 && echo remote done "  & 
	$rxrsh "/tmp/viamillipede rx $rxport verbose $verb > /dev/null && echo -n remote done "  & 
	sleep 0.5
	sshpid=$!
	time_start
	$txrsh "$payloadstream | /tmp/viamillipede $rxhost_graph verbose $verb $threads "
	time_stop "smoke-stream$1-verb$2-th$3"
	rempid=`$rxrsh "ps -auxw | grep -v grep | grep viamillipede" `
	if [  $rempid ] ; then
		echo "rx vmpd still running?"
		exit 4
	fi
	wait $sshpid
}

time_start()  {
	begint=`date +"%s"`
}
time_stop()  {
	endt=`date +"%s"`
	delta=`dc -e "$endt $begint - p "`
	echo "$1 took $delta(s)"
}

zsend_shunt () {
	#provide a reference for how fast the storage is, and what the pipe capability is
	time_start
	$txrsh "zfs send $txpool/$txdataset@initial > /dev/null"
	time_stop zsend_shunt
	time_start
	$txrsh "zfs send $txpool/$txdataset@initial | md5 "
	time_stop zsend_md5
}
zsend_dd_shunt () {
	#provide a reference for how fast the storage is, and what the pipe capability is
	time_start
	$txrsh "zfs send $txpool/$txdataset@initial | dd bs=64k > /dev/null"
	time_stop zsend_dd_shunt
}


#time_start 
#$txrsh "true"
#time_stop trvialcommand
makepayloads
install_bin 
zsend_shunt
zsend_dd_shunt
ncref
thread_counts=`jot 16 1`
for thread_count in $thread_counts 
do 
	smoke "zfs send $txpool/$txdataset@initial" 2 $thread_count
done
#$cleanpayloadds

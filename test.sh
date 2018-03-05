#!/bin/sh 
#!/bin/sh -x
pkill viamillipede
which_test=${1:-default}

catch_trap() {
        echo "caught trap pid $$  $* for $mytag -  cleaning up locks and dying"
	pkill viamillipede
	$rxrsh "pkill viamillipede"
	$rxrsh "pkill viamillipede"
}
#trap catch_trap TERM INT KILL BUS FPE 2 


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


ncref () {
	pkill  nc 
	nc -d -l 12323 > /dev/null &
	npid=$!
	sleep 1;
	time_start 	
	dd if=/dev/zero bs=1m count=10000 |  nc -N localhost 12323 
	time_stop "nc_ref"
}
install_bin () {
	echo cleaning bins
	make clean; make || exit 2
	$txrsh "pkill viamillipede; rm -f /tmp/viamillipede" 
	$rxrsh "pkill viamillipede; rm -f /tmp/viamillipede" 
	cat viamillipede | $txrsh " cat -  > /tmp/viamillipede"  
	cat viamillipede | $rxrsh " cat -  > /tmp/viamillipede" 
	$txrsh "chmod 700 /tmp/viamillipede" 
	$rxrsh "chmod 700 /tmp/viamillipede" 
}

smoke() { 
	payloadstream=$1
	remote_command=$2
	verb=$3 
	threads="threads $4"
	smoke_output="invalid_smoke"
	$rxrsh "/tmp/viamillipede rx $rxport verbose $verb  $remote_command " > /tmp/smoke_output & 
	sshpid=$!
	#sleep 1
	time_start
	$txrsh "$payloadstream | /tmp/viamillipede checksums $chaos $rxhost_graph verbose $verb $threads "
	time_stop "smoke payload: $payloadstream verbose: $verb remotecmd: $2  threads: $threads"
	wait $sshpid
	smoke_output=`cat /tmp/smoke_output; rm /tmp/smoke_output`
	export smoke_output
	#echo  "smoke_output: $smoke_output"
}

setup_checkphrase() {
	# seems that hosts that dont respond stall the connect routine
	pkill viamillipede
	$rxrsh "/tmp/viamillipede verbose 3 checkphrase tuuW  rx 12323 > /dev/null" &
	vrxpid=$!
	echo this fails. let it. dont die
	$txrsh " echo 'wat' | /tmp/viamillipede threads 3 verbose 3 checkphrase nope tx $rxhost 12323 " || true
	}
tiny() {
	$rxrsh "/tmp/viamillipede verbose 3 rx 12323 > /dev/null " &
	vrxpid=$!
	$txrsh "echo 'wat' | /tmp/viamillipede threads 3 verbose 3 tx $rxhost 12323"  || exit 1
	}
setup_hotpath() {
	$rxrsh "/tmp/viamillipede verbose 2 rx 12323 > /dev/null" &
	vrxpid=$!
	time_start
	$txrsh "dd if=/dev/zero bs=1m count=10000 | /tmp/viamillipede threads 8 verbose 2 $rxhost_graph " || exit 1
	time_stop "hotpath"
	# expected run on uncontended 10Gbps path: 
	#10485760000 bytes transferred in 10.722739 secs (977899372 bytes/sec)
 	#976.610 MBps
	#hotpath took 11(s)
}
deaddetect() {
	pkill viamillipede
	$rxrsh " /tmp/viamillipede verbose 4  rx 12323 > /dev/null" &
	vrxpid=$!
	time_start
	$txrsh "dd if=/dev/zero bs=1m count=10 | /tmp/viamillipede  checksums threads 16 verbose 4 tx $rxhost 12323  tx localhost 6666 " || exit 2
	time_stop "deaddetect"
	}
deaddetect_extended() {
	pkill viamillipede
	$rxrsh " /tmp/viamillipede verbose 4  rx 12323  > /dev/null " &
	vrxpid=$!
	time_start
	$txrsh "dd if=/dev/zero bs=1m count=1024 | /tmp/viamillipede checksums threads 16 verbose 4 tx $rxhost 12323  tx localhost 6666" || exit 3
	time_stop "deaddetect_extended"
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
	# actually we just wait on md5 mosttly
	time_start
	$txrsh "zfs send $txpool/$txdataset@initial > /dev/null"
	time_stop zsend_shunt
	time_start
	expected_md5=`$txrsh "zfs send $txpool/$txdataset@initial | md5 "`
	time_stop zsend_md5
}
zsend_dd_shunt () {
	#provide a reference for how fast the storage is, and what the pipe capability is
	time_start
	$txrsh "zfs send $txpool/$txdataset@initial | dd bs=64k > /dev/null"
	time_stop zsend_dd_shunt
}

check_output( ){
	if [ "$expected_md5" = "$smoke_output" ]; then
	
		echo pass
	else
		banner -w50 fail 
		exit -1
	fi
}
setup_smoke() {
	rxport=12324
	rxcommand=" | md5 " 
	verb=4
	thread_count=16
	rxhost_graph=" tx $rxhost $rxport"
	payload_generator="tar cf - /usr/share/doc"
	payload_generator="dd if=/dev/zero bs=64k count=10k" 
	payload_generator="/bin/dd if=/dev/zero bs=64k count=10k "
	chaos=" chaos 1050"
	badcrypto=" /usr/bin/openssl enc -aes-128-cbc -k bad_pass -S 5A "
	expected_md5=`$payload_generator | $badcrypto | md5  `
	smoke "$payload_generator | $badcrypto" " $rxcommand " $verb $thread_count
	#failcase smoke "$payload_generator " " $rxcommand " $verb $thread_count
	check_output
}



setup_grand() {
	txpool="dozer"
	txdataset="payloads"
	txsnapshot="intial"
	time_start 
	$txrsh "true"
	time_stop trvialcommand
	#makepayloads
	zsend_shunt
	# populate expectedmd5
	zsend_dd_shunt
	ncref
	thread_counts=`jot 16 1`
	setup_common
	payload_generator="zfs send $txpool/$txdataset@initial"
	for thread_count in $thread_counts 
	do 
		smoke " $payload_generator "  " | md5 "  2 $thread_count
		check_output
	done
	#$cleanpayloadds
}

setup_rxhostgraph() {
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
              tx $rxhost9 $rxport  "
}
setup_common(){
	txhost="kaylee.a.aeria.net"
	rxhost="mal.a.aeria.net"
	txrsh="ssh root@$txhost "
	rxrsh="ssh root@$rxhost "
	setup_rxhostgraph
	install_bin
}

setup_default() {
	setup_checkphrase
	tiny
	ncref
	deaddetect
	deaddetect_extended
	setup_grand
	setup_hotpath
}
setup_common
setup_$which_test

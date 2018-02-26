#include "worker.h"

int dispatch_idle_worker ( struct txconf_s * txconf ) {
	int retcode =-1 ; 
	txstatus ( txconf , 5); 
	int spins = 0; 
	int sleep_thief =0; 
	while (   retcode < 0  ) {
		pthread_mutex_lock (&(txconf->mutex));
		for ( int i = 0 ; (i < txconf->worker_count) && (retcode < 0 ) ; i++ ) {
			if ( txconf->workers[i].state == 'i' ) {
				retcode = i; 
				sleep_thief=0; 
				spins = 0 ;	
			} else {
			}
		}
		assert ( retcode  < kthreadmax );
		pthread_mutex_unlock (&(txconf->mutex));
		if (retcode <  0 ) {
			//txstatus ( txconf , 19); 
			spins++; 
			//whisper ( 11, "no workers available backing off spins: %i\n", spins ); 
			sleep_thief ++; 
			usleep( sleep_thief);
		}
	}
	assert ( retcode  < kthreadmax );
return (retcode); 
}

void start_worker ( struct txworker_s * txworker ) {
	assert ( txworker->id < kthreadmax); 
	pthread_mutex_lock (&(txworker->txconf_parent->mutex));
	txworker->state='d'; 	
	pthread_mutex_unlock (&(txworker->txconf_parent->mutex));
	txstatus ( txworker->txconf_parent, 6 ); 
}
	
//dump stdin in chunks
// this is single thread and should use memory at a drastic pace
void txingest (struct txconf_s * txconf ) {
	int readsize;
	int in_fd = STDIN_FILENO;
	int foot_cursor =0;
	int done =0; 
	unsigned long saved_checksum = 0xff;
	int ingest_leg_counter = 0; 
	txconf->stream_total_bytes=0; 
	checkperror("nuisancse ingest err");
	while ( !done ) {
		int worker = dispatch_idle_worker ( txconf ); 
		assert (  (txconf->workers[worker].buffer) != NULL ); 
		readsize = bufferfill (  in_fd ,(u_char *) (txconf->workers[worker].buffer) , kfootsize  ) ;  
		whisper ( 8, "\ntxw:%i read leg %i : fd:%i siz:%i\n",worker,  ingest_leg_counter, in_fd, kfootsize); 
		assert ( readsize <= kfootsize ); 
		if ( readsize > 0  ) { 
			// XXX find the idle worker , lock it and dispatch as seprarte calls -- perhaps
			txconf->workers[worker].pkt.size = readsize;   
			txconf->workers[worker].pkt.leg_id = ingest_leg_counter; 
			txconf->workers[worker].pkt.opcode=feed_more; 
#ifdef vmpd_strict
			//XXX expensive
			txconf->workers[worker].pkt.checksum = 
				mix ( saved_checksum + ingest_leg_counter , 
					txconf->workers[worker].buffer, 
					readsize);  
			saved_checksum = txconf->workers[worker].pkt.checksum;
#endif
			start_worker ( &(txconf->workers[worker]) );
			txconf->stream_total_bytes += readsize ; 
		} else { 
			whisper ( 4, "txingest no more stdin, send shutdown"); 
			done = 1; 
			int worker = dispatch_idle_worker ( txconf ); 
			txconf->workers[worker].pkt.size=0; 
			txconf->workers[worker].pkt.leg_id = ingest_leg_counter; 
			txconf->workers[worker].pkt.opcode=end_of_millipede; 
			start_worker ( &(txconf->workers[worker]) );
		}
		ingest_leg_counter ++; 
	}

	whisper ( 4, "tx:ingest complete for %lu(bytes) in ",txconf->stream_total_bytes); 
	u_long usecbusy = stopwatch_stop( &(txconf->ticker),4 );
	//bytes per usec - thats interesting 
	whisper (6, " %8.4f MBps\n" , ( txconf->stream_total_bytes *0.0000001) / (0.000001 * usecbusy  )    );
}


int txpush ( struct txworker_s *txworker ) {
	//push this buffer out the socket
	int retcode = 1;
	int writelen =-1; 
	int cursor=0 ;
	/*
	if ( txworker->writeremainder > kfootsize  ) { 
		whisper ( 2, "tx villany, the remaining buffer must not be larger ( %i) than the buffer, %i", 
		txworker->writeremainder , kfootsize); 
	}
	assert ( txworker->writeremainder <= kfootsize ); 
	*/
 	/*	
	write (txworker->sockfd ,  preamble, 2);  // this XXX hsoule be  a protocol
		whisper (9, "."); 
	write (txworker->sockfd ,  &(txworker->pkt.size), sizeof(int)); 
		whisper (9, "."); 
	write (txworker->sockfd ,  &(txworker->bufferleg), sizeof(int)); 
		whisper (9, "."); 
	*/		
	txworker->state='a';// preAmble
	write (txworker->sockfd , &txworker->pkt, sizeof(struct millipacket_s)); 
	txworker->writeremainder = txworker->pkt.size;
	assert ( txworker->writeremainder <= kfootsize ); 
	while (  txworker->writeremainder  && retcode ) {
		int minedsize = MIN (MAXBSIZE,txworker->writeremainder);
		txworker->state='P'; // 'P'ush
		if ( chaos_fail () ) {
			close (txworker->sockfd);  // things fail sometimes
		}
		writelen = write(txworker->sockfd , ((txworker->buffer)+cursor) , minedsize ) ;
		if ( errno != 0 ) { retcode = 0 ;}; 
		checkperror (" write to socket" ); 
		txworker->state='p'; // 'p'ush complete
		txworker->writeremainder -= writelen; 
		//assert ( txworker->writeremainder <= kfootsize ); 
		cursor += writelen; 
		//whisper (10, "txw:%i push leg:%lu.(+%i -%i)  ",txworker->id,txworker->pkt.leg_id,writelen,txworker->writeremainder); 
	}
	checkperror( "writesocket"); 
	//assert ( writelen );
	pthread_mutex_lock (&(txworker->txconf_parent->mutex));
	if ( retcode == 1)   {
		assert ( txworker->writeremainder == 0 );
		txworker->state='i';  // idle ok
	} else  {
		txworker->state='x';  // dead  do not transmit more
	}
	pthread_mutex_unlock (&(txworker->txconf_parent->mutex));
	txworker->pkt.size=0; 
	//whisper ( 6 , "txw:%i  leg:%lu idled \n" , txworker->id, txworker->pkt.leg_id); 
	//txworker->pkt.leg_id=0; 
	//pthread_mutex_unlock( &(txworker->mutex)); 
	return retcode; 
}
int tx_tcp_connect_next ( struct txconf_s *  txconf  ) {
	// pick a port/host from the list, cycleing through  them 
	txconf->target_port_cursor ++;
	txconf->target_port_cursor %= txconf->target_port_count ;
	assert ( txconf->target_port_cursor < txconf->target_port_count); 
	return ( tcp_connect ( 
		txconf->target_ports[ txconf->target_port_cursor].name, 
		txconf->target_ports[ txconf->target_port_cursor].port
		) 
	); 
}
void txworker (struct  txworker_s *txworker ) {
	int done =0; 
	int retcode =-1; 
	char local_state = 0 ;
	char hellophrase[]="yoes";
	char checkphrase[]="ok";
	int state_spin =0; 
	int sleep_thief =0; 
	char readback[2048]; 
	pthread_mutex_init ( &(txworker->mutex)	, NULL ) ; 
	pthread_mutex_lock ( &(txworker->mutex));
	txworker->state = 'c'; //connecting
	/*
	txworker->sockfd = tcp_connect ( 
		txworker->txconf_parent->target_ports[txworker->id % target_count].name, 
		txworker->txconf_parent->target_ports[txworker->id % target_count].port); 
	*/
	txworker->sockfd = tx_tcp_connect_next ( txworker->txconf_parent ); 
	// this can flood the remote end trivially on high bw networks resulting in this syndrome and a partially connnected graph
	/*
	sonewconn: pcb 0xfffff8014ba261a8: Listen queue overflow: 10 already in queue awaiting acceptance (2 occurrences)
	pid 4752 (viamillipede), uid 1001: exited on signal 6 (core dumped)
	*/
	//can the remote end talk? 
	//this is lame  but rudimentary q/a session will assert that the tcp stream is able to bear traffic
	whisper ( 18, "txw:%i send yoes\n", txworker->id);
	retcode = write (txworker->sockfd, hellophrase, 4); 
	checkperror ( "tx: write fail"); 
	whisper ( 18, "txw:%i expect ok \n", txworker->id);
	retcode = read (txworker->sockfd, &readback, 2); 
	checkperror ("tx: read fail"); 
	if  ( bcmp ( checkphrase, readback, 2 ) != 0 )  {
		whisper ( 5, "txw: %i checkphrase failure readlen:%i",  txworker->id, retcode ); 
	}
	assert ( bcmp ( checkphrase, readback, 2 ) == 0 ); 
	whisper ( 8, "txw:%i online and idling fd:%i\n", txworker->id, txworker->sockfd);
	//txstatus (txworker->txconf_parent,7); 
	txworker->state = 'i'; //idle
	txworker->pkt.size = 0 ; 
	txworker->writeremainder=-88; 
	txworker->pkt.leg_id=0;
	txworker->pkt.preamble = preamble_cannon_ul;
	checkperror( "worker buffer allocator");
	pthread_mutex_unlock ( &(txworker->mutex));
	while ( !done ) {
		pthread_mutex_lock ( &(txworker->txconf_parent->mutex));
		local_state = txworker->state; 
		pthread_mutex_unlock ( &(txworker->txconf_parent->mutex));
		switch (local_state) {
			/* valid states:
				E: uninitialized
				a: premable
				c: connected
				d: loaded; => push
				Pp: pushing
				i: idle but connected
				x: disconnected , still bearing a buffer .. can't bail yet reconnect  => push
				n: not yet connected,  new no buffer ??? connect => idle
				
			*/
			case 'i': break; //idle
			case 'd': 
				 if ( txpush ( txworker ) == 0) {
					// retry  this
					whisper ( 2, "txw: %i failed, unconnected , will retry leg %lu ", txworker->id , txworker->pkt.leg_id); 
				} 
				sleep_thief=0; 
				break; 
			default: assert( -1 && "bad zoot");
			}
		state_spin ++; 
		if ( (state_spin % 30000) == 0 && ( txworker->state == 'i' ) )  {
			//txstatus ( txworker -> txconf_parent,10 ) ; 
			whisper ( 9, "txw:%i is lonely after %i spins \n", txworker->id, state_spin); 
		}
		sleep_thief ++; // this Looks crazy ; but it's good for 30% 
		// this works by allowing a context switch and permitting a dispatch ready thread to push
		usleep ( sleep_thief ); 
	} // while !done 
	pclose ( txworker->pip); 
} //  txworker

void txlaunchworkers ( struct txconf_s * txconf) {
	int worker_cursor = 0;
	int ret;	
	while ( worker_cursor < txconf->worker_count )  {
		txconf->workers[worker_cursor].state = '0'; // unitialized
		txconf->workers[worker_cursor].txconf_parent = txconf; // allow inpection/inception
		txconf->workers[worker_cursor].pkt.leg_id = 0; // 
		txconf->workers[worker_cursor].pkt.size = -66; // 
		txconf->workers[worker_cursor].sockfd = -66; // 
		whisper( 16, "txw:%i launching ", worker_cursor); 
		txconf->workers[worker_cursor].state = 'L';
		txconf->workers[worker_cursor].id = worker_cursor; 
		//allocate before thread launch
		txconf->workers[worker_cursor].buffer = calloc ( 1,(size_t) kfootsize ); 
		assert (txconf->workers[worker_cursor].buffer != NULL && "insufficient memory up front" ); 
		//digression: pthreads murders all possible kittens stored in  argument types
		ret = pthread_create ( 
			&(txconf->workers[worker_cursor].thread ),
			NULL ,
			//clang suggest this gibberish, why question?
			(void *(* _Nonnull)(void *))&txworker,  
			&(txconf->workers[worker_cursor]) 
			);
		checkperror ("pthread launch"); 
		assert ( ret == 0 && "pthread launch"); 
		worker_cursor++;
		usleep ( 10 * 1000);  
		// 10ms standoff  to increase the likelyhood that PCBs are available on the rx side
		}	
	txstatus ( txconf, 5);
}

void txstatus ( struct txconf_s* txconf , int log_level) {
	whisper ( log_level, "\n");
	
	for ( int i=0; i < txconf->worker_count ; i++) {
		
		whisper(log_level, "%c:%lu-%i ", 
				txconf->workers[i].state, 
				txconf->workers[i].pkt.leg_id,
				(txconf->workers[i].writeremainder) >> 10 //kbytes are sufficient
			);
		}
}
void txbusyspin ( struct txconf_s* txconf ) {
	// wait until all legs are pushed; called after ingest is complete
	// if there are launche/dispatched /pushing workers; hang here
	int done =0; 
	int busy_cycles=0; 
	char  instate='E';  //error uninitialized
	while (!done) {
		usleep ( 1000 );  // e^n backoff? 
		if ( (busy_cycles % 100) == 0 ) txstatus( txconf, 4 ); 
		busy_cycles++; 
		int busy_workers = 0; 
		for ( int i =0; i < txconf->worker_count ; i++ ) {
			//pthread_mutex_lock (  &(txconf->workers[i].mutex) ); 
			instate =  txconf->workers[i].state	;
			//pthread_mutex_unlock (  &(txconf->workers[i].mutex) ); 
			if ( instate != 'i') busy_workers++;  // XXX janky structure  continue
		} 
		done = ( busy_workers == 0 ) ; 
	}
	whisper ( 6, "\ntx: all workers idled after %i spins\n", busy_cycles); 
}
struct txconf_s *gtxconf; 
void wat ( ) {
	struct txconf_s *txconf=gtxconf;
	whisper ( 1, "\n%lu(mbytes) in ",txconf->stream_total_bytes >> 20); 
	u_long usecbusy = stopwatch_stop( &(txconf->ticker),2 );
	whisper ( 1 , "\n" ); 
	txstatus  ( txconf, 1 );  
	//bytes per usec - thats interesting  bytes to mb 
	whisper (1, "\n %8.4f MBps\n" , ( txconf->stream_total_bytes / ( 1.0 *  usecbusy  ))    );
}

void partingshot() {
	struct txconf_s *txconf=gtxconf;
	wat ( ); 
	whisper ( 2, "exiting after signal"); 
	checkperror ( " signal caught"); 
	exit (-6);	
}

void tx (struct txconf_s * txconf) {
	int retcode; 
	int done = 0; 
	gtxconf = txconf;	
	//start control channel
        signal (SIGINFO, &wat);
        signal (SIGINT, &partingshot);
        signal (SIGHUP, &partingshot);
	pthread_mutex_init ( &(txconf->mutex), NULL ) ; 
	stopwatch_start( &(txconf->ticker) ); 
	txlaunchworkers( txconf ); 	
	txingest ( txconf ); 
	txbusyspin ( txconf ); 

	whisper ( 2, "all complete for %lu(bytes) in ",txconf->stream_total_bytes); 
	u_long usecbusy = stopwatch_stop( &(txconf->ticker),2 );
	//bytes per usec - thats interesting   ~== to mbps
	whisper (1, " %8.4f mbps\n" , ( txconf->stream_total_bytes  / ( 1.0 * usecbusy  ))    );
}

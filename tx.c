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
		pthread_mutex_unlock (&(txconf->mutex));
		if (retcode <  0 ) {
			//txstatus ( txconf , 19); 
			spins++; 
			//whisper ( 11, "no workers available backing off spins: %i\n", spins ); 
			sleep_thief ++; 
			usleep( sleep_thief);
		}
	}
return (retcode); 
}

void start_worker ( struct txworker_s * txworker ) {
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
	u_char taste_buf; 
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


void txpush ( struct txworker_s *txworker ) {
	//push this buffer out the socket
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
	while (  txworker->writeremainder  ) {
		int minedsize = MIN (MAXBSIZE,txworker->writeremainder);
		txworker->state='P'; // 'P'ush
		writelen = write(txworker->sockfd , ((txworker->buffer)+cursor) , minedsize ) ;
		txworker->state='p'; // 'p'ush complete
		txworker->writeremainder -= writelen; 
		//assert ( txworker->writeremainder <= kfootsize ); 
		cursor += writelen; 
		//whisper (10, "txw:%i push leg:%lu.(+%i -%i)  ",txworker->id,txworker->pkt.leg_id,writelen,txworker->writeremainder); 
	}
	assert ( txworker->writeremainder == 0 );
	checkperror( "writesocket"); 
	//assert ( writelen );
	pthread_mutex_lock (&(txworker->txconf_parent->mutex));
	txworker->state='i'; 
	pthread_mutex_unlock (&(txworker->txconf_parent->mutex));
	txworker->pkt.size=0; 
	//whisper ( 6 , "txw:%i  leg:%lu idled \n" , txworker->id, txworker->pkt.leg_id); 
	//txworker->pkt.leg_id=0; 
	//pthread_mutex_unlock( &(txworker->mutex)); 
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
	int target_count = txworker->txconf_parent->target_port_count;
	assert ( target_count > 0 ); 
	txworker->sockfd = tcp_connect ( 
		txworker->txconf_parent->target_ports[txworker->id % target_count].name, 
		txworker->txconf_parent->target_ports[txworker->id % target_count].port); 
	//can the remote end talk? 
	//this is lame  but rudimentary q/a session will assert that the tcp stream is able to bear traffic
	retcode = write (txworker->sockfd, hellophrase, 4); 
	checkperror ( "write fail"); 
	retcode = read (txworker->sockfd, &readback, 2); 
	checkperror ("read fail"); 
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
			case 'i': break; //idle
			case 'd': txpush ( txworker ); sleep_thief=0; break; 
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

	for ( int i=0; i < 16 ; i++) {
		txconf->workers[i].state = '0'; // unitialized
		txconf->workers[i].txconf_parent = txconf; // allow inpection/inception
		txconf->workers[i].pkt.leg_id = 0; // 
		txconf->workers[i].pkt.size = -66; // 
		txconf->workers[i].sockfd = -66; // 
	}

	while ( worker_cursor < txconf->worker_count )  {
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
	int busy_cycles; 
	char  instate; 
	while (!done) {
		usleep ( 1000);  // e^n backoff?
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
	whisper ( 4, "\nall workers idled after %i spins\n", busy_cycles); 
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

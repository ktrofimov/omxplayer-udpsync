#include "OMXPlayerSync.h"

OMXPlayerSync::OMXPlayerSync( SyncType _syncType, int _numNodes, int _port )
{
   jitter = 0;
   syncType = _syncType;
   syncTimeState = ONTRACK;
   numNodes = _numNodes;
   port = _port;
   serverAddress = "";
   alpha = 0;
   pause = false;
   reply = strdup ( "0" ); // Can be anything at start
   forceRebind = 1;
   updateTicker = 2 * UPDATE_THRESHOLD; // want to force an update on the first try
}

OMXPlayerSync::OMXPlayerSync( SyncType _syncType, int _port, const string &_serverAddress )
{
   jitter = 0;
   syncType = _syncType;
   syncTimeState = ONTRACK;
   port = _port;
   serverAddress = _serverAddress;
   alpha = 0;
   pause = false;
   question = strdup ( "1" ); // Can be anything
   forceRebind = 1;
   updateTicker = 2 * UPDATE_THRESHOLD; // want to force an update on the first try
}

OMXPlayerSync::~OMXPlayerSync()
{
   closeConnection ();
}

void OMXPlayerSync::reset ()
{
   jitter = 0;
   syncTimeState = ONTRACK;
   updateTicker = 2 * UPDATE_THRESHOLD; // want to force an update on the first try
}

void OMXPlayerSync::update ( int src_alpha )
{
   if( ( syncType == SYNC_SERVER )  )
    {
      alpha = src_alpha;
      syncServer();
    }

   if ( updateTicker >= UPDATE_THRESHOLD )
   {
      if( ( syncType == SYNC_CLIENT )  )
      {
//	 printf( "\r%d = ", updateTicker );
	 syncClient( true );
         updateJitter ();
         updateSpeed ();
      }
      updateTicker = 0;
      displayVerbose ();
   }
   else
   {
      syncClient( false );
      updateTicker++;
   }
}

void OMXPlayerSync::updateJitter ()
{
   if ( clientTime > serverTime + JITTER_TOLERANCE )
   {
      syncTimeState = AHEAD;
      jitter -= JITTER_DELTA;
   }
   else if ( clientTime < serverTime - JITTER_TOLERANCE )
   {
      syncTimeState = BEHIND;
      jitter += JITTER_DELTA;
   }
   else
   {
      syncTimeState = ONTRACK;
      // jitter = 0;
   }
}

void OMXPlayerSync::setPort ( int _port )
{
   port = _port;
}

void OMXPlayerSync::updateSpeed ()
{
   if ( syncTimeState != ONTRACK )
   {
//      printf( "SYNC updateSpeed %d\n", playspeeds[playspeed_current] + S(jitter) );
//      SetSpeed(playspeeds[playspeed_current] + S(jitter));
   }
}

float OMXPlayerSync::getJitter ()
{
   return jitter;
}

struct TimeSync
{
    char source;
    double cTime;
    double sTime;
};

int OMXPlayerSync::ServerBcast ()
{
     static double lastTime=0;

     serverTime = stamp;
     if( serverTime == lastTime ) // the same timeframe - just return
	return 0;
     lastTime = serverTime;

     sockaddr_in bcast;
     memset((void *) &bcast, 0, sizeof(struct sockaddr_in));
     server.sin_family = AF_INET;
     server.sin_port = htons( port );
     server.sin_addr.s_addr = htonl( INADDR_BROADCAST );

     TimeSync ts;
     ts.source = 'S';
     ts.cTime = alpha;
     ts.sTime = serverTime;

     if (-1 == sendto(sockfd, &ts, sizeof(ts), 0, (struct sockaddr*) &server, sizeof(server)) )
     {
        perror("0:bcast parent");
        return 1;
     }
    // resend down to DMX process
    sockaddr_in localhost;
    memset((void *) &localhost, 0, sizeof(struct sockaddr_in));
    localhost.sin_family = AF_INET;
    localhost.sin_port = htons( port+3 ); // send to DMX controller
    localhost.sin_addr.s_addr = htonl( (127<<24)|(0<<16)|(0<<8)|1 );
    if (-1 == sendto(sockfd, buff, sizeof(TimeSync), 0, (struct sockaddr*) &localhost, sizeof(localhost)) )
    {
      perror("1:send");
    }

     // test code
     // printf( "SBCAST P:%d, A:%d\n", port, alpha );
     return 0;
}

int OMXPlayerSync::atFadingIn ()
{
     sockaddr_in localhost;
     memset((void *) &localhost, 0, sizeof(struct sockaddr_in));
     localhost.sin_family = AF_INET;
     localhost.sin_port = htons( (port/10)*10 ); // send to play controller
     localhost.sin_addr.s_addr = htonl( (127<<24)|(0<<16)|(0<<8)|1 );

     TimeSync ts;
     ts.source = 'I';
     ts.cTime = port;
     ts.sTime = -1;

     if( -1 == sendto(sockfd, &ts, sizeof(ts), 0, (struct sockaddr*) &localhost, sizeof(localhost)) )
     {
        perror("0:send to master");
        return 1;
     }
     printf( "At fading in\n" );
     return 0;
}

int OMXPlayerSync::atFadingOut ()
{
     sockaddr_in master;
     memset((void *) &master, 0, sizeof(struct sockaddr_in));
     master.sin_family = AF_INET;
     master.sin_port = htons( (port/10)*10 ); // send to play controller
     master.sin_addr.s_addr = htonl( (127<<24)|(0<<16)|(0<<8)|1 );

     TimeSync ts;
     ts.source = 'O';
     ts.cTime = port;
     ts.sTime = -1;

     if( -1 == sendto(sockfd, &ts, sizeof(ts), 0, (struct sockaddr*) &master, sizeof(master)) )
     {
        perror("0:send to master");
        return 1;
     }
     printf( "At fading out\n" );
     return 0;
}

int OMXPlayerSync::atPlayUnpause ()
{
     sockaddr_in master;
     memset((void *) &master, 0, sizeof(struct sockaddr_in));
     master.sin_family = AF_INET;
     master.sin_port = htons( (port/10)*10 ); // send to play controller
     master.sin_addr.s_addr = htonl( (127<<24)|(0<<16)|(0<<8)|1 );

     TimeSync ts;
     ts.source = 'f';
     ts.cTime = port;
     ts.sTime = -1;

     if( -1 == sendto(sockfd, &ts, sizeof(ts), 0, (struct sockaddr*) &master, sizeof(master)) )
     {
        perror("0:send to master");
        return 1;
     }
     printf( "At play unpause\n" );
     return 0;
}

int OMXPlayerSync::syncServer ()
{
   sockaddr_in ca;
   socklen_t slen=sizeof(ca);

   if ( syncType == SYNC_SERVER )
   {
    while( pause )
    {
     // Receive the tcp messages
     bytesreceived = recvfrom( sockfd, buff, BUFFLEN, 0, (struct sockaddr *) &ca, &slen);
     if ( -1 != bytesreceived) {
        if( ((TimeSync*)buff)->source == 'G' ) // master start
        {
            printf( "%d: Unpause movie\n", port );
            pause = false;
        }

        if( ((TimeSync*)buff)->source == 'E' ) // master exit
        {
            printf( "%d Master exit request\n", port );
            pause= false;
            exitPlayer();
        }
     }
     usleep( 1000 );
    }

     // Broadcast current position
// printf( "FR: %f\r", serverTime );
     ServerBcast();

     // Receive the tcp messages
     bytesreceived = recvfrom( sockfd, buff, BUFFLEN, 0, (struct sockaddr *) &ca, &slen);
     if (-1 == bytesreceived) {
        return 1;
     }

     if( ((TimeSync*)buff)->source == 'E' ) // master exit
     {
        printf( "%d Master exit request\n", port );
        pause= false;
        exitPlayer();
     }

     // then rapidly send a message back
     if( ((TimeSync*)buff)->source == 'C' ) // client request
     {
/*	printf("F:%d P:%d A:%d.%d.%d.%d\n", ca.sin_family, ca.sin_port,
	    ((ca.sin_addr.s_addr) & 0xFF), ((ca.sin_addr.s_addr>>8) & 0xFF), 
	    ((ca.sin_addr.s_addr>>16) & 0xFF), ((ca.sin_addr.s_addr>>24) & 0xFF) );
*/
         TimeSync ts;
         ts.source = 'R';
         ts.cTime = ((TimeSync*)buff)->cTime;
         ts.sTime = serverTime;
         if( -1 == sendto(sockfd, &ts, sizeof(ts), 0, (struct sockaddr*) &ca, slen) )
         {
            perror("0:send parent");
            return 1;
         }
     }

     if( ((TimeSync*)buff)->source == 'G' ) // master go
     {
	printf( "Master play request\n" );
	unpauseMovie();
     }

     if( ((TimeSync*)buff)->source == 'F' ) // Sound fade request
     {
	printf( "Sound fade request\n" );
	fadeSound();
     }
     pause = false;
   }
}

double diff_sliding[3] = {-1000.0, -1000.0, -1000.0};

int OMXPlayerSync::syncClient ( bool send )
{
   static bool slowDown=false;
   static bool speedUp=false;
   sockaddr_in srv;
   socklen_t slen=sizeof( srv );
   if ( syncType == SYNC_CLIENT )
   {
      clientTime = stamp;
      do {

      if( send && server.sin_addr.s_addr != INADDR_ANY )
      {
          TimeSync ts;
          ts.source = 'C';
          ts.cTime = clientTime;
          ts.sTime = -1;

          if (-1 == sendto(sockfd, &ts, sizeof(ts), 0, (struct sockaddr*) &server, sizeof(server))) {
	     perror("1:send");
          }
      }
//      printf( "CLI: %10.2f SRV: %10.2f\r", clientTime, serverTime );

      bytesreceived = recvfrom(sockfd, buff, BUFFLEN, 0, (struct sockaddr *) &srv, &slen);
      if ( bytesreceived != -1 )
      {
	static float nextSecond = -1;
        if( ((TimeSync*)buff)->source == 'R' && clientTime >0 ) // Server response
	{
	    serverTime = ((TimeSync*)buff)->sTime;
	    if( ((TimeSync*)buff)->cTime != -1 )
	    {
		double diff = (serverTime-( ((TimeSync*)buff)->cTime + clientTime)/2)/(0.040*DVD_TIME_BASE);
		if( diff_sliding[2] == -1000.0 )
		{
		    diff_sliding[2] = diff;
		    diff_sliding[1] = diff;
		    diff_sliding[0] = diff;
		}
		else
		{
		    diff_sliding[2] = diff_sliding[1];
		    diff_sliding[1] = diff_sliding[0];
		    diff_sliding[0] = diff;
		}
                diff = (diff_sliding[0]+diff_sliding[1]+diff_sliding[2] )/3;
		printf( "D=%10.2f D0=%10.2f D1=%10.2f D2=%10.2f CLI: %10.2f CRT: %10.2f SRV: %10.2f\n",
		     diff,
		     diff_sliding[0],
		     diff_sliding[1],
		     diff_sliding[2],
		     clientTime,
		     ((TimeSync*)buff)->cTime,
		     serverTime );
		if( diff < -1.2 && !slowDown )
		{
//		    pauseMovie();
//		    usleep( (serverTime-( ((TimeSync*)buff)->cTime + clientTime)/2) );
printf("\nSLOW\n");
		    jitter = diff/70; // -1/50 = -0.02;
		    unpauseMovie();
		    speedUp = false;
		    slowDown = true;
		}
		if( diff > 0.5 && !speedUp )
		{
//		    pauseMovie();
printf("\nFAST\n");
		    jitter = diff /70; // 1/50 = 0.02;
		    unpauseMovie();
		    speedUp = true;
		    slowDown = false;
		}
		if( diff > -1.2 && slowDown )
		{
//		    pauseMovie();
printf("\nSLOW->NORMAL\n");
		    jitter = 0;
		    unpauseMovie();
		    slowDown = false;
		    speedUp = false;
		}
		if( diff < 0.5 && speedUp )
		{
//		    pauseMovie();
printf("\nFAST->NORMAL\n");
		    jitter = 0;
		    unpauseMovie();
		    slowDown = false;
		    speedUp = false;
		}
	    }
	}

     if( ((TimeSync*)buff)->source == 'E' ) // master exit
     {
	printf( "%d Master exit request\n", port );
        pause= false;
	exitPlayer();
     }

     if( ((TimeSync*)buff)->source == 'F' ) // Sound fade request
     {
	printf( "Sound fade request\n" );
	fadeSound();
     }

        if( ((TimeSync*)buff)->source == 'G' ) // Server play
	{
	    printf( "Client %d: Master play request\n", port );
	    serverTime = 0;
	    // resend down to DMX process
            sockaddr_in localhost;
            memset((void *) &localhost, 0, sizeof(struct sockaddr_in));
            localhost.sin_family = AF_INET;
            localhost.sin_port = htons( port+3 ); // send to DMX controller
            localhost.sin_addr.s_addr = htonl( (127<<24)|(0<<16)|(0<<8)|1 );
            if (-1 == sendto(sockfd, buff, sizeof(TimeSync), 0, (struct sockaddr*) &localhost, sizeof(localhost)) )
            {
	      perror("1:send");
            }
            pause = false;
	}

        if( ((TimeSync*)buff)->source == 'S' ) // Server broadcast
	{
            // store server address
            server.sin_addr.s_addr = srv.sin_addr.s_addr;
//printf( "CLI: %10.2f SRV: %10.2f\n", clientTime, serverTime );
	    serverTime = ((TimeSync*)buff)->sTime;
	    // resend down to DMX process
            sockaddr_in localhost;
            memset((void *) &localhost, 0, sizeof(struct sockaddr_in));
            localhost.sin_family = AF_INET;
            localhost.sin_port = htons( port+3 ); // send to DMX controller
            localhost.sin_addr.s_addr = htonl( (127<<24)|(0<<16)|(0<<8)|1 );
            if (-1 == sendto(sockfd, buff, sizeof(TimeSync), 0, (struct sockaddr*) &localhost, sizeof(localhost)) )
            {
	      perror("1:send");
            }
/*        if( ((TimeSync*)buff)->source == 'G' ) // Server play
	{
	    serverTime = 0;
	    // resend down to DMX process
            sockaddr_in localhost;
            memset((void *) &localhost, 0, sizeof(struct sockaddr_in));
            localhost.sin_family = AF_INET;
            localhost.sin_port = htons( port+3 ); // send to DMX controller
            localhost.sin_addr.s_addr = htonl( (127<<24)|(0<<16)|(0<<8)|1 );
            if (-1 == sendto(sockfd, buff, sizeof(TimeSync), 0, (struct sockaddr*) &localhost, sizeof(localhost))) {
	      perror("1:send");
            pause = false;
	}*/
	if( nextSecond == -1 )
	{
	    nextSecond = DVD_TIME_BASE*(long long)( (serverTime+2000000)/DVD_TIME_BASE );
	    printf( "NS=%f\n", nextSecond );
	}

	if( pause && 0 <= serverTime && serverTime <= 100000 )
	{
	    JumpToPos( 200000 ); // +0.2s ???
	    pause = false;
	    atPlayUnpause();
	    printf( "Synchro start\n" );
	}

//	printf( "P: %d NS:%f SRC: %c  CLI: %10.2f SRV: %10.2f\n", pause, nextSecond, ((TimeSync*)buff)->source, clientTime, serverTime );
// Jump to current server position
	if( pause && nextSecond != -1 && nextSecond <= serverTime-200000 ) // 1/10c
	{
	    JumpToPos( nextSecond );
	    pause=false;
	    atPlayUnpause();
	    printf( "NS=%f ST=%f\n", nextSecond, serverTime );
	}

	}
// End of jump

	//if( 0 <= serverTime && serverTime < 100000 )
	//    pause=false;
	}
	usleep( 1000 );
      }
      while( pause );
   }
   
   return 0;
}

int OMXPlayerSync::setUpConnection ()
{
   
   if ( syncType == SYNC_SERVER )
   {
      printf ("%d: Sync server startup\n", port );
      sockfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
      if (-1 == sockfd) {
         perror("0:socket");
         return 1;
      }
      
      if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &forceRebind, sizeof(int))) {
         perror("0:setsockopt");
         return 1;
      }
      int broadcastEnable=1;
      if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable))) {
         perror("0:setsockopt");
         return 1;
      }

      memset((void *) &server, 0, sizeof(struct sockaddr_in));
      server.sin_family = AF_INET;
      server.sin_port = htons( port );
      server.sin_addr.s_addr = INADDR_ANY;

      if (-1 == bind(sockfd, (const struct sockaddr *) &server,
            sizeof(struct sockaddr_in))) {
         perror("0:bind");
         return 1;
      }
      
      return 0;
   }
   else
   {
      printf ("Sync client startup\n");
      sockfd = socket(AF_INET, SOCK_DGRAM|SOCK_NONBLOCK, 0);
      if (-1 == sockfd) {
         perror("1:socket");
         return 1;
      }

      if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &forceRebind, sizeof(int))) {
         perror("0:setsockopt");
         return 1;
      }

      int broadcastEnable=1;
      if (-1 == setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable))) {
         perror("0:setsockopt");
         return 1;
      }

/*      if (0 != getaddrinfo( serverAddress.c_str(), NULL, NULL, &serverinfo )) {
         perror("getaddrinfo111");
         return 1;
      }
*/
      memset((void *) &server, 0, sizeof(struct sockaddr_in));
      server.sin_family = AF_INET;
      server.sin_port = htons( port );
      server.sin_addr.s_addr = INADDR_ANY;

      if (-1 == bind(sockfd, (const struct sockaddr *) &server,
            sizeof(struct sockaddr_in))) {
         perror("0:bind");
         return 1;
      }
      /*Copy size of sockaddr_in b/c res->ai_addr to big for this example*/
//      memcpy(&server, serverinfo->ai_addr, sizeof(struct sockaddr_in));
      server.sin_family = AF_INET;
      server.sin_port = htons( port );
      freeaddrinfo(serverinfo);
      
      return 0;
   }
}

int OMXPlayerSync::closeConnection ()
{
      close(sockfd);
      return 0;
}

/* Forks a process to run in background and                            *
 * 1) Listen for server broadcasting its play time                     *
 * 2) Listen for clients indicating that they are ready to run         *
 */
void OMXPlayerSync::start ( bool m_pause )
{
   // Pause movie, unpause later via tcp communication

   if ( syncType == SYNC_SERVER )
   {
    if( m_pause )
    {
	pause = true;
	printf( "%d: Pause\n", port );
	pauseMovie();
    }
    printf("%d: Connect\n", port);
    setUpConnection ();
    printf("%d: Sync\n", port);
    syncServer ();
    unpauseMovie();
    printf("%d: Broadcast\n", port);
    ServerBcast();
   }
   else
   { //Client
    printf("%d: Pause\n", port);
    pauseMovie ();
    pause = true;
    printf("%d: Connect\n", port);
    setUpConnection ();
    printf("%d: Sync\n", port);
    syncClient ( false );
    printf("%d: Unpause\n", port);
    unpauseMovie ();
   }
   printf("%d: Go\n", port);
   displayVerbose ();
}

/*
 * Initally we pause the movie then resume when all nodes are ready.   *
 * Note, we never call this function more than once, hence why we      *
 * don't need to include the jitter yet.                               *
 */
void OMXPlayerSync::pauseMovie ()
{
   playspeed_current = playspeed_slow_min;
   SetSpeed(playspeeds[playspeed_current]);
}

/*
 * Server waits until all nodes are ready then broadcasts a start      *
 * signal. This is meant to only be used once.                         *
 */
void OMXPlayerSync::unpauseMovie ()
{
   playspeed_current = playspeed_normal;
   SetSpeed(playspeeds[playspeed_current] + S(jitter));
}

void OMXPlayerSync::display ()
{
   if ( m_is_sync_verbose )
   {
      if ( syncType == SYNC_SERVER )
      {
         printf ( "Sync Type: SYNC_SERVER\n" );
         printf ( "Jitter: %f\n", jitter );
         printf ( "Num Nodes: %d\n", numNodes );
         printf ( "Port: %d\n", port );
         cout << "Server Time: " << serverTime << "\n";
      }
      else
      {
         printf ( "Sync Type: SYNC_CLIENT\n" );
         printf ( "Jitter: %f\n", jitter );
         printf ( "Port: %d\n", port );
         cout << "Server Address: " << serverAddress << "\n";
         cout << "Server Time: " << serverTime << "\n";
      }
   }
}

void OMXPlayerSync::displayVerbose ()
{
   if ( m_is_sync_verbose )
   {
      cout.precision (15);
      if ( syncType == SYNC_SERVER )
      {
         cout << "[" << updateTicker << "] Server Time: " << serverTime << "\n";
      }
      else
      {
         cout << "[" << updateTicker << "] Server Time: " << serverTime << " local time: " << clientTime << " [" << (serverTime-clientTime)/1000000 << "]"\
	      << " Jitter:" << jitter << " CS: " << playspeeds[playspeed_current] <<"\n";
      }
   }
}

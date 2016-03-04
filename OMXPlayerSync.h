#ifndef OMXPLAYERSYNCH_H
#define OMXPLAYERSYNCH_H

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netdb.h>
#include <string>
#include <string.h>
#include <ctime>
#include <time.h>

// Not sure what I am doing here, just included all the below in order
// to get the extern working
#include "omxplayer.h"
#include "OMXStreamInfo.h"

#include "utils/log.h"

#include "DllAvUtil.h"
#include "DllAvFormat.h"
#include "DllAvFilter.h"
#include "DllAvCodec.h"
#include "linux/RBP.h"

#include "OMXVideo.h"
#include "OMXAudioCodecOMX.h"
#include "utils/PCMRemap.h"
#include "OMXClock.h"
#include "OMXAudio.h"
#include "OMXReader.h"
#include "OMXPlayerVideo.h"
#include "OMXPlayerAudio.h"
#include "OMXPlayerSubtitles.h"
// #include "OMXControl.h"
#include "DllOMX.h"
#include "Srt.h"
// #include "KeyConfig.h"
#include "utils/Strprintf.h"
// #include "Keyboard.h"

#include <utility>

#include "version.h"

using namespace std;

#define JITTER_DELTA 0.01
#define JITTER_TOLERANCE 10000
#define CONNECT_SLEEP_PERIOD ( 1 * 1000 )
#define UPDATE_THRESHOLD 100
#define BUFFLEN 4096
#define MAX_NUM_CLIENT_SOCKETS 100
#define LOOP_UNTIL_CONNECT

typedef enum
{
   SYNC_SERVER = 1,
   SYNC_CLIENT
} SyncType;

typedef enum
{
   AHEAD = 1,
   BEHIND,
   ONTRACK
} SyncTimeState;

extern OMXClock *m_av_clock;
extern OMXPlayerVideo m_player_video;

// static void SetSpeed(int iSpeed);

class OMXPlayerSync
{
   public:
      OMXPlayerSync( SyncType _syncType, int _numNodes, int _port );
      OMXPlayerSync( SyncType _syncType, int _port, const string &_serverAddress );
      ~OMXPlayerSync();
      void setPort ( int _port );
      int setUpConnection ();
      int closeConnection ();
      int atFadingIn ();
      int atFadingOut ();
      int atPlayUnpause ();
      int syncServer ();
      int syncClient ( bool send );
      int ServerBcast();
      void update ( int src_alpha );
      void reset ();
      void updateJitter ();
      void updateSpeed ();
      float getJitter ();
      // double getServerTime ();
      // int setServerTime ( double _serverTime );
      // int setClientTime ( double _clientTime );
      void start ( bool pause );
      void pauseMovie ();
      void unpauseMovie ();
      void display ();
      void displayVerbose ();
   
   private:
      SyncType syncType;
      SyncTimeState syncTimeState;
      float jitter;
      double serverTime;
      double clientTime;
      int numNodes;
      int port;
      int alpha;
      string serverAddress;
      
      //server info
      int sockfd;
      int clientSocket [ MAX_NUM_CLIENT_SOCKETS ];
      int acceptfd;
      int forceRebind;
      struct sockaddr_in server;
      struct sockaddr_in client;
      socklen_t clientaddrlen;
      ssize_t bytesreceived;
      char buff [ BUFFLEN ];
      char * reply;
      int updateTicker; //time not reliable, use ticks instead
    
      //additional client info
      // char *hostname;
      struct addrinfo *serverinfo;
      char *question;
      bool pause;
};
#endif // #ifndef OMXPLAYERSYNCH_H

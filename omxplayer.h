#ifndef OMXPLAYER_H
#define OMXPLAYER_H

#define S(x) (int)(DVD_PLAYSPEED_NORMAL*(x))

void SetSpeed(int iSpeed);
void JumpToPos( double Position );
void exitPlayer( void );
void fadeSound( void );
extern int playspeeds[];
extern int playspeed_current;
extern const int playspeed_slow_min;
extern const int playspeed_normal;
extern bool m_is_sync_verbose;
extern double stamp;

#endif // #ifndef OMXPLAYER_H

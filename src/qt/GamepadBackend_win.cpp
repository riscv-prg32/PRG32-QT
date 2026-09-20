#include "GamepadBackend.h"
#include <QTimer>
#include <windows.h>
#include <mmsystem.h>
class WinGamepadBackend final:public GamepadBackend{
public:explicit WinGamepadBackend(QObject*p=nullptr):GamepadBackend(p){timer_.setInterval(16);connect(&timer_,&QTimer::timeout,this,&WinGamepadBackend::poll);timer_.start();poll();}
private:void poll(){JOYINFOEX j{};j.dwSize=sizeof(j);j.dwFlags=JOY_RETURNALL;MMRESULT r=joyGetPosEx(JOYSTICKID1,&j);if(r!=JOYERR_NOERROR){if(connected()){setState(false,{});publish(0);}return;}JOYCAPS caps{};QString n="Windows game controller";if(joyGetDevCaps(JOYSTICKID1,&caps,sizeof(caps))==JOYERR_NOERROR)n=QString::fromWCharArray(caps.szPname);setState(true,n);quint32 m=0;auto axis=[](DWORD v,DWORD lo,DWORD hi){double mid=(double(lo)+hi)/2.0,span=double(hi)-lo;return span?((double(v)-mid)/(span/2.0)):0.0;};double x=axis(j.dwXpos,0,65535),y=axis(j.dwYpos,0,65535);if(x<-.45)m|=1;if(x>.45)m|=2;if(y<-.45)m|=4;if(y>.45)m|=8;if(j.dwPOV!=JOY_POVCENTERED){DWORD p=j.dwPOV;if(p>=22500&&p<=31500)m|=1;if(p>=4500&&p<=13500)m|=2;if(p<=4500||p>=31500)m|=4;if(p>=13500&&p<=22500)m|=8;}if(j.dwButtons&JOY_BUTTON1)m|=16;if(j.dwButtons&JOY_BUTTON2)m|=32;if(j.dwButtons&((1u<<6)|(1u<<7)|(1u<<8)|(1u<<9)))m|=64;publish(m);}
 QTimer timer_;
};
GamepadBackend*createGamepadBackend(QObject*p){return new WinGamepadBackend(p);}

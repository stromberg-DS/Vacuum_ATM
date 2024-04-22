#ifndef _TIMER_DS_
#define _TIMER_DS_

class IoTTimer {
    unsigned int _timerStart, _timerTarget;

    public:

        void startTimer(unsigned int msec){
            _timerStart = millis();
            _timerTarget = msec;
        }        

        bool isFinished(){
            return((millis()-_timerStart) >= _timerTarget);
        }
};

#endif  //_TIMER_DS_
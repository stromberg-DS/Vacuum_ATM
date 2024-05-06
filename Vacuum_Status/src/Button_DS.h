#ifndef _BUTTON_H_
#define _BUTTON_H_

class Button{
    int _buttonPin;
    int _prevButtonState;
    int _prevRButtonState;
    bool _pullUp;

    public:

        //Constructor//
        Button(int buttonPin, bool pullUp=false){
            _buttonPin = buttonPin;
            _pullUp = pullUp;

            if(pullUp){
                pinMode(_buttonPin, INPUT_PULLUP);
            } else{
                pinMode(_buttonPin, INPUT_PULLDOWN);
            }
        }

        //////Button Functions//////

        //Checks if button is currently pressed down
        bool isPressed(){
            bool _buttonState;

            _buttonState = digitalRead(_buttonPin);
            if(_pullUp){
                _buttonState = !_buttonState;
            }
            return _buttonState;
        }

        //Checks for the first button click
        bool isClicked(){
            bool _buttonState, _clicked;

            //Read pin and invert it if pullup is set
            _buttonState = digitalRead(_buttonPin);
            if(_pullUp){
                _buttonState = !_buttonState;
            }

            if(_buttonState != _prevButtonState){
                _clicked = _buttonState;
            } else{                                     
                _clicked = false;
            }
            _prevButtonState = _buttonState;

            return _clicked;
        }

        bool isReleased(){
            bool _buttonState, _released;

            _buttonState = digitalRead(_buttonPin);
            if(_pullUp){
                _buttonState = !_buttonState;
            }

            if(_buttonState != _prevRButtonState){
                _released = _prevRButtonState;
            } else{
                _released = false;
            }
            _prevRButtonState = _buttonState;

            return _released;
        }

};

#endif // _BUTTON_H_
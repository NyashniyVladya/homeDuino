
#include <OneButton.h>
#include <math.h>
#define lightLentAnalogPin A0


class RoomController {
    private:
        unsigned short RGB[3]; // Хранение указаний о цвете (без изменений от регулятора).
        
        const int RGBPin[3] = {11, 10, 9};
        bool ringMode = false; // Режим прозвонки на 13 пине.
        bool lastRingStatus;
        
        
        bool needChangeCycle;
        unsigned short needRGB[3]; // Нужные значения, для изменения.
        unsigned short startValues[3]; // Нужные значения, для изменения.
        unsigned colorRelaxTime; // Время на автоматическую смену цветов в мс. Если 0 - отключено.
        unsigned long timeFromStart;
        unsigned long startChangeTime;
        
        
        bool colorRelaxModeIsOn(void) {
            return (colorRelaxTime > 100);
        }
        
        void setRandomColor(void) {
            for (int i = 0; i <= 2; i++) {
                needRGB[i] = random(0, 256);
            };
            equateColorArrays(startValues, RGB);
            startChangeTime = timeFromStart;
            needChangeCycle = true;
        }
        
        void equateColorArrays(unsigned short *array1, unsigned short *array2) {
            // Делает первый массив равным второму.
            for (int i = 0; i <= 2; i++) {
                array1[i] = array2[i];
            }
        }
        
        
        void lentWriteColorToArray(const String *colorAndVal) {
            // Записывает значение, принятое с COM порта в массив, для дальнейшей обработки.
            char *color = new char(((*colorAndVal).charAt(0)));
            int *colorIndex = new int(getColorIndex(color));
            delete color;
            if ((*colorIndex) >= 10) {
                delete colorIndex;
                return;
            }
            int *value = new int((((*colorAndVal).substring(1)).toInt()));
            if (*value < 0) {
                *value = 0;
            } else if (*value > 255) {
                *value = 255;
            };
            RGB[(*colorIndex)] = *value;
            delete colorIndex;
            delete value;
        }
        
        void colorWrite(const int *pinNum, const int *colorVal) {
            // Ставит нужное напряжение на контакте $pinNum. Обёртка над analogWrite().
            int *value = new int(*colorVal);
            if (*value < 0) {
                *value = 0;
            } else if (*value > 255) {
                *value = 255;
            };
            analogWrite(*pinNum, *value);
            delete value;
        }
        
    public:
    

        const unsigned short lampPins[2] = {6, 7};
        bool lampStatus[2] = {false, false};

        void roomLightOnOff(const bool lNum) {
            const short lampNum = lNum;
            // Переключает состояние (вкл./выкл.) на лампе $lampNum.
            const unsigned short pin = lampPins[lampNum];
            bool newLampStatus = !(digitalRead(pin));
            digitalWrite(pin, newLampStatus);
            lampStatus[lampNum] = newLampStatus;
            Serial.print("Lamp ");
            Serial.print((lampNum + 1));
            Serial.print(": ");
            Serial.println(newLampStatus);
            delay(50);
        }

        
        void roomLightOn(const bool lNum) {
            if (!(lampStatus[lNum])) {
                roomLightOnOff(lNum);
            }
        }
        
        void roomLightOff(const bool lNum) {
            if (lampStatus[lNum]) {
                roomLightOnOff(lNum);
            }
        }

        bool numInRange(const float *num, const float start, const float stp) {
            return ((start <= (*num)) && ((*num) < stp));
        }
        void updateTime(const unsigned long *_time) {
            timeFromStart = *_time;
        }
        
        float lentLightLevel(void) {
            /* 
            Возвращает округлённое процентное значения аналогового регулятора,
            где (.0 <= value <= 1.)
            */
            const float percentLevel = analogRead(lightLentAnalogPin) / 1023.;
            if (numInRange(&percentLevel, .0, .2)) {
                return .0;
            } else if (numInRange(&percentLevel, .2, .4)) {
                return .2;
            } else if (numInRange(&percentLevel, .4, .6)) {
                return .5;
            } else if (numInRange(&percentLevel, .6, .8)) {
                return .8;
            } else {
                return 1.;
            }
        }
        
        int getColorIndex(const char *color) {
            // Возвращает индекс цвета, в нужных массивах.
            switch (*color) {
                case 'r':
                    return 0;
                case 'g':
                    return 1;
                case 'b':
                    return 2;
                default:
                    return 10;
            }
        }
        
        void relaxColorCycleControl(void) {
            
            if (colorRelaxModeIsOn()) {
                if (!(needChangeCycle)) {
                    setRandomColor();
                };
                const double *elapsedTime = new double((timeFromStart - startChangeTime));
                if ((*elapsedTime) >= colorRelaxTime) {
                    equateColorArrays(RGB, needRGB);
                    needChangeCycle = false;
                } else {
                    short *fullDistance = new short;
                    short *distanceNow = new short;
                    const float *percentOfPath = new float(((*elapsedTime) / colorRelaxTime));
                    for (int i = 0; i <= 2; i++) {
                        *fullDistance = needRGB[i] - startValues[i];
                        *distanceNow = (*fullDistance) * (*percentOfPath);
                        RGB[i] = startValues[i] + (*distanceNow);
                    }
                    delete fullDistance;
                    delete distanceNow;
                    delete percentOfPath;
                }
                delete elapsedTime;
            }
            
            
        }
        
        
        void setColorsToLent(void) {
            /*
            Функция, работающая в цикле. 
            Переключает цвета, на хранящиеся в RGB[], с поправкой на регулятор.
            */
            relaxColorCycleControl();
            
            const float *lightPercent = new float(lentLightLevel());
            int *colorNeed = new int;
            int *pinNum = new int;
            for (int i = 0; i <= 2; i++) {
                *colorNeed = RGB[i] * (*lightPercent); // Запрашиваемый цвет, помноженный на значение регулятора.
                *pinNum = RGBPin[i];
                colorWrite(pinNum, colorNeed);
            }
            delete lightPercent;
            delete colorNeed;
            delete pinNum;
        }
        
        void switchRelaxColorsMode(void) {
            String *command = new String;
            if (colorRelaxModeIsOn()) {
                *command = "colors";
            } else {
                *command = "colors60000";
            }
            parseMessage(command);
            delete command;
        }
        
        void parseMessage(const String *inputMessage) {
            /*
            Обрабатывает команды с COM-порта.
            */
            if ((*inputMessage).startsWith("rl")) {
                // Комнатный свет.
                int *lNum = new int((((*inputMessage).substring(2)).toInt()));
                (*lNum)--;
                roomLightOnOff(*lNum);
                delete lNum;
            } else if ((*inputMessage).startsWith("lent")) {
                // Светодиодная лента.
                String *colorAndVal = new String(((*inputMessage).substring(4)));
                if ((*colorAndVal).length() <= 0) {
                    delete colorAndVal;
                    return;
                }
                lentWriteColorToArray(colorAndVal);
                delete colorAndVal;
            } else if ((*inputMessage).startsWith("ring")) {
                // Включить/Выключить прозвонку на 13 пине.
                ringMode = !(ringMode);
                lastRingStatus = false;
                Serial.print("ringmode: ");
                Serial.println(ringMode);
            } else if ((*inputMessage).startsWith("colors")) {
                colorRelaxTime = ((*inputMessage).substring(6)).toInt();
                if (!(colorRelaxModeIsOn())) {
                    for (int i = 0; i <= 2; i++) {
                        RGB[i] = 0;
                    }
                    needChangeCycle = false;
                }
                
            }
            //colorRelaxTime
        }
        
        void ringCycleFunc(void) {
            if (ringMode) {
                bool *contactNow = new bool((!(digitalRead(13))));
                if ((*contactNow) != lastRingStatus) {
                    lastRingStatus = (*contactNow);
                    Serial.print("ring: ");
                    Serial.println(*contactNow);
                };
                delete contactNow;
            };
        }
        
};


class DiskPhone {
    
    private:
        
        RoomController *_cont;
        
        unsigned long timeNow;
    
        unsigned short diskCounter = 0b1;
        unsigned short fullCode = 0b0;
    
        const unsigned short handsetPin = 2;
        const unsigned short diskPin = 8;
        const unsigned short counterPin = 3;
        
        bool handset, disk, counter;
        
        bool _counterWasPush = false;
        unsigned long pushStartTime;
        bool counterWasClicked(void) {
            if (counter) {
                // Нажатие сейчас.
                if (!(_counterWasPush)) {
                    // Зафиксировано ещё не было. Фиксируем.
                    pushStartTime = timeNow;
                    _counterWasPush = true;
                }
            } else {
                // Нажатия нет.
                if (_counterWasPush) {
                    // Но было.
                    _counterWasPush = false;
                    if ((timeNow - pushStartTime) >= 20) {
                        // По времени больше 20 мс. Не дребезг.
                        return true;
                    }
                }
            }
            return false;
        }
        
        
    public:
    
    
        DiskPhone(RoomController *c) {
            pinMode(handsetPin, INPUT);
            pinMode(diskPin, INPUT);
            pinMode(counterPin, INPUT);
            _cont = c;
        };
        
        void cycleFunc(const unsigned long *t) {
            
            timeNow = *t;
            
            handset = digitalRead(handsetPin);
            disk = digitalRead(diskPin) && handset;
            counter = (!(digitalRead(counterPin))) && disk;
            
            if (handset) {
                // Трубка снята.
                if (counterWasClicked()) {
                    diskCounter <<= 1;
                }
                if (!(disk)) {
                    // Диск отпущен.
                    if (diskCounter > 0b1) {
                        fullCode ^= (diskCounter >> 1);
                        diskCounter = 0b1;
                    }
                }
            } else {
                diskCounter = 0b1;
                _counterWasPush = false;
                if (fullCode > 0b0) {
                    (*_cont).roomLightOnOff(0);
                    fullCode = 0b0;
                }
            }
            
            
        }
    
};



const char separator = '/';
char inChar;
unsigned long _timeNow;
String message = "";

RoomController controller;
DiskPhone phone(&controller);
OneButton roomLightButton(12, true);





void setup(void) {
    
    Serial.begin(9600);
    
    // Переключает значения ламп. Для управления с одной кнопки, без ПК.
    roomLightButton.attachClick(
        (
            []() {
                if (controller.lampStatus[0]) {
                    controller.roomLightOnOff(1);
                }
                controller.roomLightOnOff(0);
            }
        )
    );
    roomLightButton.attachLongPressStop(
        ([]() {controller.switchRelaxColorsMode();})
    );
    
        
    pinMode(13, INPUT_PULLUP);

    int outputPins[] = {9, 10, 11, 6, 7};
    for (int counter = 0; counter < 5; counter++) {
        pinMode(outputPins[counter], OUTPUT);
    }
    
}


void loop(void) {
    
    _timeNow = millis();
    
    controller.updateTime(&_timeNow);
    phone.cycleFunc(&_timeNow);
    roomLightButton.tick();
    controller.ringCycleFunc();
    controller.setColorsToLent();
    
    while (Serial.available()) {
        inChar = Serial.read();
        if (inChar == separator) {
            message.trim();
            message.toLowerCase();
            if (message.length() <= 0) {
                continue;
            }
            controller.parseMessage(&message);
            message = "";
        } else {
            message += inChar;
        }
    }
}

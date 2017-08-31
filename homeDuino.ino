
#include <math.h>
#define lightLentAnalogPin A0


class Button {
    
    private:
        int buttonPinNum;
        boolean buttonWasPush = false;
        
    public:
    
        Button(int buttonPin) {
            buttonPinNum = buttonPin;
        };
        
        boolean buttonWasClicked() {
            /*
            Определяет была ли нажата кнопка, по условиям: 
            Была нажата, сейчас отпущена.
            */
            boolean *buttonNotPush = new boolean;
            for (int scanStep = 1; scanStep <= 2; scanStep++) {
                *buttonNotPush = digitalRead(buttonPinNum);
                if (*buttonNotPush) {
                    delete buttonNotPush;
                    if (buttonWasPush) { // Кнопка сейчас не нажата, но БЫЛА нажата. Фиксируем клик.
                        buttonWasPush = false;
                        return true;
                    } else { // Сейчас не нажата и нажата не была.
                        return false;
                    }
                } else {
                    if (scanStep <= 1) { // Перепроверяем, на случай дребезга.
                        delay(10);
                    } else { // Нажатие есть. Фиксируем переменную "кнопка была нажата".
                        buttonWasPush = true;
                    }
                }
            }
            delete buttonNotPush;
            return false;
        }
};

class RoomController {
    private:
        boolean lampOne = false;
        boolean lampTwo = false;
        int RGB[3]; // Хранение указаний о цвете (без изменений от регулятора).
        const int RGBPin[3] = {11, 10, 9};
        boolean ringMode = false; // Режим прозвонки на 13 пине.
        boolean lastRingStatus;
        
        void roomLightOnOff(const int lampNum) {
            // Переключает состояние (вкл./выкл.) на лампе $lampNum.
            boolean *newLampStatus = new boolean;
            if (lampNum == 1) {
                *newLampStatus = !(digitalRead(6));
                lampOne = *newLampStatus;
                digitalWrite(6, *newLampStatus);
                Serial.print("Lamp 1: ");
            } else if (lampNum == 2) {
                *newLampStatus = !(digitalRead(7));
                lampTwo = *newLampStatus;
                digitalWrite(7, *newLampStatus);
                Serial.print("Lamp 2: ");
            };
            Serial.println(*newLampStatus);
            delay(50);
            delete newLampStatus;
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
        Button roomLightButton{12};
        boolean numInRange(const float *num, const float start, const float stop) {
            return ((start <= (*num)) && ((*num) < stop));
        }
        float lentLightLevel() {
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
        
        void switchLampMode() {
            // Переключает значения ламп. Для управления с одной кнопки, без ПК.
            if (lampOne) {
                roomLightOnOff(2);
            }
            roomLightOnOff(1);
        }
        
        int getColorIndex(const char *color) {
            // Возвращает индекс цвета, в нужных массивах.
            if (*color == 'r') {
                return 0;
            } else if (*color == 'g') {
                return 1;
            } else if (*color == 'b') {
                return 2;
            } else {
                return 10;
            }
        }
        
        void setColorsToLent() {
            /*
            Функция, работающая в цикле. 
            Переключает цвета, на переданные, через COM, с поправкой на регулятор.
            */
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
        
        void parseMessage(const String *inputMessage) {
            /*
            Обрабатывает команды с COM-порта.
            */
            if ((*inputMessage).startsWith("rl")) {
                // Комнатный свет.
                int *lNum = new int((((*inputMessage).substring(2)).toInt()));
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
            }
        }
        
        void ringCycleFunc() {
            if (ringMode) {
                boolean *contactNow = new boolean((!(digitalRead(13))));
                if ((*contactNow) != lastRingStatus) {
                    lastRingStatus = (*contactNow);
                    Serial.print("ring: ");
                    Serial.println(*contactNow);
                };
                delete contactNow;
            };
        }
        
};

const char separator = '/';
char inChar;
String message = "";
RoomController controller;

void setup() {
    Serial.begin(9600);
    pinMode(12, INPUT_PULLUP);
    pinMode(13, INPUT_PULLUP);
    int outputPins[] = {9, 10, 11, 6, 7};
    for (int counter = 0; counter < 5; counter++) {
        pinMode(outputPins[counter], OUTPUT);
    }
}


void loop() {
    if (controller.roomLightButton.buttonWasClicked()) {
        controller.switchLampMode();
    };
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


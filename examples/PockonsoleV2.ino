/*********************************************************************
 Harry Pockonsole V2
 **********************************************************************
 Board: VoV1366v0.8
 Design by Steve French @ Volt-Vision
 
 Code:
 ______________________________
 DMX Connections (hardware UART @ Serial1) to TI SN75LBC184D:
 DMX1RX on pin 0
 DMX1TX on pin 1
 DMX1DE and RE (DMX_REDE) on pin 24
 _____________________________
 OLED connections (software SPI):
 OLED MOSI0 on pin 11
 OLED SCK0 on pin 13
 OLED_DC on pin 32
 OLED_RST on pin 33
 OLED_CS on pin 34
 ______________________________
 Wiper Connections (10k mini pots):
 Wiper 1: A1
 Wiper 2: A2
 Wiper 3: A3
 Wiper 4: A4
 Wiper 5: A5
 Wiper 6: A6
 Wiper 7: A7
 Wiper 8: A8
 Wiper 9: A9
 ______________________________
 4x4 Keypad:
 Rows: D9, D8, D7, D6
 Columns = D5, D4, D3, D2
 ______________________________
 Breadboard Area:
 Digital Pins: D10,D12,D25,D25,D27,D28,D29,D30 (D2-D9 used in keypad)
 Analog Pins: D31/A12,D35/A16,D36/A17,D37/A18,D38/A19,D39/A20
 3V3 Rail
 Ground Rail (future iterations will have a switch for analog isolation)
***********************************************************************/

#include <U8g2lib.h>
#include <U8x8lib.h>
#include <Arduino.h>
#include <TeensyDmx.h>
#include <Keypad.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define DMX_REDE 24


const long analogFaders = 9;            //there will always be the same number of pots (9)
const int analogFaderMap[analogFaders] = {1,2,3,4,5,6,7,8,9};     // this is to allow complex pin assignments
long dmxChannels = 18;           // intializing with a limiting to the number of values that can take up a DMX instruction

int channelMap[512];                    //The beginning of being able to map channels together to create large submasters

byte dmxVal[512];           //currently limiting to one universe, though that won't always be the case

float scalerVal;


TeensyDmx Dmx(Serial1, DMX_REDE);

//___________________________U8G2 CONSTRUCTOR (declares pinout for Teensy 3.6 with the U8g2lib.h OLED library)
U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ 13, /* data=*/ 11, /* cs=*/ 34, /* dc=*/ 32, /* reset=*/ 33);

enum pgmMode {   // _____________________________________PROGRAM MODES_____________
    FADER_MODE,
    KPD_MODE,
    KPDFADER_MODE
};

pgmMode controlMode = KPD_MODE;

/* keypad constants                  ____________________________________________________*/
const byte ROWS = 4; //four rows
const byte COLS = 4; //three columns
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'U'},
    {'4', '5', '6', 'D'},
    {'7', '8', '9', 'S'},
    {'@', '0', '-', 'E'}
};

byte rowPins[ROWS] = {9, 8, 7, 6}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {5, 4, 3, 2}; //connect to the column pinouts of the keypad

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

char channelOneString[5];           // first channel in commmand
int channelOneInt;                // storage for the array of characters into an array

int integerPlaces = 0;      // initializes the integer count at 0

char channelTwoString[5];           // second channel in commmand
int channelTwoInt;                // storage for the array of characters into an array

char intensityString[9];           // first channel in commmand
float kpdIntensityFloat;      // first intensity channel

enum kpdProgress {   // current keypad progress
    FADER_MODE,
    NO_CMD,
    DMXCH_ONE,
    DMX_MOD_ONE,
    DMXCH_TWO,
    DMX_MOD_TWO,
    DMX_INTENSITY,
    DMX_ENTER
};

kpdProgress kpdState = NO_CMD;
//___________________________

void setup() {
    Serial.begin(9600);
    
    Dmx.setMode(TeensyDmx::DMX_OUT);  // Teensy DMX Declaration of Output
    analogReadRes(16);
    analogReadAveraging(8);
    u8g2.begin();
    u8g2oledIntroPage();
    u8g2.sendBuffer();   // transfer internal memory to the display
    u8g2.clearBuffer();
    delay(1000);

}

void loop(){
    pockonsoleDMX(KPD_MODE);
    
}



/* function containing mode switching instead of putting a switch in the main loop*/
void pockonsoleDMX(pgmMode mode){
    switch (mode) {
        case FADER_MODE:
            faderToDmxVal(16, 9);
            u8g2.sendBuffer();   // transfer internal memory to the display
            u8g2.clearBuffer();
            break;
            
        case KPD_MODE:                        // Channel Assignment command
            char key = keypad.getKey();
            if (key != NO_KEY){
                kpdToCommand(key);
            }
            break;
            
        case KPDFADER_MODE:                  // Channel Assignment command
            char key = keypad.getKey();
            if (key != NO_KEY){
                kpdToCommand(key);
            }
            break;
    }
}



void kpdToCommand(char key){
    switch (key) {
            //___________________________________________________________________________________________________
        case '@':                       //  fall through switch for the '@' key with function trigger
        case '-':                       //  fall through switch for the '-' key with function trigger
        case 'E':                       //  fall through switch for the 'E' key with function trigger
        case 'S':                       //  fall through switch for the 'S' key with function trigger
        case 'D':                       //  fall through switch for the 'D' key with function trigger
        case 'U':                       //  mapping for 'U' key with function trigger
            keypadLogic(false, key);
            break;
        case '0':                       //  fall through switch for the '0' key with function trigger
        case '1':                        //  fall through switch for the '1' key with function trigger
        case '2':                       //  fall through switch for the '2' key with function trigger
        case '3':                       //  fall through switch for the '3' key with function trigger
        case '4':                       //  fall through switch for the '4' key with function trigger
        case '5':                       //  fall through switch for the '5' key with function trigger
        case '6':                       //  fall through switch for the '6' key with function trigger
        case '7':                       //  fall through switch for the '7' key with function trigger
        case '8':                       //  fall through switch for the '8' key with function trigger
        case '9':                       //  mapping for '9' key with function trigger
            keypadLogic(true, key);
            break;
    }
}




void keypadLogic(bool isAnInteger, char kpdInput){
    switch (kpdState) {
        //NO_CMD______________________________________
        case NO_CMD:
            if (isAnInteger == false){
                kpdState = NO_CMD;
                Serial.println("INVALID INPUT");
                break;
            }else {
                channelOneString[integerPlaces] = kpdInput;
                kpdState = DMXCH_ONE;
                integerPlaces++;
                Serial.println(channelOneString);
                break;
            }
        //DMXCH_ONE____________________________________
        case DMXCH_ONE:                                                     // First Channel Assignment command
            if (isAnInteger == false){                                      // is this an integer?
                if ((kpdInput == '@') && (integerPlaces > 0)){       // if it is '@' and there are more than 0 integers
                    channelOneInt = atoi (channelOneString);
                    Serial.println("Conversion to Integer: ");
                    Serial.println(channelOneInt);
                    
                    kpdState = DMX_INTENSITY;                  // move to the stage where you assign intensity
                    integerPlaces = 0;                        //bring integer count back to zero because we're moving to intensity
                    break;
                }if ((kpdInput == '-') && (integerPlaces > 0)){  // if it is '-' and there are more than 0 integers
                    channelOneInt = atoi (channelOneString);
                    Serial.println("Conversion to Integer: ");
                    Serial.println(channelOneInt);
                    kpdState = DMXCH_TWO;                      // move to the stage where you select the second channel
                    integerPlaces = 0;                     //bring integer count back to zero because we're moving to Ch02
                    break;
                }
                break;                                          // leave the switch without doing anything
            }else if (integerPlaces > 1){                           //more than 2 integer places
                channelOneString[integerPlaces] = kpdInput;                       //wrap the number
                
                kpdState = DMX_MOD_ONE;
                Serial.println(channelOneString);
                //                integerPlaces++;
                break;                                              // leave the switch
            }else {                                                 // if we aren't overflowing, do this
                channelOneString[integerPlaces] = kpdInput;   // adding the char to the array
                kpdState = DMXCH_ONE;                   // make sure we are in this mode
                Serial.println(channelOneString);
                integerPlaces++;                            // DO NOT increment integer places in order for wrap to work in the next mode?
                break;                                          // leave the switch
            }
        //DMX_MOD_ONE____________________________________
        case DMX_MOD_ONE:{
            if (isAnInteger == false){                                      // is this an integer?
                if ((kpdInput == '@') && (integerPlaces > 0)){       // if it is '@' and there are more than 0 integers
                    channelOneInt = atoi (channelOneString);
                    Serial.println("Channel: ");
                    Serial.println(channelOneInt);
                    
                    //
                    kpdState = DMX_INTENSITY;                  // move to the stage where you assign intensity
                    //
                    integerPlaces = 0;                        //bring integer count back to zero because we're moving to intensity
                    break;
                }if ((kpdInput == '-') && (integerPlaces > 0)){  // if it is '-' and there are more than 0 integers
                    channelOneInt = atoi (channelOneString);
                    Serial.println("Channel: ");
                    Serial.println(channelOneInt);
                    
                    
                    kpdState = DMXCH_TWO;                      // move to the stage where you select the second channel
                    
                    integerPlaces = 0;                     //bring integer count back to zero because we're moving to Ch02
                    break;
                }
                break;                                              // leave the switch
            }else {                                                 // if we're still getting integers
                channelOneString[integerPlaces - 2] = channelOneString[integerPlaces - 1];  //wrapping digits to next array position (overwrites the value of the last item)
                channelOneString[integerPlaces -1] = channelOneString[integerPlaces];  //wrapping digits to next array position
                channelOneString[integerPlaces] = kpdInput;   // adding the char to the array
                kpdState = DMX_MOD_ONE;                   // make sure we are in this mode
                Serial.println(channelOneString);
                //integerPlaces++;                            // DO NOT increment integer places in order for wrap to work
                break;                                          // leave the switch
            }
            //DMXCH_TWO____________________________________
        case DMXCH_TWO:                  // First Channel Assignment command
            if (isAnInteger == false){                                      // is this an integer?
                if ((kpdInput == '@') && (integerPlaces > 0)){       // if it is '@' and there are more than 0 integers
                    channelTwoInt = atoi (channelTwoString);
                    Serial.println("Conversion to Integer: ");
                    Serial.println(channelTwoInt);
                    
                    kpdState = DMX_INTENSITY;                  // move to the stage where you assign intensity
                    integerPlaces = 0;                        //bring integer count back to zero because we're moving to intensity
                    break;
                    }
                break;                                          // leave the switch without doing anything
            }else if (integerPlaces > 1){                           //more than 2 integer places
                channelTwoString[integerPlaces] = kpdInput;                       //wrap the number
                
                kpdState = DMX_MOD_TWO;
                Serial.println(channelTwoString);
                //                integerPlaces++;
                break;                                              // leave the switch
            }else {                                                 // if we aren't overflowing, do this
                channelTwoString[integerPlaces] = kpdInput;   // adding the char to the array
                kpdState = DMXCH_TWO;                   // make sure we are in this mode
                Serial.println(channelTwoString);
                integerPlaces++;                            // DO NOT increment integer places in order for wrap to work in the next mode?
                break;                                          // leave the switch
            }
            //DMX_MOD_TWO__________________________________
        case DMX_MOD_TWO:{
            //this is where '@' would customarily reside though 'and' will also work in future boards versions
            
        }
        case DMX_INTENSITY:                  // Intensity Assignment Part of the Function
            if (pgmMode == KPDFADER_MODE){
                
            }else{
                if (isAnInteger == false){                          // is this an integer?
                    if ((kpdInput == 'E') && (integerPlaces > 0)){  // if it is '@' and there are more than 0 integers
                        kpdIntensityFloat = atof (intensityString);
                        integerPlaces = 0;                     //bring integer count back to zero because we're moving to intensity
                        kpdState = DMX_ENTER;                  // move to the stage where you assign intensity
                        break;
                    }
                    break;                                              // leave the switch without doing anything
                }else if (integerPlaces > 8){                           //more than 8 integer places
                    intensityString[integerPlaces] = kpdInput;                       //wrap the number
                    kpdState = DMX_ENTER;
                    break;                                              // leave the switch
                }else {     // if it isn't a character and we aren't overflowing, do this
                    intensityString[integerPlaces] = kpdInput;   // adding the char to the array
                    kpdState = DMX_INTENSITY;                   // make sure we are in this mode
                    integerPlaces++;                            // increment integer places
                    break;                                          // leave the switch
                }
            }//DMX_ENTER____________________________________
        case DMX_ENTER:if (isAnInteger == false){                          // is this an integer?
            if ((kpdInput == 'E') && (integerPlaces > 0)){  // if it is '@' and there are more than 0 integers
                kpdIntensityFloat = atof (intensityString);
                integerPlaces = 0;                     //bring integer count back to zero because we're moving to intensity
                kpdState = DMX_ENTER;                  // move to the stage where you assign intensity
                break;
            }
            break;                                              // leave the switch without doing anything
        }else if (integerPlaces > 8){                           //more than 8 integer places
            intensityString[integerPlaces] = kpdInput;                       //wrap the number
            kpdState = DMX_ENTER;
            break;                                              // leave the switch
        }else {     // if it isn't a character and we aren't overflowing, do this
            intensityString[integerPlaces] = kpdInput;   // adding the char to the array
            kpdState = DMX_INTENSITY;                   // make sure we are in this mode
            integerPlaces++;                            // increment integer places
            break;                                          // leave the switch
        }
    }
    
}


float floatmap(float x, float in_min, float in_max, float out_min, float out_max)  {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void u8g2oledIntroPage()  {  // intro U8G2 stuff in a function instead of all of this code in the setup
    delay(100);
    u8g2.setFont(u8g2_font_helvB14_tf); // choose a font
    u8g2.drawStr(5,21,"Pockonsole");  // write something to the internal memory
    
    u8g2.setFont(u8g2_font_baby_tf); // choose a font
    u8g2.drawStr(103,9,"V 0.5");  // write something to the internal memory
    
    u8g2.setFont(u8g2_font_7x13_tf); // choose a font
    u8g2.drawStr(8,36,"by Harry Pray IV");  // write something to the internal memory
}


void faderToDmxVal(int bitRate, int scalerFader){
    u8g2.clearBuffer();
    if (bitRate == 16) {

        scalerVal = (floatmap(analogRead(analogFaderMap[(scalerFader-1)]), 1, 65536, 65025, 1) / 65025);// MasterFader__________
        
        if (scalerVal <= .01) {                  // eliminate small values to avoid flickering (I may eventually do smoothing instead)
            scalerVal = 0;
        };
        int i = 0;
        int pin = 0;
        while(i < ((analogFaders-1)*2)){    // cycle through analog faders with an algorithm to catch other cases
            int k = (i+1);
            dmxVal[k] = round(floatmap(analogRead(analogFaderMap[pin]), 1.0, 65536.0, 255.0, 1.0) * scalerVal);
            dmxVal[i] = round((floatmap(analogRead(analogFaderMap[pin]), 1.0, 65536.0, 65025.0, 1.0) * scalerVal)/dmxVal[k]);
            if (dmxVal[i] < 3) {                  // eliminate small values to avoid flickering (I may eventually do smoothing instead)
                dmxVal[i] = 0;
            };
            if (dmxVal[k] < 3) {            // eliminate small values to avoid flickering (I may eventually do smoothing instead)
                dmxVal[k] = 0;
            };
            Dmx.setDmxChannel((i+1), dmxVal[i]);
            Dmx.setDmxChannel((k+1), dmxVal[k]);
            i = (i + 2);
            pin = (pin + 1);
        }
        u8g2.setFont(u8g2_font_profont15_tf);  // choose a suitable font
        u8g2.drawStr(2,24,"A");
        u8g2.drawStr(26,24,"B");
        u8g2.drawStr(50,24,"C");
        u8g2.drawStr(72,24,"D");
        u8g2.drawStr(2,60,"E");
        u8g2.drawStr(26,60,"F");
        u8g2.drawStr(50,60,"G");
        u8g2.drawStr(72,60,"H");
        u8g2.drawStr(94,41,"All");
        
        u8g2.setFont(u8g2_font_5x8_mn);  // choose a suitable font
        u8g2.setCursor(0, 10);
        u8g2.print(dmxVal[8]);
        
        u8g2.setCursor(22, 10);
        u8g2.print(dmxVal[9]);
        
        u8g2.setCursor(44, 10);
        u8g2.print(dmxVal[10]);
        
        u8g2.setCursor(66,10);
        u8g2.print(dmxVal[11]);
        
        u8g2.setCursor(0, 47);
        u8g2.print(dmxVal[12]);
        
        u8g2.setCursor(22, 47);
        u8g2.print(dmxVal[13]);
        
        u8g2.setCursor(44, 47);
        u8g2.print(dmxVal[14]);
        
        u8g2.setCursor(66, 47);
        u8g2.print(dmxVal[15]);
        
        u8g2.setCursor(93, 30);
        u8g2.print(scalerVal*100);
        
        u8g2.drawLine(17, 0, 17, 64);
        u8g2.drawLine(39, 0, 39, 64);
        u8g2.drawLine(61, 0, 61, 64);
        u8g2.drawLine(83, 0, 83, 64);
        u8g2.drawLine(0, 30, 83, 30);
        u8g2.drawFrame(87,21,94,24);
        
    }else {
        scalerVal = (floatmap(analogRead(scalerFader), 1, 65536, 65025, 1) / 65025);   // Master Fader________________________________
        if (scalerVal <= .01) {                  // eliminate small values to avoid flickering (I may eventually do smoothing instead)
            scalerVal = 0;
        };
        
        for (int i = 0; i < (analogFaders-1); ++i) {
            dmxVal[i] = round(floatmap(analogRead(i+1), 1, 65536, 255, 1) * scalerVal);
            if (dmxVal[i] < 2) {                  // eliminate small values to avoid flickering (I may eventually do smoothing instead)
                dmxVal[i] = 0;
            };
            Dmx.setDmxChannel((i+1), dmxVal[i]);
        }
        u8g2.setFont(u8g2_font_5x8_mn);  // choose a suitable font
        u8g2.setCursor(0, 10);
        u8g2.print(dmxVal[0]);
        
        u8g2.setCursor(22, 10);
        u8g2.print(dmxVal[1]);
        
        u8g2.setCursor(44, 10);
        u8g2.print(dmxVal[2]);
        
        u8g2.setCursor(66,10);
        u8g2.print(dmxVal[3]);
        
        u8g2.setCursor(0, 47);
        u8g2.print(dmxVal[4]);
        
        u8g2.setCursor(22, 47);
        u8g2.print(dmxVal[5]);
        
        u8g2.setCursor(44, 47);
        u8g2.print(dmxVal[6]);
        
        u8g2.setCursor(66, 47);
        u8g2.print(dmxVal[7]);
        
        u8g2.setCursor(93, 30);
        u8g2.print(scalerVal*100);
        
        u8g2.setFont(u8g2_font_profont15_tf);  // choose a suitable font
        u8g2.drawStr(2,24,"1");
        u8g2.drawStr(26,24,"2");
        u8g2.drawStr(50,24,"3");
        u8g2.drawStr(72,24,"4");
        u8g2.drawStr(2,60,"5");
        u8g2.drawStr(26,60,"6");
        u8g2.drawStr(50,60,"7");
        u8g2.drawStr(72,60,"8");
        u8g2.drawStr(94,41,"All");
        
        u8g2.drawLine(17, 0, 17, 64);
        u8g2.drawLine(39, 0, 39, 64);
        u8g2.drawLine(61, 0, 61, 64);
        u8g2.drawLine(83, 0, 83, 64);
        u8g2.drawLine(0, 30, 83, 30);
        u8g2.drawFrame(87,21,94,24);
    }  
}


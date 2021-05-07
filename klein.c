/// how to use the watch - let it come on the home page of time, date, temp, AND spo2. View your SPO2 patiently.
/// After this, if When you want to calculate your ecg, press the button once to start calibrating. 
/// When you want to stop seeing the ecg, press the button again to come to home screen.

#include <TaskScheduler.h>
#include <Wire.h>

//wifi init
#include <WiFi.h>
const char* ssid     = "Tremor";
const char* password = "ElClasico@10";

//RTC init
#include "time.h"
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800;
const int   daylightOffset_sec = 0;

double I2Cwrite(int dev, int reg, int H, int L);
void printLocalTime();
//OLED init
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET    -1  // Reset pin # (or -1 if sharing reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
int x, y = 0;


// Temp init
const int TMP116_Address = 0x48;    
const int Temp_Reg = 0x00;           // Temperature register
const int Config_Reg = 0x01;         // Configuration register
const int High_Lim_Reg = 0x02;       // High limit register
const int Low_Lim_Reg = 0x03;        // Low limit register
const int EEPROM_Unlock_Reg = 0x04;  // EEPROM unlock register
const int Device_ID_Reg = 0x0F;      // Device ID register
const uint8_t highlimH = B00001101;   // High byte of high lim
const uint8_t highlimL = B10000000;   // Low byte of high lim  - High 27 C
const uint8_t lowlimH = B00001100;    // High byte of low lim
const uint8_t lowlimL = B00000000;    // Low byte of low lim   - Low 24 C


//ECG init
const int ecg_switch = 14; 
const int ANALOG_INPUT_PIN = 34;
const int MIN_ANALOG_INPUT = 1400;
const int MAX_ANALOG_INPUT = 2200;
int _circularBuffer[SCREEN_WIDTH]; //fast way to store values (rather than an ArrayList with remove calls)
int _curWriteIndex = 0; // tracks where we are in the circular buffer
int depth = 20;
int _graphHeight = 40;
int analogVal = 0;
int yPos;
volatile bool flag = 0;


//heart rate init
#include "MAX30105.h"
#include "spo2_algorithm.h"
MAX30105 particleSensor;
#define MAX_BRIGHTNESS 255

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
//Arduino Uno doesn't have enough SRAM to store 100 samples of IR led data and red led data in 32-bit format
//To solve this problem, 16-bit MSB of the sampled data will be truncated. Samples become 16-bit data.
uint16_t irBuffer[100]; //infrared LED sensor data
uint16_t redBuffer[100];  //red LED sensor data
#else
uint32_t irBuffer[100]; //infrared LED sensor data
uint32_t redBuffer[100];  //red LED sensor data
#endif

int32_t bufferLength; //data length
int32_t spo2; //SPO2 value
int8_t validSPO2; //indicator to show if the SPO2 calculation is valid
int32_t heartRate; //heart rate value
int8_t validHeartRate; //indicator to show if the heart rate calculation is valid
byte pulseLED = 11; //Must be on PWM pin
byte readLED = 13; //Blinks with each data read
byte pp = 0;
// Callback methods prototypes
void t1Callback();
void t2Callback();
void t3Callback();

//Tasks
Task t1(60000, TASK_FOREVER, &t1Callback); //time
Task t2(100, TASK_FOREVER, &t2Callback);   //temperature // t4, t2 runs indefinitely, t4 will start by measuring 1st temperature and end// and on every 2mins t2 runs
Task t3(100, TASK_FOREVER, &t3Callback);  //ecg  ---- if button pressed/ touchRead(14)

Scheduler runner;


void t1Callback() {
  for (y=2; y<=22; y++){
       for (x=5; x<120; x++){
        display.drawPixel(x, y, BLACK); 
       }
      }
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A,%B %d %Y %H:%M");
  
  //display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(8,2);
  display.println(&timeinfo, "%A, %B %d");
  display.setTextSize(1);
  display.setCursor(45,14);
  display.println(&timeinfo, "%H:%M");
  
  display.display(); 
 
  Serial.println();
}

void t2Callback() {
  for (y=24; y<=32; y++){
       for (x=33; x<96; x++){
        display.drawPixel(x, y, BLACK); 
       }
      }
  uint8_t data[2]; 
  int16_t datac;   
  Wire.beginTransmission(TMP116_Address); 
  Wire.write(Temp_Reg); 
  Wire.endTransmission(); 
  delay(10); 
  Wire.requestFrom(TMP116_Address,2); 
  if(Wire.available() <= 2){  
    data[0] = Wire.read(); 
    data[1] = Wire.read(); 
    datac = ((data[0] << 8) | data[1]); 
    display.setTextSize(1);
    display.setCursor(33,24);
    display.print("You : ");
    display.setCursor(64,24);
    display.print(String(int(datac*0.0078125)));
    display.cp437(true);
    display.write(167);  
    display.print("C"); 
    display.display();
  }
  
}

void t3Callback() {

   if((digitalRead(32) == 1)||(digitalRead(33) == 1)){
    Serial.println('Connect the leads properly');
    display.setTextSize(1);
    display.setCursor(0,32);
    display.println("Please connect the leads properly.");    
    display.display();
    for (y=32; y<=64; y++){
       for (x=0; x<64; x++){
        display.drawPixel(x, y, BLACK); 
       }
      }
    
    }

    
    else{
       int analogVal = analogRead(ANALOG_INPUT_PIN);
      Serial.println(analogVal);
      _circularBuffer[_curWriteIndex++] = analogVal;
      if(_curWriteIndex >= display.width()){
        _curWriteIndex = 0;
      }
      int xPos = 0; 
      for (int i = _curWriteIndex; i < display.width(); i++){
        int analogVal = _circularBuffer[i];
        analogVal = _circularBuffer[i];
        yPos = map(analogVal, MIN_ANALOG_INPUT, MAX_ANALOG_INPUT, depth, _graphHeight+depth);    
        display.drawPixel(xPos, yPos, WHITE);
        xPos++;
      }
      for(int i = 0; i < _curWriteIndex; i++){
        analogVal = _circularBuffer[i];
        yPos = map(analogVal, MIN_ANALOG_INPUT, MAX_ANALOG_INPUT, depth, _graphHeight+depth);
        display.drawPixel(xPos, yPos, WHITE);
        xPos++;;
      }
      
      display.display();
      //Serial.println(analogRead(34));
      //delay(10);
      //display.clearDisplay();
       for (y=20; y<=60; y++){
           for (x=0; x<1024; x++){
            display.drawPixel(x, y, BLACK); 
           }
          }
        }
      
  
  
  
}
int16_t pi = 0;
void testdrawrect(void) {
    //display.clearDisplay();
  //for(int16_t pi=0; pi<display.height()/2; pi+=2) {
    display.drawRect(pi, pi, display.width()-2*pi, display.height()-2*pi, SSD1306_WHITE);
    display.display(); // Update screen with each newly-drawn rectangle
    
}

void IRAM_ATTR isr() {
  if (!flag)//set flag to low if flag is true
    flag = 1;//set flag
}

void setup () {
  pinMode(ecg_switch, INPUT_PULLUP);
  pinMode(33, INPUT); // Setup for leads off detection LO +
  pinMode(32, INPUT); // Setup for leads off detection LO -
  attachInterrupt(14, isr, FALLING);
  
  Serial.begin(115200);

  //heart rate setup
  pinMode(pulseLED, OUTPUT);
  pinMode(readLED, OUTPUT);

  // Initialize sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println(F("MAX30105 was not found. Please check wiring/power."));
    while (1);
  }

  /*Serial.println(F("Attach sensor to finger with rubber band. Press any key to start conversion"));
  while (Serial.available() == 0) ; //wait until user presses a key
  Serial.read();*/

  byte ledBrightness = 60; //Options: 0=Off to 255=50mA
  byte sampleAverage = 4; //Options: 1, 2, 4, 8, 16, 32
  byte ledMode = 2; //Options: 1 = Red only, 2 = Red + IR, 3 = Red + IR + Green
  byte sampleRate = 100; //Options: 50, 100, 200, 400, 800, 1000, 1600, 3200
  int pulseWidth = 411; //Options: 69, 118, 215, 411
  int adcRange = 4096; //Options: 2048, 4096, 8192, 16384

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange); //Configure sensor with these settings

  //oled check 
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  { 
      Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
  }
  
  // Write to register
  I2Cwrite(TMP116_Address, High_Lim_Reg, highlimH, highlimL);
  I2Cwrite(TMP116_Address, Low_Lim_Reg, lowlimH, lowlimL);
  I2Cwrite(TMP116_Address, Config_Reg,0x02, 0x20); 

  

  display.clearDisplay();
  display.setTextColor(WHITE);
  
  display.setTextSize(2);
  display.setCursor(28,16);
  display.println("Klein");
  delay(1000);
  display.clearDisplay();
  testdrawrect();      // Draw rectangles (outlines)
  display.display();
  

  
//get time
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {}
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  printLocalTime();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  t2Callback();
  
  // start

  runner.init();
  runner.addTask(t1);
  runner.addTask(t2);
  runner.addTask(t3);
  
  
  

}


void loop () {
  runner.execute();
  if (!digitalRead(ecg_switch)) {
    flag = !flag;     
    display.clearDisplay();
    testdrawrect();
    printLocalTime();
    t2Callback();
    
  }
    if (flag) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(8,2);
    display.println("ECG:");
    t3.enable();
    t2.disable();
    t1.disable();
  }
  else {
            t3.disable();
            t2.enable();
            t1.enable();
        
            // heartRateCode
           bufferLength = 100; //buffer length of 100 stores 4 seconds of samples running at 25sps
        
          for (byte i = 0 ; i < bufferLength ; i++)
          {
            while (particleSensor.available() == false) //do we have new data?
              particleSensor.check(); //Check the sensor for new data
        
            redBuffer[i] = particleSensor.getRed();
            irBuffer[i] = particleSensor.getIR();
            particleSensor.nextSample(); //We're finished with this sample so move to next sample
        
           
          }
        
          //calculate heart rate and SpO2 after first 100 samples (first 4 seconds of samples)
          maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
        
          //Continuously taking samples from MAX30102.  Heart rate and SpO2 are calculated every 1 second
          for (pp = 0; pp<2; pp++) {
            //dumping the first 25 sets of samples in the memory and shift the last 75 sets of samples to the top
            for (byte i = 25; i < 100; i++)
            {
              redBuffer[i - 25] = redBuffer[i];
              irBuffer[i - 25] = irBuffer[i];
            }
        
            //take 25 sets of samples before calculating the heart rate.
            for (byte i = 75; i < 100; i++)
            {
              for (y=36; y<=60; y++){
               for (x=32; x<96; x++){
                display.drawPixel(x, y, BLACK); 
               }
              }
              while (particleSensor.available() == false) //do we have new data?
                particleSensor.check(); //Check the sensor for new data
        
              digitalWrite(readLED, !digitalRead(readLED)); //Blink onboard LED with every data read
        
              redBuffer[i] = particleSensor.getRed();
              irBuffer[i] = particleSensor.getIR();
              particleSensor.nextSample(); //We're finished with this sample so move to next sample
       
        
              display.setTextSize(1);
              display.setCursor(32,36);
              display.println("HR :");
              display.setTextSize(1);
              display.setCursor(66,36);
              display.println(heartRate/2, DEC);
              display.display();
        
              display.setTextSize(1);
              display.setCursor(32,48);
              display.println("SPO2 :");
              display.setTextSize(1);
              display.setCursor(66,48);
              display.println(spo2, DEC);
              display.display();
              
              
            }
            
            //After gathering 25 new samples recalculate HR and SP02
            maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
          }
          
          
  }

  
}

double I2Cwrite(int dev, int reg, int H, int L){
  Wire.beginTransmission(dev); 
  Wire.write(reg);
  Wire.write(H);
  Wire.write(L);
  Wire.endTransmission();
  delay(10);
}


void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  display.setTextSize(1);
  display.setCursor(8,2);
  display.println(&timeinfo, "%A, %B %d");
  display.setTextSize(1);
  display.setCursor(45,14);
  display.println(&timeinfo, "%H:%M");
  display.display(); 

}

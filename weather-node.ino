/* RFM69 library and code by Felix Rusu - felix@lowpowerlab.com
// Get libraries at: https://github.com/LowPowerLab/
// Make sure you adjust the settings in the configuration section below !!!
// **********************************************************************************
// Copyright Felix Rusu, LowPowerLab.com
// Library and code by Felix Rusu - felix@lowpowerlab.com
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses></http:>.
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************/

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <math.h>
#include "Adafruit_Si7021.h"
#include <RTCZero.h> 

//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NETWORKID     100  // The same on all nodes that talk to each other
#define NODEID        2    // The unique identifier of this node
#define RECEIVER      1    // The recipient of packets

//Match frequency to the hardware version of the radio on your Feather
//#define FREQUENCY     RF69_433MHZ
#define FREQUENCY     RF69_868MHZ
// #define FREQUENCY     RF69_915MHZ
#define ENCRYPT true
#define ENCRYPTKEY    "ABCDEFGHIJKLMNOP" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HCW   true // set to 'true' if you are using an RFM69HCW module

//*********************************************************************************************
#define SERIAL_BAUD   115200

/* for Feather M0 Radio */
#define RFM69_CS      8
#define RFM69_IRQ     3
#define RFM69_IRQN    3  // Pin 3 is IRQ 3!
#define RFM69_RST     4

/* RTC settings */
//////////////// Key Settings ///////////////////

/* Change these values to set the current initial time */
const byte hours = 0;
const byte minutes = 0;
const byte seconds = 0;
/* Change these values to set the current initial date */
const byte day = 1;
const byte month = 1;
const byte year = 0;

/////////////// Global Objects ////////////////////
RTCZero rtc;    // Create RTC object

#define LED           13  // onboard blinky
//#define LED           0 //use 0 on ESP8266

#define VBATPIN A7
float measuredvbat = 0;


RFM69 radio = RFM69(RFM69_CS, RFM69_IRQ, IS_RFM69HCW, RFM69_IRQN);
Adafruit_Si7021 sensor = Adafruit_Si7021();


char buffer[100]; // holds message to send


void setup() {
  // while (!Serial); // wait until serial console is open, remove if not tethered to computer. Delete this line on ESP8266
  Serial.begin(SERIAL_BAUD);

  Serial.println("Feather RFM69HCW Transmitter");
  
  // Hard Reset the RFM module
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, HIGH);
  delay(100);
  digitalWrite(RFM69_RST, LOW);
  delay(100);

  // Initialize radio
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  if (IS_RFM69HCW) {
    radio.setHighPower();    // Only for RFM69HCW & HW!
  }
  radio.setPowerLevel(31); // power output ranges from 0 (5dBm) to 31 (20dBm)
  
  radio.encrypt(ENCRYPTKEY);
  
  pinMode(LED, OUTPUT);
  Serial.print("\nTransmitting at ");
  Serial.print(FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(" MHz");

  // initialize Sensor
  sensor.begin();
  Serial.flush();
  
  // initialize RTC and go to sleep
  rtc.begin();    // Start the RTC in 24hr mode
  rtc.setTime(hours, minutes, seconds);   // Set the time
  rtc.setDate(day, month, year);    // Set the date
    ///////// Interval Timing and Sleep Code ////////////////
  rtc.setAlarmTime(0,0,10); // RTC time to wake, currently seconds only
  rtc.attachInterrupt(alarmMatch); // Attaches function to be called, currently blank
  
  
}

byte sendLen; // holds length of send buffer
float temp; // reading of the temperature sensor
float hum; // reading of the humidity sensor


void loop() {
  printTime();
  printDate();
  Blink(LED, 1000, 1);
  radio.receiveDone(); //put radio in RX mode
  temp = radio.readTemperature();
  // hum = 

  measuredvbat = analogRead(VBATPIN);
  measuredvbat *= 2;    // we divided by 2, so multiply back
  measuredvbat *= 3.3;  // Multiply by 3.3V, our reference voltage
  measuredvbat /= 1024; // convert to voltage
  // Serial.print("VBat: " ); Serial.println(measuredvbat);
  
  // String tStr = 
  // String hStr = String(hum, 2);
  // String bStr = String(measuredvbat, 2);
  Serial.print("Radio Temp: "); Serial.println(String(temp,2));
  // Serial.print("Hum: "); Serial.println(hStr);
  // Serial.print("VBat: "); Serial.println(measuredvbat);
  sprintf(buffer, "{\"node\":%i,\"t\":%.2f,\"h\":%.2f,\"bat\":%.2f,\"rt\":%.2f}", NODEID, sensor.readTemperature(), sensor.readHumidity() , measuredvbat, temp);

  sendLen = strlen(buffer);
  Serial.print("Sending "); Serial.print(sendLen); Serial.println(buffer);
  Serial.flush(); //make sure all serial data is clocked out before sleeping the MCU
 
  if (radio.sendWithRetry(RECEIVER, buffer, sendLen )) { //target node Id, message as string or byte array, message length
    Serial.println("OK");
    Blink(LED, 50, 3); //blink LED 3 times, 50ms between blinks
  }

  // rtc.setTime(0,0,0);
  rtc.setAlarmTime(0,0,10);
  // enable Alarm
  rtc.enableAlarm(rtc.MATCH_SS); // Match seconds only
  // put Radio to sleep
  radio.sleep();
  // put MCU to sleep
  Serial.println("Good night!"); Serial.flush();
  rtc.standbyMode();    // Sleep until next alarm match  

}

void alarmMatch() // Do something when interrupt called
{
  // Blink(LED,100,3);
}

void printTime() // Do something when interrupt called
{
  Serial.print(rtc.getHours()); Serial.print(":");
  Serial.print(rtc.getMinutes()); Serial.print(":");
  Serial.println(rtc.getSeconds());
  Serial.flush();
}

void printDate() // Do something when interrupt called
{
  Serial.print(rtc.getDay()); Serial.print(".");
  Serial.print(rtc.getMonth()); Serial.print(".");
  Serial.println(rtc.getYear() + 2000);
  Serial.flush();
}

void Blink(byte PIN, byte DELAY_MS, byte loops)
{
  for (byte i=0; i<loops; i++)
  {
    digitalWrite(PIN,HIGH);
    delay(DELAY_MS);
    digitalWrite(PIN,LOW);
    delay(DELAY_MS);
  }
}


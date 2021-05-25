#include "LedControl.h"
#include <Wire.h>
#include <TimeLib.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress timeServer(132, 163, 96, 1); // time-a.timefreq.bldrdoc.gov
const int timeZone = 9;
EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

//#include <DS3231.h>
/*
 Now we need a LedControl to work with.
 pin 12 is connected to the DataIn 
 pin 10 is connected to the CLK 
 pin 11 is connected to LOAD 
 */
LedControl lc=LedControl(4,2,3,1);

/* we always wait a bit between updates of the display */
unsigned long delaytime=10;
unsigned long m;

void lcdDisplay()
{
  //Serial.println("LCD Display");
  static int oldsecond;
  static long oldmillis;   
  int asecond,aminute,ahour,hlast,mlast,slast,tens,hundreds;
  bool PM, h12; 
  //second=Clock.getSecond();
  asecond = second();
  if ( asecond != oldsecond ) {
    oldmillis = millis();
    oldsecond = asecond; 
  }
  //minute=Clock.getMinute();
  aminute = minute();
 // hour=Clock.getHour(h12, PM);
 ahour = hour();
  slast = asecond % 10;
  mlast = aminute % 10;
  hlast = ahour % 10;
  asecond = (asecond - slast) / 10;
  aminute = (aminute - mlast) / 10;
  ahour = (ahour - hlast) / 10;
  tens = (millis()-oldmillis)/100;
  hundreds = ((millis()-oldmillis)/10)%10;
    lc.setDigit(0,7,ahour,false);
    lc.setDigit(0,6,hlast,true);
    lc.setDigit(0,5,aminute,false);
    lc.setDigit(0,4,mlast,true);
    lc.setDigit(0,3,asecond,false);
    lc.setDigit(0,2,slast,true);
    lc.setDigit(0,1,tens,false);
    lc.setDigit(0,0,hundreds,false);
}

void setup() {
  /*
   The MAX72XX is in power-saving mode on startup,
   we have to do a wakeup call
   */
  Wire.begin(); 
  lc.shutdown(0,false);
  /* Set the brightness to a medium values */
  lc.setIntensity(0,3);
  /* and clear the display */
  lc.clearDisplay(0);
  Serial.begin(115200);

  if (Ethernet.begin(mac) == 0) {
    // no point in carrying on, so do nothing forevermore:
    while (1) {
      Serial.println("Failed to configure Ethernet using DHCP");
      delay(10000);
    }
  }
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(Ethernet.localIP());
  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  
}
time_t prevDisplay = 0; // when the digital clock was displayed
unsigned long dtime = 10;

void loop() 
{
   if (timeStatus() != timeNotSet)
   { 
      m = millis();
      lcdDisplay();
      delay(millis()-m); //adjust
   }  
}
void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

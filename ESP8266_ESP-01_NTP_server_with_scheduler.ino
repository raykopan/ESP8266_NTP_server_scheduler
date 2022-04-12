// Rayko Panteleev
// ESP8266 ESP-01 Internet real time clock with scheduler
  
#include <ESP8266WiFi.h>
#include <WiFiUdp.h> 
#include <NTPClient.h>               // Include NTPClient library
#include <Timezone.h>    // https://github.com/JChristensen/Timezone
#include <ESP8266WebServer.h>
#include <EEPROM.h> 

ESP8266WebServer server(80); // Server port
 
const char *ssid     = "YOUR_SSID"; //Replace with your Network ID
const char *password = "your_password"; //Replace with your Password

int out = 0; // Output PIN - GPIO0 ESP-01, D3 NodeMCU, Active Low

byte second_, minute_, hour_, day_, month_, last_second = 60;
int year_, hours, minutes, startHour, startMin, stopHour, stopMin;
String minutesString, startHourString, startMinString, stopHourString, stopMinString;
int delayPeriod = 500;
boolean stopped = true;
String schedulerStatus = "OFF";

WiFiUDP ntpUDP;

// 'time.nist.gov' is used (default server) with hour offset (3600 seconds) 60 seconds (60000 milliseconds) update interval
NTPClient timeClient(ntpUDP, "time.nist.gov", 0, 60000); 

// Time Change Rules
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 3, 120};     // 120 EU Summer Time (UTC +3 Hours);  -240 (UTC -4 Hours)
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 4, 60};       // 60 EU Standard Time (UTC +2 Hours); -300 (UTC -5 Hours)
Timezone CE(CEST, CET);

void setup() {
 
  pinMode(out, OUTPUT);
  digitalWrite(out, HIGH);
    
  Serial.begin(115200);
 
  WiFi.begin(ssid, password);
 
  Serial.print("Connecting...");
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
   Serial.println("WiFi Failed!");
   WiFi.reconnect();    
  }
  
  Serial.println("connected");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true); 
  timeClient.begin();
  Serial.print("IP: ");  Serial.println(WiFi.localIP());
  
   server.on("/", handle_OnConnect);
   server.onNotFound(handle_NotFound);
   server.on("/set", handle_Set);
   server.on("/stop", handle_Stop);
  
  server.begin();
  
  //Init and read EEPROM
  eepromInit();
}
 
void loop() {
 
 ntpClock();
   for (int i = 0; i < 60; i++){ 
     server.handleClient();
     delay(delayPeriod);
     scheduler();  
   }
 }
 
 void handle_OnConnect() {
  server.send(200, "text/html", index_html()); 
 }

 void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
 }

 void handle_Set(){
  server.send(200, "text/html", started_html());
  startHourString = server.arg("startHour"); //this lets you access a query param
  startMinString = server.arg("startMin");
  stopHourString = server.arg("stopHour");
  stopMinString = server.arg("stopMin");
  
  startHour = startHourString.toInt();
  if(startHour < 0 || startHour > 23){
    startHour = 0;
    startHourString = "00";
  }  
  startMin = startMinString.toInt();
  if(startMin < 0 || startMin > 59){
    startMin = 0;
    startMinString = "00";
  }  
  stopHour = stopHourString.toInt();
  if(stopHour < 0 || stopHour > 23){
    stopHour = 0;
    stopHourString = "00";
  }  
  stopMin = stopMinString.toInt();  
  if(stopMin < 0 || stopMin > 59){
    stopMin = 0;
    stopMinString = "00";
  }  
  
  Serial.println("Scheduler is started!");
  Serial.print("Start Time ");
  Serial.print(startHour);
  Serial.print(":");
  Serial.println(startMinString);
  Serial.print("Stop Time ");
  Serial.print(stopHour);
  Serial.print(":");
  Serial.println(stopMinString);
  stopped = false;
  schedulerStatus = "ON";
  
  //Write data into EEPROM
  EEPROM.write(0, startHour);
  EEPROM.write(1, stopHour);
  EEPROM.write(2, startMin);
  EEPROM.write(3, stopMin);
  EEPROM.write(4, stopped);
  EEPROM.commit();
}

void handle_Stop(){
  server.send(200, "text/html", stopped_html());
  Serial.println("Scheduler is stopped!");
  digitalWrite(out, HIGH);
  stopped = true;
  schedulerStatus = "OFF";
}

void ntpClock(){
  
  timeClient.update();
  unsigned long unix_epoch = CE.toLocal(timeClient.getEpochTime()); // timeClient.getEpochTime() Get Unix epoch time from the NTP server // CE.toLocal - Timezone + daylight saving correction from Timezone.h
 
  second_ = second(unix_epoch);
  if (last_second != second_) { 
      
    minute_ = minute(unix_epoch);
    hour_   = hour(unix_epoch);
    day_    = day(unix_epoch);
    month_  = month(unix_epoch);
    year_   = year(unix_epoch);
  
    hours = hour_;
    minutes = minute_; 
    last_second = second_;               
  } 
 
  // Prints time on serial monitor
    Serial.println("The time is:");
    Serial.print(hours);
    Serial.print(":");
   if (minutes < 10) {
     minutesString = "0"+(String)minutes;        
   }
   else minutesString = (String)minutes;
    Serial.println(minutesString);
 }
 
 String index_html(){
  String ptr;  
 ptr += "<!DOCTYPE html> <html>\n";
 ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
 ptr += "<meta http-equiv=\"refresh\" content=\"30\">\n";
 ptr += "<title>ESP8266</title>\n";
 ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
 ptr += "body{margin-top: 50px;} h1 {color: #000000;margin: 50px auto 30px;}\n";
 ptr += "p {font-size: 24px;color: #000000;margin-bottom: 10px;}\n";
 ptr += "</style>\n";
 ptr += "</head><body>\n"; 
 ptr += "<h1>ESP8266 NodeMCU</h1>\n";
 ptr += "<p>Server time is ";
 ptr += hours;
 ptr += ":";
 ptr += minutesString;
 ptr += " h";
 ptr += "</p>";
 ptr += "<p>Scheduler is ";
 ptr += schedulerStatus;
 ptr += "</p>"; 
 ptr += "<h2>Scheduler Settings</h2>\n";
 ptr += "<form  action=\"/set\" method=\"post\">\n";
 ptr += "<label for=\"aname\">Start time </label><input type=\"text\" name=\"startHour\" value=";
 ptr += startHourString;
 ptr += " maxlength=\"2\" size=\"1\"><label for=\"bname\">:</label><input type=\"text\" name=\"startMin\" value=";
 ptr += startMinString;
 ptr += " maxlength=\"2\" size=\"1\"><br><br>\n";
 ptr += "<label for=\"cname\">Stop time </label><input type=\"text\" name=\"stopHour\" value=";
 ptr += stopHourString;
 ptr += " maxlength=\"2\" size=\"1\"><label for=\"dname\">:</label><input type=\"text\" name=\"stopMin\" value=";
 ptr += stopMinString;
 ptr += " maxlength=\"2\" size=\"1\"><br><br>\n";
 ptr += "<input type=\"submit\" value=\"Start\"></form><br><a href=\"/stop\"><button>Stop</button></a>\n";         
 ptr += "</body></html>\n";
 return ptr;
}

String started_html() {
 String ptr = "<!DOCTYPE html> <html>\n";
 ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
 ptr += "<meta http-equiv=\"refresh\" content=\"2; URL='/'\">\n";
 ptr += "<title>ESP8266</title>\n";
 ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
 ptr += "body{margin-top: 50px;} h1 {color: #000000;margin: 50px auto 30px;}\n";
 ptr += "p {font-size: 24px;color: #000000;margin-bottom: 10px;}\n";
 ptr += "</style>\n";
 ptr += "</head><body>\n"; 
 ptr += "<h1>ESP8266 NodeMCU</h1>\n";
 ptr += "<p>Scheduler is started!</p>";
 ptr += "</body></html>\n";
 return ptr;
}

String stopped_html() {
 String ptr = "<!DOCTYPE html> <html>\n";
 ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
 ptr += "<meta http-equiv=\"refresh\" content=\"2; URL='/'\">\n";
 ptr += "<title>ESP8266</title>\n";
 ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
 ptr += "body{margin-top: 50px;} h1 {color: #000000;margin: 50px auto 30px;}\n";
 ptr += "p {font-size: 24px;color: #000000;margin-bottom: 10px;}\n";
 ptr += "</style>\n";
 ptr += "</head><body>\n"; 
 ptr += "<h1>ESP8266 NodeMCU</h1>\n";
 ptr += "<p>Scheduler is stopped!</p>";
 ptr += "</body></html>\n";
 return ptr;
}

void scheduler(){ // Start or stop the switch automatically
  if(!stopped){
    if (hours * 60 + minutes >= startHour * 60 + startMin && hours * 60 + minutes < stopHour * 60 + stopMin){ 
          digitalWrite(out, LOW); // Start, Active LOW 
    } else if (startHour == stopHour && startMin == stopMin){
          digitalWrite(out, LOW);   
    } else{ 
          digitalWrite(out, HIGH); // Stop          
          }        
  }
}

void eepromInit(){
  EEPROM.begin(12);
  
  startHour = EEPROM.read(0);  
  stopHour = EEPROM.read(1); 
  startMin = EEPROM.read(2);  
  stopMin = EEPROM.read(3);  
  stopped = EEPROM.read(4);
  
  if(startHour>23){
    
  startHourString = "01";
  startMinString = "00";
  stopHourString = "23";
  stopMinString = "00";
  } 
     
  else {
        
  startHourString = (String)startHour;
  if(startHour < 10){
    startHourString = 0+(String)startHour;
   }
  startMinString =  (String)startMin; 
   if(startMin < 10){
    startMinString = 0+(String)startMin;
   }
  stopHourString = (String)stopHour;
  if(stopHour < 10){
    stopHourString = 0+(String)stopHour;
   }
  stopMinString = (String)stopMin; 
    if(stopMin < 10){
    stopMinString = 0+(String)stopMin; 
    }
  if(stopped){
  schedulerStatus = "OFF";
  } else {
  schedulerStatus = "ON";   
  }
 }
}

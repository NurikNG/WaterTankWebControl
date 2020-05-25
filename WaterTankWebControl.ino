#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <WiFiUdp.h>
#include <Ticker.h> 

#ifndef STASSID
#define STASSID "Wifi"
#define STAPSK  "qweqweqwe"
#endif

#define MAGIC_COMMAND 0x15
#define MAGIC_COMMAND2 0x60
#define MAGIC_COMMAND3 0x62
#define READ_SIZE 4

int addr = 15;

byte data[32];

String HTMLpage1 = "";
String HTMLpage2 = "";
String HTMLpageValue1 = "";
String HTMLpageValue2 = "";
String HTMLpageValue3 = "";
String HTMLpageValue4 = "";
String HTMLpageValue5 = "";
String HTMLpageValue6 = "";
String HTMLpageValue7= "";
String HTMLpageValue8= "";
String HTMLpageValue9= "";
String HTMLpageValue10 = "";
String LOGPAGE = "";
String Script = "";

byte volume = 0;
byte valve = 0;
byte floatSensor = 0;
byte buttonStatus = 0;

unsigned int startHour;
unsigned int minutesOpen;
unsigned int emptyMinutes = 2;
unsigned int epmtyCounter = 0;
unsigned int waterCounter;
bool needCheckEmptyWater = false;
byte needCheckWater = 0;
byte oldVolume = 0;
bool needStopWater = false;
bool showFloat = false;

byte needStartWater = 0;

Ticker updateTimer;

#define LOG_SIZE 150
String logTime[LOG_SIZE];
byte currentLog = 0;

bool manual = false;

const char* ssid = STASSID;
const char* password = STAPSK;

WiFiUDP ntpUDP;
  
ESP8266WebServer server(80);

NTPClient timeClient(ntpUDP, "pool.ntp.org", 6*3600 , 12*3600*1000);

byte needUpdateTime = 0;
byte needUpdateSerial = 0;
int currentDay = 0;

byte needToReset = 0;

unsigned int currentMinutes = 0;

void updateHandler2Sec(){
  needUpdateSerial = 1;
}

void updateHandler(){
  needUpdateTime = 1;
  if (needCheckEmptyWater){
    if (epmtyCounter-- == 1){
      if (oldVolume == volume) {
        addLogRecord("Остановка набора по потоку воды.");
        needStopWater = true;
      }
      needCheckEmptyWater = false;
    }
  }
  if (needCheckWater == 1){
    if (waterCounter-- == 1){
      if (valve == 1){
        addLogRecord("Остановка набора по времени.");
        needStopWater = true;
      }
      needCheckWater = 0;
    }
  }
}

void setup(void) {
  LOGPAGE +="<head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"stylesheet\" href=\"https://www.w3schools.com/w3css/4/w3pro.css\"><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">";
  HTMLpage1 += "<head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"stylesheet\" href=\"https://www.w3schools.com/w3css/4/w3pro.css\"><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"> <style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head><title>Бочка воды</title></head><div class=\"w3-container w3-card\"><h3>Объем <span id=\"volume\">";
  HTMLpage2 += "</span>%</p></h3><h4><span id=\"valve\"></h4><span id=\"time\"></span><br><a href=\"/set\">Настройки</a>   <a href=\"/log\">Log</a>";
  Script = "<script>setInterval(function() { getData();}, 1000); function getData() { var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) {document.getElementById(\"volume\").innerHTML = this.responseText;  }  };  xhttp.open(\"GET\", \"readVolume\", true);  xhttp.send();} setInterval(function() { getTime();}, 1000); function getTime() { var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) {document.getElementById(\"time\").innerHTML = this.responseText;  }  };  xhttp.open(\"GET\", \"readTime\", true);  xhttp.send();}setInterval(function() { getDataValve();}, 1000); function getDataValve() { var xhttp = new XMLHttpRequest(); xhttp.onreadystatechange = function() { if (this.readyState == 4 && this.status == 200) {document.getElementById(\"valve\").innerHTML = this.responseText;  }  };  xhttp.open(\"GET\", \"readValve\", true);  xhttp.send();}</script>";
  HTMLpageValue1 += "<head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"stylesheet\" href=\"https://www.w3schools.com/w3css/4/w3pro.css\"><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"> <style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head><title>Бочка воды</title></head><div class=\"w3-container w3-card\"><h3>Настройки</h3><form action=\"/setSettings\" method=\"post\"><b>Время запуска набора</b><input name=\"startHour\" id=\"t\" value=\"";
  HTMLpageValue2 += "\" size=\"2\"><br><b>Максимальное время набора</b><input name=\"minutesOpen\" id=\"t\" value=\"";
  HTMLpageValue5 += "\" size=\"2\"><div><button>Сохранить</button></div></div><br><a href=\"/reset\">Перезагрузить</a>";
  HTMLpageValue6 += "<br><a href=\"/setManual\">Включить ручное управление</a>";
  HTMLpageValue7 += "<br><a href=\"/resetManual\">Включить работу по расписанию</a>";
  HTMLpageValue8 += "<br><a href=\"/startWaterManual\">Запустить набор воды</a>";
  HTMLpageValue3 += "<br><a href=\"/stopWaterManual\">Остановить набор воды</a>";
  HTMLpageValue9 += "<head> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><link rel=\"stylesheet\" href=\"https://www.w3schools.com/w3css/4/w3pro.css\"><meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"> <style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style></head><title>Бочка воды</title></head><div class=\"w3-container w3-card\"><h3>Настройки</h3><form action=\"/setManualFormTemp\" method=\"post\"><b>Температура</b><input name=\"temp\" id=\"t\" value=\"";\
  HTMLpageValue10 += "\" size=\"2\"><div><button>Установить</button></div>";
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

   EEPROM.begin(512);
   manual = EEPROM.read(addr);
   startHour = EEPROM.read(addr+1);
   minutesOpen = EEPROM.read(addr+2);
   if (manual > 1)
      manual = 0;
   if (startHour == 0xff)
      startHour = 2;
   if (minutesOpen == 0xff)
      minutesOpen = 45;
  if (MDNS.begin("esp8266")) { }
  
  server.on("/", [](){
      server.send(200, "text/html", HTMLpage1+String(volume)+HTMLpage2+(valve == 0 ? HTMLpageValue8 : HTMLpageValue3)+Script);
  });

  server.on("/setSettings", HTTP_POST, handleSettings);
  server.on("/set", set);
  server.on("/readVolume", handleReadVolume);
  server.on("/readValve", handleReadValve);
  server.on("/readTime", handleReadTime);
  server.on("/reset", handleReset);
  server.on("/log", getLog);
  server.on("/setManual", setManual);
  server.on("/resetManual", resetManual);
  server.on("/startWaterManual", startWaterManual);
  server.on("/stopWaterManual", stopWaterManual);
  
 
  server.begin();
  needUpdateTime = 1;
  updateTimer.attach(30, updateHandler); 
  updateTimer.attach(2, updateHandler2Sec); 
  timeClient.begin();
  currentDay = -1;
  currentLog = 0;
}


void getLog(){
  String output = LOGPAGE;
  for (byte i = 0; i < currentLog; i++){
    output+=logTime[i]+"<br>";
  }
  server.send(200, "text/html", output);
}

void addLogRecord(String rec){
  if (currentLog == LOG_SIZE){
    for (byte i = 0 ; i < LOG_SIZE-1; i++)
      logTime[i] = logTime[i+1];
    currentLog = LOG_SIZE-1;
  }
  logTime[currentLog] = String(timeClient.getDay())+" "+timeClient.getFormattedTime()+" - "+rec+" \\"+String(volume);
  currentLog++;
}

void setWater(byte on){
  Serial.write(MAGIC_COMMAND2);
  Serial.write(on);
  if (on == 1) {
    addLogRecord("Включили набор");
    startCheckWater();
  } else
    addLogRecord("Выключили набор");
}

void serialUpdate(){
    Serial.write(MAGIC_COMMAND);
    Serial.write(1);
    Serial.flush();
}

void serialWorker(){
    byte numBytes = Serial.available();
    if (numBytes >= 2) {
      byte i;
      for (i = 0; i < numBytes; i++) {
          data[i] = Serial.read();
      }
      if (i == READ_SIZE) {
        parceData();
      }
    }
}

void loop(void) {
  server.handleClient();
  MDNS.update();
  
  if (needUpdateSerial){
    needUpdateSerial = 0;
    serialUpdate();
    serialWorker();
  }
  
  if (needUpdateTime){
     if (needToReset){
      needToReset = 0;
      ESP.reset();
    }

    //Serial.println("Update!");
    timeClient.update();

    if (timeClient.getEpochTime() < 1573295213){
      timeClient.forceUpdate();
      //addLogRecord("Ошибка времени");
      return;
    }

    if (currentDay == -1) {
      currentDay = timeClient.getDay();
      addLogRecord("Запуск");
    }

  if ((currentDay != timeClient.getDay()) && (timeClient.getHours() == 0)){
      currentDay = timeClient.getDay();
      currentMinutes = 0;
    }
      
    if (currentMinutes == 0)
      currentMinutes = timeClient.getHours()*60+timeClient.getMinutes();
     needUpdateTime = 0;
           
    currentMinutes = timeClient.getHours()*60+timeClient.getMinutes();

   if (manual == 0){ 
     if (needStartWater == 0)  
      if ((timeClient.getHours() == startHour) && (timeClient.getMinutes() == 0) && (volume < 80)) {
        needStartWater = 1;
        return;
      }

      if (needStartWater == 1)
        if ((timeClient.getHours() == startHour) && (timeClient.getMinutes() == 1)) {
          needStartWater = 0;
          setWater(1);
          return;
        }
    }
    
  } 

  if (needStopWater){
    setWater(0);
    needStopWater = false;
  }
}

void parceData(){
  volume = data[0];
  valve = data[1];
  floatSensor = data[2];
  buttonStatus = data[3];
    
    
  if (needCheckWater > 1)
    needCheckWater--;

  if (!showFloat)
    if (floatSensor == 1) {
        showFloat = true;
        addLogRecord("Сработал поплавок!");
    }
  if (showFloat)
    if (floatSensor == 0)
        showFloat = false;
  if (needCheckWater == 1){
    if (valve == 0){
      addLogRecord("Набор окончен.");
      needCheckWater = 0;
    }
  }
  
  if (buttonStatus == 1){
    if (needCheckWater == 0) {
      addLogRecord("Запустили набор по кнопке!");
      startCheckWater();
    }
    Serial.write(MAGIC_COMMAND3);
    Serial.write(1);
  }
}

void startCheckWater(){
  epmtyCounter = emptyMinutes*2;
  waterCounter = minutesOpen*2;
  needCheckEmptyWater = true;
  needCheckWater = 3;
  oldVolume = volume;
}

void handleReadVolume() {
 server.send(200, "text/plane", String(volume)); 
}
void handleReadValve() {
 server.send(200, "text/plane", valve ? String("Кран открыт"):String("Кран закрыт")); 
}

void handleReadTime() {
  String hours = String(timeClient.getHours());
  if (timeClient.getHours() < 10)
    hours = "0"+hours;
   String minutes = String(timeClient.getMinutes());
   if (timeClient.getMinutes() < 10)
    minutes = "0"+minutes;

  String output;
  if (needToReset){
    output = "Ожидайте перезагрузки";
  } else {
    if (manual == 0) {
      output = "Время "+hours+":"+minutes+"("+String(currentDay)+"/"+String(currentMinutes)+"), в "+String(startHour)+":00"+(timeClient.getHours()>=startHour ? " завтра":"");
    } else
       output = "Ручное управление";
  }
 server.send(200, "text/plane", output); 
}

void set(){
  server.send(200, "text/html", HTMLpageValue1+String(startHour)+
          HTMLpageValue2+String(minutesOpen)+HTMLpageValue5 
          + (manual == 0 ? HTMLpageValue6 : HTMLpageValue7));
}

void handleReset(){
  server.sendHeader("Location", "/",true);
  server.send(302, "text/plane",""); 
  needToReset = 1;
}

void setManual(){
  server.sendHeader("Location", "/",true);
  server.send(302, "text/plane",""); 
  manual = 1;
  EEPROM.write(addr, manual);
  EEPROM.commit();
  addLogRecord("Включили ручное управление");
}

void resetManual(){
  server.sendHeader("Location", "/",true);
  server.send(302, "text/plane",""); 
  manual = 0;
  EEPROM.write(addr, manual);
  EEPROM.commit();
  addLogRecord("Выключили ручное управление");
}

void handleSettings(){
  if (server.hasArg("startHour")){
    startHour = server.arg("startHour").toInt();server.sendHeader("Location", "/",true);
  server.send(302, "text/plane",""); 
  }
  if (server.hasArg("minutesOpen")){
    minutesOpen = server.arg("minutesOpen").toInt();
  }
  EEPROM.write(addr+1, startHour);
  EEPROM.write(addr+2, minutesOpen);
  EEPROM.commit();
  addLogRecord("Настройки "+String(startHour)+" "+String(minutesOpen));
  server.sendHeader("Location", "/",true); 
  server.send(302, "text/plane",""); 
}

void startWaterManual(){
  addLogRecord("Запустили набор вручную"); 
  setWater(1);
  server.sendHeader("Location", "/",true);
  server.send(302, "text/plane",""); 
}

void stopWaterManual(){
  addLogRecord("Остановили набор вручную"); 
  setWater(0);
  server.sendHeader("Location", "/",true);
  server.send(302, "text/plane",""); 
}

//=========================БАЗОВІ НАЛАШТУВАННЯ=================

//РОЛЬ=експериментальний модуль
#define S_version 2.13
#define comp_date __DATE__
#define eeprom_size 1024
#define ONE_WIRE_BUS_PIN 4
#define led_in_pin 2
#define sensor_in_pin 16
#define relay_in_pin 5
#define battery_in_pin A0
#define zoomer_pin 15
#define wifi_off_pin 14
#define ap_mode_pin 12
#define interrupt_pin 13

//=========================ПІДКЛЮЧЕННЯ БІБЛІОТЕК===============

//підключення бібліотек
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>                          
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

//загальні для всіх підключення  
ESP8266WebServer Wserver(80);
WiFiClient gclient;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ua.pool.ntp.org", 7200, 60000);
OneWire oneWire(ONE_WIRE_BUS_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress *sensorsUnique;

//=====================ОПИС КЛАСІВ=============================

//клас доступу до вбудованої памяті
class EEPROM_READ_WRITE{
  protected:
  int Adress,Size,startAdress = 0;
  public:
  EEPROM_READ_WRITE(int adr, int sz = 128, int startAdr = 0) {
    Adress=adr; Size=sz; startAdress=startAdr;
    }
  void SET_ADRESS(int Adr){
    Adress=Adr;
    }
  int EEPROM_READ_INT(){
    int ret = -1;
    EEPROM.begin(Size);   
    EEPROM.get(Adress+startAdress,ret);
    EEPROM.end();
    return ret;
    }
  char EEPROM_READ_CHAR(){
    char ret = -1;
    EEPROM.begin(Size);   
    EEPROM.get(Adress+startAdress,ret);
    EEPROM.end();
    return ret;
    }    
  bool EEPROM_READ_BOOL(){
    bool ret = false;
    EEPROM.begin(Size);   
    EEPROM.get(Adress+startAdress,ret);
    EEPROM.end();
    return ret;
    }    
  void EEPROM_WRITE_SV(auto ret){
    EEPROM.begin(Size);   
    EEPROM.put(Adress+startAdress,ret); 
    EEPROM.end();
    } 
  void EEPROM_WRITE_STRING (String Str) {
    byte lng=Str.length();
    int cIndex=Adress+startAdress;
    EEPROM.begin(Size);
    if (lng>128)  lng=128;
    EEPROM.write(cIndex,lng); 
    unsigned char* buf = new unsigned char[lng+2];
    Str.getBytes(buf,lng+1);
    cIndex++;
    for(byte i=0; i<lng; i++) {EEPROM.write(cIndex+i, buf[i]); delay(10);}
    EEPROM.end();
    } 
  char *EEPROM_READ_STRING () {
    int cIndex=Adress+startAdress;
    EEPROM.begin(Size);
    byte lng = EEPROM.read(cIndex);
    char* buf = new char[lng+2];
    cIndex++;
    for(byte i=0; i<lng; i++) buf[i] = char(EEPROM.read(cIndex+i));
    buf[lng] = '\x0';
    EEPROM.end();
    return buf;
    }  
};

//підключення класу керування пінами
template <class S>
class device_one_pin{
  protected:
  S pinIn;
  char pinMod; 
  volatile int  digitalStatus, analogStatus, delayStatus = 0;
  volatile bool enabled, digWrite, anWrite ,isAnalog = false;
  public:
  device_one_pin (S pin, char pMod = 'O', bool an = false) {
    pinIn=pin; pinMod=pMod; isAnalog=an;
    }
  bool enable(){
    if (isAnalog==false) {
    if (pinMod=='I') {pinMode(pinIn,INPUT); enabled=true;}
    if (pinMod=='P') {pinMode(pinIn,INPUT_PULLUP); enabled=true;}
    if (pinMod=='O') {pinMode(pinIn,OUTPUT); enabled=true;}
    } else {
      if (pinMod=='I') {pinMode(A0,INPUT); enabled=true;}
      if (pinMod=='P') {pinMode(A0,INPUT_PULLUP); enabled=true;}
      if (pinMod=='O') {pinMode(A0,OUTPUT); enabled=true;}      
      } 
    return enabled;
    }    
  bool eStatus() {
    return enabled;   
    }
  int dStatus(){
    return digitalStatus;
    }
  int aStatus(){
    analogStatus=analogRead(A0);
    return analogStatus;
    }
  void SetDelay(bool state = false, int del = 100){
    digWrite=state;
    if (del>delayStatus) {delayStatus=del;}
    }  
  void DelayTick(){
    if (delayStatus>=0){
      delayStatus=delayStatus-1;
      digitalWrite(pinIn,digWrite);
      }      
    if (delayStatus==0){
      digWrite=!digWrite; 
      digitalWrite(pinIn,digWrite);
      }    
    }  
  void ReadPin() {
    digitalStatus=digitalRead(pinIn);
    analogStatus=analogRead(pinIn);
    }  
  void dWrite0(){  
    if (enabled==true){
      digWrite=0;
      digitalWrite(pinIn,digWrite);}
    }  
  void dWrite1(){ 
    if (enabled==true){
      digWrite=1;
      digitalWrite(pinIn,digWrite);}
    }  
  void dWrite(int bin){ 
    if (enabled==true){
      digWrite=bin;
      digitalWrite(pinIn,digWrite);}
    }
  void aWrite(int bin){  
    if (enabled==true){
      anWrite=bin;
      digitalWrite(pinIn,anWrite);}
    }
  void dWriteReverse(){   
    if (enabled==true){
      digWrite=!digWrite;
      digitalWrite(pinIn,digWrite);}
    } 
  void buzzie(int hz,int td=-1){
    if (td<1) {tone(pinIn,hz);} else {tone(pinIn,hz,td);}  
    }
  void dWriteReverseAndDelay(int del){   
    if (enabled==true){
      digWrite=!digWrite;
      digitalWrite(pinIn,digWrite);
      delay(del); 
      }
    }     
  double backVolts (double nVolts = 3.3){
    double vStatus = 0;
    analogStatus=analogRead(A0);
    if (enabled==true){vStatus=(nVolts/1024)*analogStatus;}
    return vStatus;
    }     
  };

//клас створення таймерів
template <class A>
class tTimer{
  protected:
  A (*fnc)() = NULL;
  unsigned long timer_interval, timer_now = 0;
  bool enabled = false;
  public:
  tTimer(unsigned long interval, bool enable = true, A (*cFcn)() = NULL){
    timer_interval=interval;
    enabled=enable;
    fnc=cFcn;
  }
  void Tick(){
    if ((micros() - timer_now > timer_interval) and (enabled==true)) { 
    timer_now = micros(); 
    fnc();   
    }
  }
  bool tEnabled(){
    return enabled;
  }
  void isEnable(bool en){
    enabled=en;
    }
  void setInterval(unsigned long i){
    timer_interval=i;
    }  
  }; 

//=====================ОБЯВЛЕННЯ ФУНКЦІЙ============================

void time_flow_0();
void time_flow_1();
void time_flow_2();
void time_flow_3();
void time_flow_4();
void setup();
void loop();
int sendmesage(String mes, String url_key, bool std_message = false);
bool connect_to_wifi(String sid,String pid,int rep);

//=====================ОБЯВЛЕННЯ ОБЕКТІВ============================

//0,8,16,24,32,40,48,56,64...*8/*64
EEPROM_READ_WRITE eEPROM_max_rt(0,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_sec_enable(8,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_pass(16,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_ssid(80,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_web_url(144,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_dev_id(272,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_pass_ap(336,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_ssid_ap(400,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_sysok(408,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_alert(416,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_fire(424,eeprom_size,0);
EEPROM_READ_WRITE eEPROM_crTemp(432,eeprom_size,0);

EEPROM_READ_WRITE ep_saved_pass(440,eeprom_size,0);
EEPROM_READ_WRITE ep_relay_time_on_HR(472,eeprom_size,0);
EEPROM_READ_WRITE ep_relay_time_on_MINS(476,eeprom_size,0);
EEPROM_READ_WRITE ep_relay_time_off_HR(480,eeprom_size,0);
EEPROM_READ_WRITE ep_relay_time_off_MINS(484,eeprom_size,0);

EEPROM_READ_WRITE ep_caller_count(500,eeprom_size,0);
EEPROM_READ_WRITE ep_caller_1(504,eeprom_size,0);
EEPROM_READ_WRITE ep_caller_2(568,eeprom_size,0);
EEPROM_READ_WRITE ep_caller_3(632,eeprom_size,0);
EEPROM_READ_WRITE ep_caller_4(696,eeprom_size,0);

tTimer timer0(1,true,time_flow_0);
tTimer timer1(1*1000,true,time_flow_1);
tTimer timer2(1000*1000,true,time_flow_2); 
tTimer timer3(1000*1000,true,time_flow_3); 
device_one_pin led_in(led_in_pin,'O');
device_one_pin relay_in(relay_in_pin,'O');
device_one_pin zoomer(zoomer_pin,'O');
device_one_pin battery(battery_in_pin,'I',true); //ESP8266 A0 PIN
device_one_pin move_sensor(sensor_in_pin,'I');
device_one_pin wifi_enable(wifi_off_pin,'P');
device_one_pin ap_mode(ap_mode_pin,'P');

//=====================ГЛОБАЛЬНІ ЗМІННІ=============================
unsigned long system_unix_type = 0;
int max_relay_time,countSensors,alert,fireMsg,sysOk,INTERNET,session_timeout,c_t_sensors = 0;
volatile int relay_on_timer = 0;
String relay_setting="AUTO"; //on,auto,off
int last_power_state=0;
bool secure_status,connected_to_wifi,time_updated,zoomer_enabled,ap_test,ap_tested,soft_ap,p_sensor = false; 
String ssid_name,ssid_pass,ssid_name_ap,ssid_pass_ap,web_url,dev_id,web_message_out,temp_string,web_page_answer,text_time_line,tsn,tsp,unx_text_time_line = "";
String new_pass,user_pass,saved_pass="";
int relay_time_on_HR,relay_time_on_MINS,relay_time_off_HR,relay_time_off_MINS,ligth_up=0;
String page="home";   //home,main_settings
int wrong_password_inputs=10;
int sysOk_timeout=3600*8; 
int alert_timeout=60*10;
int fireMsg_timeout=60;
int critTemperature=65;
int INTERNET_TIMEOUT = 10;
float l_voltage=100;
float h_voltage=0;
float temperature[10],h_temperature;
int timers[6]; //sec,min,hour,days,mounth,year
int unx_timers[4] = {0,0,0,0};
int caller_count = 0; 
int caller_timer[4] = {60,60,60,60};
String caller_adr[4];

//=====================ФУНКЦІЇ ЧАСУ=============================
void timecalculate_unx(){
unx_timers[0]++;
if (unx_timers[0]>59) {unx_timers[0]=0; unx_timers[1]++;}
if (unx_timers[1]>59) {unx_timers[1]=0; unx_timers[2]++;}
if (unx_timers[2]>23) {unx_timers[2]=0; unx_timers[3]++;}
unx_text_time_line=String(unx_timers[3])+"days /"+String(unx_timers[2])+":"+String(unx_timers[1])+":"+String(unx_timers[0]);  
}

//розбір змінних часу на частини,відлік часу
void timecalculate_ntp(){
  if (INTERNET_TIMEOUT>0 and time_updated==false) {timeClient.update(); time_updated=true;} 
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);   
  timers[5]=ptm->tm_year+1900;
  timers[4]=ptm->tm_mon+1;
  timers[3]=ptm->tm_mday;
  timers[2]=timeClient.getHours();
  timers[1]=timeClient.getMinutes();
  timers[0]=timeClient.getSeconds(); 
  //INTERNET 
  INTERNET=false;
  if ((timers[5]>1970 and timers[5]<3000)){
    INTERNET=true;
    Serial.println("INTERNET!");
  } else {Serial.println("NO INTERNET!");}  
  if (INTERNET_TIMEOUT>0){INTERNET_TIMEOUT--; time_updated=false;}  
  text_time_line=String(timers[5])+"-"+String(timers[4])+"-"+String(timers[3])+"/"+String(timers[2])+":"+String(timers[1])+":"+String(timers[0]);
  } 

//=====================Елементи веб інтерфейсу=================

String add_button(String text = "", String link = "" ,String al=""){
  if (link!="") {
  if (al!="") {text="<a href="+link+"><button type=\"button\" align=\""+al+"\" >"+text+"</button></a>";} else {text="<a href="+link+"><button type=\"button\">"+text+"</button></a>";}
  } else {
    text="<button type=\"button\">"+text+"</button>";
    }
  return text;  
  }
  
String add_text_field(String label, String var_name, String T="", String L="" ,String al=""){
  String text = "";
  if (al!="") {text=text+"<form action=\"/\" align=\""+al+"\" >";} else {text=text+"<form action=\"/\">";}
  text=text+"<label for=\""+var_name+"\">"+label+"</label>";
  text=text+"<input type=\"text\" id=\""+var_name+"\" name=\""+var_name+"\"><br>";
  text=text+L;
  if (T!="") {text=text+"<input type=\"submit\" value="+T+">";} 
  text=text+"</form>";
  return text;    
}

String add_2text_field(String label1, String var_name1, String label2, String var_name2, String T="", String L1="", String L2="" ,String al=""){
  String text = "";
  if (al!="") {text=text+"<form action=\"/\" align=\""+al+"\" >";} else {text=text+"<form action=\"/\">";}
  text=text+"<label for=\""+var_name1+"\">"+label1+L1+"</label>";
  text=text+"<input type=\"text\" id=\""+var_name1+"\" name=\""+var_name1+"\"><br><br>";
  text=text+L1;
  text=text+"<label for=\""+var_name2+"\">"+label2+L2+"</label>";
  text=text+"<input type=\"text\" id=\""+var_name2+"\" name=\""+var_name2+"\"><br><br>"; 
  text=text+L2;
  text=text+"<input type=\"submit\" value="+T+">"; 
  text=text+"</form>";
  return text;    
}

//=====================Ввід команд============================= 

//прийом команд через порт
String serial_commands(String web_command = ""){
  String hta="";
  if ((Serial.available() > 0) or (web_command!="")) {
    String ts=Serial.readString()+web_command;
    Serial.println(">"+ts);
    if (ts=="/security=on") {eEPROM_sec_enable.EEPROM_WRITE_SV(true); secure_status=eEPROM_sec_enable.EEPROM_READ_BOOL();
      hta="security on";
      }
    if (ts=="/security=off") {eEPROM_sec_enable.EEPROM_WRITE_SV(false); secure_status=eEPROM_sec_enable.EEPROM_READ_BOOL();
      hta="security off";
      }   
    if (ts=="/reset") {ESP.restart();}
    if (ts.substring(0,11)=="/ssid_name=") {ssid_name=ts.substring(11,27); eEPROM_ssid.EEPROM_WRITE_STRING(ssid_name); ssid_name=eEPROM_ssid.EEPROM_READ_STRING();  
      hta="Set ssid_name:"+ssid_name;
      } 
    if (ts.substring(0,11)=="/ssid_pass=") {ssid_pass=ts.substring(11,27); eEPROM_pass.EEPROM_WRITE_STRING(ssid_pass); ssid_pass=eEPROM_pass.EEPROM_READ_STRING(); 
      hta="Set ssid_pass:"+ssid_pass;
      }
    if (ts.substring(0,9)=="/web_url=") {web_url=ts.substring(9,127); eEPROM_web_url.EEPROM_WRITE_STRING(web_url); web_url=eEPROM_web_url.EEPROM_READ_STRING(); 
      hta="Set web_key:"+web_url;
      } 
    if (ts.substring(0,8)=="/dev_id=") {dev_id=ts.substring(8,127); eEPROM_dev_id.EEPROM_WRITE_STRING(dev_id); dev_id=eEPROM_dev_id.EEPROM_READ_STRING();  
      hta="Set dev_id:"+dev_id;
      }
    if (ts.substring(0,7)=="/timer=") {max_relay_time=ts.substring(7,16).toInt(); eEPROM_max_rt.EEPROM_WRITE_SV(max_relay_time); max_relay_time=eEPROM_max_rt.EEPROM_READ_INT(); 
      hta="Set timer:"+String(max_relay_time);
      }
    if (ts=="/help") {Serial.println("Commands for user settings> /dev_id=, /timer=, /web_url=, /ssid_name=, /ssid_pass=, /reset");}
    Serial.println(hta);
    }   
  return hta;  
  }

//веб-сервер
void html_commands(){
  String ts,tts,tsx="";
  //обробка запитів
  if (Wserver.args()) {
    if (Wserver.arg("user_pass")!="" and wrong_password_inputs>0) { //user_pass
    user_pass=Wserver.arg("user_pass");
    }
  if (wrong_password_inputs==1){
    wrong_password_inputs=-1;
    sendmesage("TRY_BRUTFORCE",web_url,true);
  }
  if (user_pass==saved_pass){    
    if (Wserver.arg("page")!="") {
    page=Wserver.arg("page");
    }
    //--- 
    if (Wserver.arg("new_pass")!="") { //new_pass
    new_pass=Wserver.arg("new_pass");
    saved_pass=new_pass;
    user_pass=new_pass;
    ep_saved_pass.EEPROM_WRITE_STRING(new_pass);
    }  
    if (Wserver.arg("caller_counts")!="") {
    caller_count=Wserver.arg("caller_counts").toInt();
    ep_caller_count.EEPROM_WRITE_SV(caller_count);
    }  
    if (Wserver.arg("caller1")!="") {
    caller_adr[0]=Wserver.arg("caller1");
    ep_caller_1.EEPROM_WRITE_STRING(caller_adr[0]);
    } 
    if (Wserver.arg("caller2")!="") {
    caller_adr[1]=Wserver.arg("caller2");
    ep_caller_2.EEPROM_WRITE_STRING(caller_adr[1]);
    }
    if (Wserver.arg("caller3")!="") {
    caller_adr[2]=Wserver.arg("caller3");
    ep_caller_3.EEPROM_WRITE_STRING(caller_adr[2]);
    }
    if (Wserver.arg("caller4")!="") {
    caller_adr[3]=Wserver.arg("caller4");
    ep_caller_4.EEPROM_WRITE_STRING(caller_adr[3]);
    }
    //---   
    if (Wserver.arg("relay_time_on_HR")!="") { //relay_time_on_HR
    relay_time_on_HR=Wserver.arg("relay_time_on_HR").toInt();
    ep_relay_time_on_HR.EEPROM_WRITE_SV(relay_time_on_HR);
    }
    if (Wserver.arg("relay_time_on_MINS")!="") { //relay_time_on_MINS
    relay_time_on_MINS=Wserver.arg("relay_time_on_MINS").toInt();
    ep_relay_time_on_MINS.EEPROM_WRITE_SV(relay_time_on_MINS);
    }
    if (Wserver.arg("relay_time_off_HR")!="") { //relay_time_off_HR
    relay_time_off_HR=Wserver.arg("relay_time_off_HR").toInt();
    ep_relay_time_off_HR.EEPROM_WRITE_SV(relay_time_off_HR);
    }
    if (Wserver.arg("relay_time_off_MINS")!="") { //relay_time_off_MINS
    relay_time_off_MINS=Wserver.arg("relay_time_off_MINS").toInt();
    ep_relay_time_off_MINS.EEPROM_WRITE_SV(relay_time_off_MINS);
    }
    if (Wserver.arg("security_on")!="") {
    tts=Wserver.arg("security_on");
    if (tts=="true" or tts=="1"){secure_status=true;  ts=ts+"secure mode on";} else {secure_status=false; ts=ts+"secure mode off";} 
    if (secure_status==true) {sendmesage("SSTATUS_ON",web_url,true); sysOk=sysOk_timeout;} else {sendmesage("SSTATUS_OFF",web_url,true);}
    eEPROM_sec_enable.EEPROM_WRITE_SV(secure_status);
    }
    if (Wserver.arg("zoomer_on")!="") {
    tts=Wserver.arg("zoomer_on");
    if (tts=="true" or tts=="1"){zoomer_enabled=true; ts=ts+"zoomer on";} else {zoomer_enabled=false; ts=ts+"zoomer off";}   
    }
    if (Wserver.arg("relay_mode")!="") {
    tts=Wserver.arg("relay_mode");
    if (tts=="ON" or tts=="1" or tts=="on"){relay_setting="ON";}
    if (tts=="AUTO" or tts=="0" or tts=="auto"){relay_setting="AUTO";}
    if (tts=="OFF" or tts=="-1" or tts=="off"){relay_setting="OFF";}
    }
    if (Wserver.arg("ssid_name")!="") {
    tsn=Wserver.arg("ssid_name");
    }
    if (Wserver.arg("ssid_pass")!="") {
    tsp=Wserver.arg("ssid_pass");
    }
    if (Wserver.arg("ssid_test")!="") {
    tts=Wserver.arg("ssid_test");
    if ((tts=="true" or tts=="1") and tsn!="" and tsp!=""){if (connect_to_wifi(tsn,tsp,600)==true){
      ap_test=true;
      ap_tested=true;
      connect_to_wifi(ssid_name,ssid_pass,600);
      } else {connect_to_wifi(ssid_name,ssid_pass,600); ap_test=false;} }   
    }  
  if (Wserver.arg("ssid_save")!="") {
    tts=Wserver.arg("ssid_save");
    if ((tts=="true" or tts=="1") and tsn!="" and tsp!="") {
      connect_to_wifi(tsn,tsp,600);
      eEPROM_ssid.EEPROM_WRITE_STRING(tsn);
      eEPROM_pass.EEPROM_WRITE_STRING(tsp);
      sendmesage("SSID_CHANGED",web_url,true);
      ESP.reset();
      } else {ts=ts+"SSID:required parameters!";}
    }
  if (Wserver.arg("web_url")!="") {
    tts=Wserver.arg("web_url");
    web_url=tts; ts=ts+"web_url changed:"+tts;
    eEPROM_web_url.EEPROM_WRITE_STRING(web_url);  
    }        
  if (Wserver.arg("dev_id")!="") {
    tts=Wserver.arg("dev_id");
    dev_id=tts; ts=ts+"dev_id changed:"+tts;
    eEPROM_dev_id.EEPROM_WRITE_STRING(dev_id);  
    } 
  if (Wserver.arg("relay_timer")!="") {
    tts=Wserver.arg("relay_timer");
    max_relay_time=tts.toInt(); ts=ts+"relay_timer changed:"+tts+"<br>";
    eEPROM_max_rt.EEPROM_WRITE_SV(max_relay_time);  
    }
  //--------------------------------
  if (Wserver.arg("sysok_timer")!="") {      
    tts=Wserver.arg("sysok_timer");
    sysOk_timeout=tts.toInt(); ts=ts+"sysOk_timeout changed:"+tts+"<br>";
    eEPROM_sysok.EEPROM_WRITE_SV(sysOk_timeout); 
    } 
  if (Wserver.arg("crittemp")!="") {      
    tts=Wserver.arg("crittemp");
    critTemperature=tts.toInt(); ts=ts+"critTemperature changed:"+tts+"<br>";
    eEPROM_crTemp.EEPROM_WRITE_SV(critTemperature);
    }  
    if (Wserver.arg("firemsg_timer")!="") {      
    tts=Wserver.arg("firemsg_timer");
    fireMsg_timeout=tts.toInt(); ts=ts+"fireMsg_timeout changed:"+tts+"<br>";
    eEPROM_fire.EEPROM_WRITE_SV(fireMsg_timeout);
    }
    if (Wserver.arg("alert_timer")!="") {      
    tts=Wserver.arg("alert_timer");
    alert_timeout=tts.toInt(); ts=ts+"alert_timeout changed:"+tts+"<br>";
    eEPROM_alert.EEPROM_WRITE_SV(alert_timeout);
    } 
  if (Wserver.arg("reset")=="true") {
    Wserver.send(200,"text/html",add_button("PLEASE WAITING WHILE REBOOTING...","/?page=home")); 
    ESP.reset();
    }
  if (Wserver.arg("logout")=="true") {
    user_pass="";
    }
  session_timeout=5*60;
  wrong_password_inputs=10;         
    } else {
      ts="Wrong password!"; 
      if (wrong_password_inputs>0){
        wrong_password_inputs--;
      } else {
        ts=ts+"Input blocked, Try later after reboot system!";
      }
    }
  }
  //===============================================================
  //формування сторінок 
  //при наявності повідомлення
  if (ap_test==true and ap_tested==true){ts="Test OK";}
  if (ap_test==false and ap_tested==true){ts="AP NOT Found";}
  if (ts!=""){
    ts="<p style=\"background-color:Orange;\" align=\"center\" >"+ts+"</p>";
    }  
  //заголовок сторінки
  ts=ts+"<title>"+dev_id+"</title>";
  ts=ts+"<p style=\"background-color:Orange; color:Blue;\" align=\"center\" >";
  ts=ts+"<b>"+dev_id+"</b>";
  ts=ts+"</p>";
  ts=ts+"<p style=\"background-color:MediumSeaGreen;\" align=\"center\" >";
  //префікс сторінки
  ts=ts+"System Time:"+unx_text_time_line+"| Sensor in:"+String(move_sensor.dStatus())+"| Relay timer:"+String(relay_on_timer)+"<br>";
  if (l_voltage!=0) {ts=ts+"Battery:"+String(battery.backVolts(3.2*3))+"V(minimum="+String(l_voltage)+")(maximum="+String(h_voltage)+")";} else {ts=ts+"Battery: N/C" ;}  
  if (c_t_sensors>0) {
    ts=ts+" | Temperatures:("+String(c_t_sensors)+")";
    ts=ts+" "+String(temperature[0]);
    for (int i=1; i<c_t_sensors; i++){
    ts=ts+", "+String(temperature[i]);   
    }} else {ts=ts+" | Temperature: N/C ";} 
  if (INTERNET==true){ts=ts+"<br> NTP Time:"+text_time_line+"<br>";} else {ts=ts+"<br> NTP Time:"+text_time_line+"[NO INTERNET]<br>";}
  if (relay_time_on_HR>=0 and relay_time_off_HR>=0 and relay_time_on_MINS>=0 and relay_time_off_MINS>=0){ts=ts+"AUTO ON TIMER:"+relay_time_on_HR+":"+relay_time_on_MINS+"-"+relay_time_off_HR+":"+relay_time_off_MINS+"<br>";}
  if (secure_status==true){ts=ts+"Security status=ON "; } else {ts=ts+"Security status=OFF "; }
  ts=ts+" | Relay status="+relay_setting;
  if (zoomer_enabled==true){ts=ts+" | Zoomer=ON <br>"; } else {ts=ts+" | Zoomer=OFF <br>"; }
  //soft_ap 
  if (soft_ap==false){ts=ts+"Active SSID:"+ssid_name; } else {ts=ts+"Active SSID(Soft AP MODE):"+ssid_name_ap; }
  ts=ts+"</p>";
if (user_pass==saved_pass){
  if (page=="home"){  //тіло сторінки домашньої
    ts=ts+"<p style=\"background-color:Yellow;\" align=\"center\" >";    
    ts=ts+add_button("security_on","/?security_on=1")+add_button("security_off","/?security_on=0")+"<br>";
    ts=ts+add_button("zoomer_on","/?zoomer_on=1")+add_button("zoomer_off","/?zoomer_on=0")+"<br>";
    ts=ts+add_button("relay_mode_on","/?relay_mode=on")+add_button("relay_mode_auto","/?relay_mode=auto")+add_button("relay_mode_off","/?relay_mode=off")+"<br><br>";
    ts=ts+add_button("Connection Settings","/?page=main_settings")+add_button("Clock Settings","/?page=clk_settings")+"<br>";
    ts=ts+add_button("Timer Settings","/?page=add_settings")+add_button("Security Settings","/?page=security_settings")+"<br>";
    ts=ts+add_button("ABOUT SYSTEM","/?page=pins_settings")+add_button("LOGOUT["+String(session_timeout)+"Seconds]","/?logout=true")+"<br><br>";
    ts=ts+add_button("RESET","/?reset=true")+"<br>";
    ts=ts+"</p>";
    }
  if (page=="main_settings"){  //тіло сторінки настройок
    ts=ts+add_2text_field("SSID:","ssid_name","PASS:","ssid_pass","Send");
    if (tsn!="" and tsp!="") {ts=ts+"New AP Settings received: SSID="+tsn+" PASS=******** <br>";}
    ts=ts+add_button("Ssid_TEST","/?ssid_test=true")+add_button("Enable_and_REBOOT","/?ssid_save=true")+"<br><br>";
    ts=ts+"Current URL:"+web_url+"<br>";
    ts=ts+add_text_field("WEB_URL:","web_url","Save")+"<br><br>"; 
    ts=ts+"Current ID:"+dev_id+"<br>";
    ts=ts+add_text_field("Dev_id:","dev_id","Save")+"<br><br>"; 
    ts=ts+add_button("To Main","/?page=home")+"<br>";
    }
  if (page=="add_settings"){  //тіло сторінки додаткових настройок 
    ts=ts+"Current RT:"+String(max_relay_time)+"Second <br>"; 
    ts=ts+add_text_field("Relay_timer:","relay_timer","Save")+"<br><br>"; 
    ts=ts+"Current SysOK timeout:"+String(sysOk_timeout)+"Second <br>"; 
    ts=ts+add_text_field("SysOK_timer:","sysok_timer","Save")+"<br><br>";
    ts=ts+"Current Alert timeout:"+String(alert_timeout)+"Second <br>"; 
    ts=ts+add_text_field("Alert_timer:","alert_timer","Save")+"<br><br>";    
    ts=ts+"Current fireMsg timeout:"+String(fireMsg_timeout)+"Second <br>"; 
    ts=ts+add_text_field("fireMsg_timer:","firemsg_timer","Save")+"<br><br>";
    ts=ts+"Current critical Temperature:"+String(critTemperature)+"'C <br>"; 
    ts=ts+add_text_field("CritTemp:","crittemp","Save")+"<br><br>"; 
    ts=ts+add_button("To Main","/?page=home")+"<br>";           
  }
  if (page=="clk_settings"){  //тіло сторінки 
    ts=ts+"Current RT ON:"+String(relay_time_on_HR)+"HH <br>"; 
    ts=ts+add_text_field("Relay_timer_ON_HOUR:","relay_time_on_HR","Save")+"<br><br>";  
    ts=ts+"Current RT ON:"+String(relay_time_on_MINS)+"MM <br>"; 
    ts=ts+add_text_field("Relay_timer_ON_MINUTES:","relay_time_on_MINS","Save")+"<br><br>";
    ts=ts+"Current RT OFF:"+String(relay_time_off_HR)+"HH <br>"; 
    ts=ts+add_text_field("Relay_timer_OFF_HOUR:","relay_time_off_HR","Save")+"<br><br>";  
    ts=ts+"Current RT OFF:"+String(relay_time_off_MINS)+"MM <br>"; 
    ts=ts+add_text_field("Relay_timer_OFF_MINUTES:","relay_time_off_MINS","Save")+"<br><br>";   
    ts=ts+add_button("To Main","/?page=home")+"<br>"; 
  }
  if (page=="security_settings"){  //тіло сторінки 
    ts=ts+"Current SECURITY PASSWORD:******** <br>";     
    ts=ts+add_text_field("CHANGE PASSWORD:","new_pass","Save")+"<br><br>";
    ts=ts+"Current CALLER [LOGIC CENTRAL FUNCTION] COUNTS:"+String(caller_count)+"<br>";  
    ts=ts+add_text_field("ACTIVATE CALLERS:","caller_counts","Save")+"<br>";
    ts=ts+"Current:"+String(caller_adr[0])+"<br>"; 
    ts=ts+add_text_field("CHANGE CALLER1:","caller1","Save")+"<br>"; 
    ts=ts+"Current:"+String(caller_adr[1])+"<br>";  
    ts=ts+add_text_field("CHANGE CALLER2:","caller2","Save")+"<br>"; 
    ts=ts+"Current:"+String(caller_adr[2])+"<br>"; 
    ts=ts+add_text_field("CHANGE CALLER3:","caller3","Save")+"<br>"; 
    ts=ts+"Current:"+String(caller_adr[3])+"<br>"; 
    ts=ts+add_text_field("CHANGE CALLER4:","caller4","Save")+"<br><br>"; 
    ts=ts+add_button("To Main","/?page=home")+"<br>";    
  }
  if (page=="pins_settings"){  //тіло сторінки 
    ts=ts+"CURRENT PINOUTS FOR SYSTEM <br><br>"; 
    ts=ts+"Current EEPROM SIZE:"+String(eeprom_size)+" <br>"; 
    ts=ts+"Current LED pin:"+String(led_in_pin)+" <br>"; 
    ts=ts+"Current INTERRUPT pin:"+String(interrupt_pin)+" <br>"; 
    ts=ts+"Current BATTERY pin:"+String(battery_in_pin)+" <br>";     
    ts=ts+"Current SENSOR pin:"+String(sensor_in_pin)+" <br>";    
    ts=ts+"Current RELAY pin:"+String(relay_in_pin)+" <br>";     
    ts=ts+"Current ZOOMER pin:"+String(zoomer_pin)+" <br>";      
    ts=ts+"Current TEMPERATURE pin:"+String(ONE_WIRE_BUS_PIN)+" <br>";       
    ts=ts+"Current WIFI_OFF pin:"+String(wifi_off_pin)+" <br>";      
    ts=ts+"Current AP_ON pin:"+String(ap_mode_pin)+" <br>";      
    ts=ts+add_button("To Main","/?page=home")+"<br>";    
  }  
} else {
  if (wrong_password_inputs>0){
  ts=ts+add_text_field("ENTER PASSWORD:","user_pass","Login")+"<br><br>";
  } else {
    ts=ts+"Input blocked, Try later after reboot system!";
  }
}
  //суфікс сторінки
  ts=ts+"<p style=\"background-color:MediumSeaGreen;\" align=\"center\" >";
  ts=ts+"ESP8266 SECURITY  SYSTEM - MAKSYM SVIRCHEVSKYI 2022(c) - VER"+S_version+"["+comp_date+"]";
  ts=ts+"</p>";
  Wserver.send(200,"text/html",ts);  
  }

//відправка листа
int sendmesage(String mes, String url_key, bool std_message){
 WiFiClient client; 
 HTTPClient http;
 int httpResponseCode = -1;
 String serverPath = "";
 if (std_message==false) {serverPath = url_key+mes;} else {serverPath = url_key+"/?value1="+dev_id+"&value2="+mes+"&value3=http://"+WiFi.localIP().toString();}
 Serial.println(serverPath);
 http.begin(client, serverPath.c_str());
 httpResponseCode = http.POST(serverPath);
 if (httpResponseCode>0) {
   Serial.print("HTTP Response code: ");
   Serial.println(httpResponseCode);
   String payload = http.getString();
   Serial.println(payload);}
 else {
  Serial.print("Error code: ");
  Serial.println(httpResponseCode);}
 delay(100);
 http.end(); 
 return  httpResponseCode;   
 }

//=====================ФУНКЦІЇ ТАЙМЕРА==============================

void ICACHE_RAM_ATTR ISR(){
 move_sensor.ReadPin();
 Serial.println("CH_PIN_STATE="+String(move_sensor.dStatus())); 
 led_in.dWrite(!move_sensor.dStatus());
 //relay control
 if (move_sensor.dStatus()==true){relay_on_timer=max_relay_time;} 
 if (relay_setting=="OFF") {relay_on_timer=0;} 
 if (relay_on_timer>0 and relay_setting=="AUTO") {relay_on_timer--; } 
 if (ligth_up==0 and relay_setting=="AUTO") {relay_on_timer=0; }
 if (relay_setting=="ON") {relay_on_timer=max_relay_time;} 
 if (h_temperature>critTemperature) {relay_on_timer=0;}  
 if (relay_on_timer>0){relay_in.dWrite(true); } else {relay_in.dWrite(false);} 
 //messaging
 if (move_sensor.dStatus()==true and secure_status==true and alert<=0) {
 if (sendmesage("ALARM",web_url,true)==200) {alert=alert_timeout; sysOk=sysOk_timeout;}
 }
}

void time_flow_0(){ //1micros
  serial_commands();
  Wserver.handleClient();
  } 
  
void time_flow_1(){ //1millis

  }
  
void time_flow_2(){ //1second 
  c_t_sensors = 0;
  h_temperature = 0;
  float i_vol = battery.backVolts(3.2*3); //h_voltage
  if (i_vol>h_voltage){h_voltage=i_vol;}
  if (i_vol<l_voltage){l_voltage=i_vol;}
  c_t_sensors=sensors.getDeviceCount();
  sensors.requestTemperatures();
  for (int i=0; i<c_t_sensors; i++){
    temperature[i] = sensors.getTempCByIndex(i);
    if (temperature[i]>h_temperature){h_temperature=temperature[i];}    
  }
  timecalculate_unx();
  timecalculate_ntp();
  system_unix_type++; 
  if (caller_count>0){   // logical central zone
    for (int i=0; i<caller_count; i++){
      if (caller_timer[i]>0 and caller_adr[i]!=""){caller_timer[i]--;}
    }
    for (int i=0; i<caller_count; i++){
     if (caller_timer[i]<5 and caller_adr[i]!="") {if (sendmesage("",caller_adr[i],false)==200){caller_timer[i]=60;}}
    }
    for (int i=0; i<caller_count; i++){
      if (caller_timer[i]<=0 and caller_adr[i]!=""){
        for (int ii=0; ii<5; ii++) {if (sendmesage("offline:"+caller_adr[i],web_url,true)==200) {ii=5;}}
        caller_timer[i]=alert_timeout;
      }
    }
  Serial.println("Callers status["+String(caller_timer[0])+","+String(caller_timer[1])+","+String(caller_timer[2])+","+String(caller_timer[3])+"]");
  }
  }
  
void time_flow_3(){ //1second
  ligth_up = -1;
  //clock timer  
  int cur_time = (timers[2]*3600)+(timers[1]*60);
  int st_time = (relay_time_on_HR*3600)+(relay_time_on_MINS*60);
  int fin_time = (relay_time_off_HR*3600)+(relay_time_off_MINS*60);
  if (relay_time_on_HR>=0 and relay_time_off_HR>=0 and relay_time_on_MINS>=0 and relay_time_off_MINS>=0){
    ligth_up=0;
    if ((st_time<=fin_time) and (cur_time>st_time) and (cur_time<fin_time)){
      ligth_up=1;
    }
   if ((st_time>fin_time) and ((cur_time>st_time) or (cur_time<fin_time))){ //22/21=+ 22/3=- +| 1/21=- 1/3=+ +| 18/21=- 18/3=- -|
      ligth_up=1;
    }
  }
  //relay control
  if (relay_setting=="OFF") {relay_on_timer=0;} 
  if (relay_on_timer>0 and relay_setting=="AUTO") {relay_on_timer--; } 
  if (ligth_up==0 and relay_setting=="AUTO") {relay_on_timer=0; }
  if (relay_setting=="ON") {relay_on_timer=max_relay_time;}
  if (h_temperature>critTemperature) {relay_on_timer=0;}    
  if (relay_on_timer>0){relay_in.dWrite(true); } else {relay_in.dWrite(false);}   
  //session control
  if (session_timeout>0) {session_timeout--;}
  if (session_timeout==0){user_pass=""; session_timeout=-1;}  
  //power managment
  if (l_voltage<3.7 and last_power_state!=0){last_power_state=0; sendmesage("BATTERY_LOW",web_url,true); delay(10000); ESP.restart(); }
  if ((l_voltage>3.8 and l_voltage<4.4) and last_power_state!=1){last_power_state=1; sendmesage("POWER_OFF",web_url,true);} 
  if ((l_voltage>4.5 and l_voltage<5.9) and last_power_state!=2){last_power_state=2; sendmesage("POWER_ON",web_url,true);} 
  if (l_voltage>6 and last_power_state!=3){last_power_state=3; sendmesage("POWER_OVERLOAD",web_url,true);}   
  //send alarm message
  alert--;
  if (alert<0) {alert=0;}   
  if (move_sensor.dStatus()==true and secure_status==true and alert<=0) {
    if (sendmesage("ALARM",web_url,true)==200) {alert=alert_timeout; sysOk=sysOk_timeout;}
    }   //<<< 
  //send fire message
  fireMsg--;
  if (fireMsg<0) {fireMsg=0;}  
  if (h_temperature>critTemperature and fireMsg<=0) {
    if (sendmesage("FIRE",web_url,true)==200) {fireMsg=fireMsg_timeout; sysOk=sysOk_timeout;}
    }  //<<<
  //send sysOK message
  sysOk--;
  if (sysOk<0) {sysOk=0;}  
  if (sysOk<=0 and secure_status==true) {  
    if (sendmesage("SysOK",web_url,true)>0) {sysOk=sysOk_timeout;} 
    }  //<<<
  //signal system
  if (zoomer_enabled==true and relay_on_timer>0 and secure_status==true) {zoomer.buzzie(1000,500);}
  if (h_temperature>critTemperature) {zoomer.buzzie(1500,800);}   
  Serial.println("System Time:"+String(system_unix_type)+"/Sensor in:"+String(move_sensor.dStatus())+"/off timer:"+String(relay_on_timer)+"/temperature:"+String(h_temperature));
  Serial.println("NTP Time:"+text_time_line); 
  }

//=====================СТАРТОВІ ФУНКЦІЇ=============================

bool connect_to_wifi(String sid,String pid,int rep){
Serial.print("Connecting to ");
WiFi.mode(WIFI_STA);
WiFi.begin(sid,pid);
Serial.println(sid);
for (int i=rep; i>0; i--){
  if (WiFi.status()!=WL_CONNECTED){
    delay(100);
    led_in.dWriteReverse();
    Serial.print(".");
    } else { 
    Serial.println("OK");
    Serial.print("IP:");
    Serial.println(WiFi.localIP()); 
    return true; 
    } 
  } 
led_in.dWrite1();   
return false;  
}

void setup() {
//first init
/*
eEPROM_max_rt.EEPROM_WRITE_SV(20);
eEPROM_sec_enable.EEPROM_WRITE_SV(true);
eEPROM_ssid.EEPROM_WRITE_STRING("SSIDNAME");
eEPROM_pass.EEPROM_WRITE_STRING("SSIDPASS");
eEPROM_web_url.EEPROM_WRITE_STRING("http://maker.ifttt.com/trigger/TRIGGERKEY");
eEPROM_dev_id.EEPROM_WRITE_STRING("DEVICE NAME");
eEPROM_ssid_ap.EEPROM_WRITE_STRING("APSSIDNAME");
eEPROM_pass_ap.EEPROM_WRITE_STRING("APSSIDPASS");
eEPROM_sysok.EEPROM_WRITE_SV(3600*8);
eEPROM_alert.EEPROM_WRITE_SV(60*10);
eEPROM_fire.EEPROM_WRITE_SV(60*10);
eEPROM_crTemp.EEPROM_WRITE_SV(65);
ep_saved_pass.EEPROM_WRITE_STRING("DEFAULTPASSWORD");
ep_relay_time_on_HR.EEPROM_WRITE_SV(-1);
ep_relay_time_on_MINS.EEPROM_WRITE_SV(-1);
ep_relay_time_off_HR.EEPROM_WRITE_SV(-1);
ep_relay_time_off_MINS.EEPROM_WRITE_SV(-1);

ep_caller_count.EEPROM_WRITE_SV(1);
ep_caller_1.EEPROM_WRITE_STRING("http://192.168.0.206");
ep_caller_2.EEPROM_WRITE_STRING("");
ep_caller_3.EEPROM_WRITE_STRING("");
ep_caller_4.EEPROM_WRITE_STRING("");
*/
//normal init  
Serial.begin(9600);
//enable pins
move_sensor.enable();
led_in.enable();
relay_in.enable();
zoomer.enable();
battery.enable();
wifi_enable.enable();
ap_mode.enable(); 
//power start managment
if (battery.backVolts(3.3*3)<3.7) {delay(10000); ESP.restart(); }
//read memory
saved_pass=ep_saved_pass.EEPROM_READ_STRING();
relay_time_on_HR=ep_relay_time_on_HR.EEPROM_READ_INT();
relay_time_on_MINS=ep_relay_time_on_MINS.EEPROM_READ_INT();
relay_time_off_HR=ep_relay_time_off_HR.EEPROM_READ_INT();
relay_time_off_MINS=ep_relay_time_off_MINS.EEPROM_READ_INT();
max_relay_time=eEPROM_max_rt.EEPROM_READ_INT();
secure_status=eEPROM_sec_enable.EEPROM_READ_BOOL();
web_url=eEPROM_web_url.EEPROM_READ_STRING();
dev_id=eEPROM_dev_id.EEPROM_READ_STRING();
sysOk_timeout=eEPROM_sysok.EEPROM_READ_INT();
alert_timeout=eEPROM_alert.EEPROM_READ_INT();
fireMsg_timeout=eEPROM_fire.EEPROM_READ_INT();
critTemperature=eEPROM_crTemp.EEPROM_READ_INT();
ssid_name=eEPROM_ssid.EEPROM_READ_STRING();
ssid_pass=eEPROM_pass.EEPROM_READ_STRING();
ssid_name_ap=eEPROM_ssid_ap.EEPROM_READ_STRING();
ssid_pass_ap=eEPROM_pass_ap.EEPROM_READ_STRING();
caller_count=ep_caller_count.EEPROM_READ_INT();
if (caller_count>0) {caller_adr[0]=ep_caller_1.EEPROM_READ_STRING();}
if (caller_count>1) {caller_adr[1]=ep_caller_2.EEPROM_READ_STRING();}
if (caller_count>2) {caller_adr[2]=ep_caller_3.EEPROM_READ_STRING();}
if (caller_count>3) {caller_adr[3]=ep_caller_4.EEPROM_READ_STRING();}
//web services
wifi_enable.ReadPin();
ap_mode.ReadPin();
sensors.begin();
if (wifi_enable.dStatus()==1){
  if (ap_mode.dStatus()==1){
  connected_to_wifi=connect_to_wifi(ssid_name,ssid_pass,600);
  if (connected_to_wifi==true) {
    timeClient.begin();    
    Wserver.begin();
    Wserver.on("/", html_commands); 
    sendmesage("POWER_UP",web_url,true); 
    soft_ap=false;
    }
  } else {
    WiFi.softAP(ssid_name_ap,ssid_pass_ap);
    Wserver.begin();
    Wserver.on("/", html_commands);  
    soft_ap=true;   
    }
} else {WiFi.mode(WIFI_OFF);}
zoomer.buzzie(2000,500);
sysOk=sysOk_timeout; 
//interrupts
pinMode(interrupt_pin,INPUT);
attachInterrupt(digitalPinToInterrupt(interrupt_pin),ISR,CHANGE);
}

void loop() {
timer0.Tick();
timer1.Tick(); 
timer2.Tick();
timer3.Tick();
}
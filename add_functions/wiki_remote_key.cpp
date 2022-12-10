#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#define led_in 2
#define c_key 5
#define r_key 0
#define l_key 12
#define e_size 512
#define e_sname 8
#define e_spass 64
#define e_clip 128
#define e_clpass 192
#define e_myhname 256

bool left_key,rigth_key,center_key = false;
String ssid_name = "";
String ssid_pass = "";
String clientRemoteIP = "";
String clientRemotePass = "";
String myHostName = "";

void EEPROM_WRITE_STRING (String Str, int Adress) {
  byte lng=Str.length();
  int cIndex=Adress;
  EEPROM.begin(e_size);
  if (lng>128)  lng=128;
  EEPROM.write(cIndex,lng); 
  unsigned char* buf = new unsigned char[lng+2];
  Str.getBytes(buf,lng+1);
  cIndex++;
  for(byte i=0; i<lng; i++) {EEPROM.write(cIndex+i, buf[i]); delay(10);}
  EEPROM.end();
  } 

char *EEPROM_READ_STRING (int Adress) {
  int cIndex=Adress;
  EEPROM.begin(e_size);
  byte lng = EEPROM.read(cIndex);
  char* buf = new char[lng+2];
  cIndex++;
  for(byte i=0; i<lng; i++) buf[i] = char(EEPROM.read(cIndex+i));
  buf[lng] = '\x0';
  EEPROM.end();
  return buf;
  }  


bool connect_to_wifi(String sid,String pid,int rep){
Serial.print("Connecting to ");
WiFi.mode(WIFI_STA);
WiFi.begin(sid,pid);
Serial.println(sid);
for (int i=rep; i>0; i--){
  if (WiFi.status()!=WL_CONNECTED){
    delay(50);
    digitalWrite(led_in,0);
    delay(50);
    Serial.print(".");
    digitalWrite(led_in,1);
    } else { 
    Serial.println("OK");
    Serial.print("IP:");
    Serial.println(WiFi.localIP()); 
    return true; 
    } 
  } 
digitalWrite(led_in,1);  
return false;  
}

int send_to_client(String send_to, String val1 = "", String val2 = ""){
 WiFiClient client; 
 HTTPClient http;
 int httpResponseCode = -1;
 String serverPath = "";
 serverPath = "http://"+send_to+"/?"+val1+"&"+val2;
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

 void Serial_commands(){
  if (Serial.available()){
    String ts = Serial.readString();
    Serial.println(">"+ts);
    if (ts=="/reset") {ESP.restart();}
    if (ts.substring(0,11)=="/ssid_name=") {
      ssid_name=ts.substring(11,27); 
      EEPROM_WRITE_STRING(ssid_name,e_sname); 
      ssid_name=EEPROM_READ_STRING(e_sname);  
      Serial.println("Set ssid_name:"+ssid_name);
      } 
    if (ts.substring(0,11)=="/ssid_pass=") {
      ssid_pass=ts.substring(11,27); 
      EEPROM_WRITE_STRING(ssid_pass,e_spass); 
      ssid_pass=EEPROM_READ_STRING(e_spass); 
      Serial.println("Set ssid_pass:"+ssid_pass);
      }
    if (ts.substring(0,11)=="/remote_ip=") {
      clientRemoteIP=ts.substring(11,27); 
      EEPROM_WRITE_STRING(clientRemoteIP,e_clip); 
      clientRemoteIP=EEPROM_READ_STRING(e_clip); 
      Serial.println("Set client ip adress:"+clientRemoteIP);
      }
    if (ts.substring(0,13)=="/remote_pass=") {
      clientRemotePass=ts.substring(13,64); 
      EEPROM_WRITE_STRING(clientRemotePass,e_clpass); 
      clientRemotePass=EEPROM_READ_STRING(e_clpass); 
      Serial.println("Set remote pass:"+clientRemotePass);
      }
    if (ts.substring(0,11)=="/myHostName=") {
      myHostName=ts.substring(11,64); 
      EEPROM_WRITE_STRING(myHostName,e_myhname); 
      myHostName=EEPROM_READ_STRING(e_myhname); 
      Serial.println("Set host name:"+myHostName);
      }    
  }
 }

 void WiFi_enable(bool en = true){
  if (en==true) {
    WiFi.setSleep(false);
    }
  if (en==false){
    WiFi.setSleep(true);
  }  
 }


void setup() {
  //first init
  /*
  EEPROM_WRITE_STRING("",e_sname);
  EEPROM_WRITE_STRING("",e_spass);
  EEPROM_WRITE_STRING("192.168.0.205",e_clip);
  EEPROM_WRITE_STRING("esp8266_",e_clpass);
  EEPROM_WRITE_STRING("PV_Key_5V_",e_myhname);
  */
  //normal init
  ssid_name=EEPROM_READ_STRING(e_sname);
  ssid_pass=EEPROM_READ_STRING(e_spass);
  clientRemoteIP=EEPROM_READ_STRING(e_clip);
  clientRemotePass=EEPROM_READ_STRING(e_clpass);
  myHostName=EEPROM_READ_STRING(e_myhname);
  pinMode(led_in,OUTPUT);
  pinMode(c_key,INPUT_PULLUP);
  pinMode(r_key,INPUT_PULLUP);
  pinMode(l_key,INPUT_PULLUP);
  Serial.begin(9600);
  delay(5000);
  if (connect_to_wifi(ssid_name,ssid_pass,100)==true){
    WiFi.hostname(myHostName);
    Serial.println("all good:)"); 
  }

}

void loop() {
  left_key=!digitalRead(l_key);
  rigth_key=!digitalRead(r_key);
  center_key=!digitalRead(c_key);
  Serial_commands();
  if (left_key or rigth_key or center_key) {
    WiFi_enable(true);
    digitalWrite(led_in,false);
    } else {
      digitalWrite(led_in,true);
      }
  if (rigth_key and center_key) {
    send_to_client(clientRemoteIP,"user_pass="+clientRemotePass,"security_on=1");
    }
  if (left_key and center_key) {
    send_to_client(clientRemoteIP,"user_pass="+clientRemotePass,"security_on=0");
    }
  if (rigth_key and center_key==false) {
    send_to_client(clientRemoteIP,"user_pass="+clientRemotePass,"relay_mode=on");
    }
  if (left_key and center_key==false) {
    send_to_client(clientRemoteIP,"user_pass="+clientRemotePass,"relay_mode=off");
    }  
  if (left_key and rigth_key) {
    send_to_client(clientRemoteIP,"user_pass="+clientRemotePass,"relay_mode=auto");
    }  
  if (rigth_key==false and center_key==false and left_key==false){
    WiFi_enable(false);
    }
  delay(1000);
}
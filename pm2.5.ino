//arduino1.8.9
//偏好設定->額外的開發板管理員http://arduino.esp8266.com/stable/package_esp8266com_index.json
//選取板子NodeMCU 1.0(ESP-12E Module)
//下載 CP2102 驅動程式 http://www.silabs.com/products/development-tools/software/usb-to-uart-bridge-vcp-drivers
//安裝 ESP8266 工具，點擊「工具」>「板子」>「Boards Manager...」捲動右側拉桿到下方，點擊「esp8266 by ESP8266 Community」那一欄，再點擊「Install」鈕。
//教學文http://pizgchen.blogspot.com/2017/04/nodemcu-lab0.html
//專題雲端https://drive.google.com/open?id=19Z2LjjiWc_t9uwMZ_4dJikXZoMMH7EC2(含libraries)
//firebase new finger 6F D0 9A 52 C0 E9 E4 CD A0 D3 02 A4 B7 A1 92 38 2D CA 2F 26
//lcd i2c 函式庫載點 https://github.com/fdebrabander/Arduino-LiquidCrystal-I2C-library
//序列監控視窗選NL&CR 
//PMS3003函式庫https://github.com/fu-hsi/PMS
//RGB函式庫https://github.com/wilmouths/RGBLed
//RGB顏色轉換數值https://www.rapidtables.com/web/color/RGB_Color.html
//thingspeak網址https://thingspeak.com/channels/520167
//thingspeak密碼第一個字要大寫
//LCD有三種顯示模式 1.無wifi連接 2.顯示ip 3.一般模式
//------------------------------函式庫------------------------------------------------------
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <FirebaseArduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
#include "DHT.h"
DHT dht;
#include "PMS.h"
#include <RGBLed.h>
//------------------------------函式庫宣告------------------------------------------------------
PMS pms(Serial);//pm2.5感測模組PMS3003
PMS::DATA data;//pm2.5感測模組PMS3003
RGBLed led(D6, D7, D8, COMMON_CATHODE);//RGB LED腳位(紅,綠,藍,共陰or陽)
//------------------------------變數宣告------------------------------------------------------
boolean a;//手機開關資料
boolean c;//control 選擇溫控OR手動模式  預設為自動 c=0--自動  c=1--手動 
boolean t;//PM2.5是否大於開啟值 是---1  否----0
int pm25max; //風扇開啟值
int humidity;//濕度
int pm25;//pm2.5值
int temperature ;//溫度
int b;//指撥開關1(盒子上為3號) 
int d;//指撥開關2 on---顯示ip  off---LCD一般模式
int y;//有無wifi連接
int befored;//上一個迴圈指撥開關2的狀態
int beforet;//上一個迴圈的溫度值
int beforeh;//上一個迴圈的溼度值
int fanPin= D2;//D2為風扇控制腳
char* event="frakw";//不可加const
char* x;//ifttt
int iftttmax;//iftttt傳到line的警戒值
//-------------------------------esp8266設定--------------------------------------------------

const char* ssid = "Tenda_101668";//wifi name
const char* password = "24795612";//wifi password
WiFiServer server(80);
//-------------------------------api設定------------------------------------------------------
String apiKey = ""; //thingspeak apikey
const char* server1 = "api.thingspeak.com";//thingspeak 網址

#define FIREBASE_HOST ""//firebase專案網址
#define FIREBASE_AUTH ""//firebase資料庫密鑰
const char *host = "maker.ifttt.com";
const char *privateKey = "";//ifttt webhook api

void setup() {
//-----------------------------PM2.5感測模組設定-----------------------------------------------  
  pms.passiveMode(); 
  pms.wakeUp();
//-----------------------------LCD設定--------------------------------------------------------    
  Wire.begin(D5, D4);//LCD資料腳位設定 (SDA,SCL)***要打在lcd.begin()之前不然會有亂碼
  lcd.begin();//lcd初始化
  lcd.home();

//-----------------------------腳位設定--------------------------------------------------------   
  dht.setup(D3);//dht11資料接到D3  
  pinMode(fanPin, OUTPUT);//風扇控制腳
  pinMode(D3, INPUT);//dht 資料輸入腳
  pinMode(D0,INPUT); //指撥開關1輸入腳
  pinMode(D1,INPUT); //指撥開關2按鈕輸入腳
//-----------------------------串列阜初始化--------------------------------------------------------   
  Serial.begin(9600);  
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
 //-----------------------------初始值設定-------------------------------------------------------- 
 digitalWrite(fanPin, LOW);//預設風扇初始值為關  
 a=0; //風扇預設停轉
 c=0;//control預設為自動 c=0--自動  c=1--手動 
 pm25max = 54; //PM2.5預設開啟風扇值 超過將開啟風扇
 iftttmax= 54; //ifttt預設提醒值 超過將傳LINE提醒
//-----------------------------WIFI初始化--------------------------------------------------------   
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)//暫停直到連接WIFI 
  { 
    getvalue();
    nowifistate();//無wifi時進入auto模式
    nowifilcd();//無wifi LCD顯示no wifi connect
    RGB();
    delay(10);//不要跑太快 會當機 
  }
  lcd.clear();
  Serial.println("");
  Serial.println("WiFi connected");
  server.begin();
  Serial.println("Server started");
  Serial.print("Use this URL to connect: ");
  Serial.print("http://");
  Serial.print(WiFi.localIP());//發送區域IP位址到序列阜(電腦顯示)
  Serial.println("/");
//-----------------------------LCD開機畫面---------------------------------------  
  lcd.setCursor(0,0);
  lcd.print("WELCOME BACK");//開機畫面
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());//開機顯示區域網IP
  delay(5000);
  lcd.clear();//清除LCD 避免殘留的字
//-----------------------------其他----------------------------------------------  

Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH); //firebse帳戶連接
}
//-----------------------------主程式-------------------------------------------
void loop() {
 
  WiFiClient client = server.available();//網路伺服器(區域網使用)
  y=0;
  getvalue();
  RGB();
  
//-----------------------------如果無WIFI連接--------------------------------
  while (WiFi.status() != WL_CONNECTED) { //如果中途WIFI斷線 讓感測器 LCD RGB繼續運作  直到連接WIFI
    getvalue();
    nowifistate();//無wifi時進入auto模式
    nowifilcd();
    RGB();
    y=1;
    delay(10);  
  }
  
//-----------------------------無線控制模式---------------------------------  
  b=digitalRead(D0);//指撥開關1
  d=digitalRead(D1);//指撥開關2
  if(d!=befored||y==1)//如果中途指撥開關2改變或wifi斷線(已連回)因為變更LCD模式 所以要清除lcd 
  {
   lcd.clear();  
  }
  if(y==1)
  {
  lcd.setCursor(0,0);
  lcd.print("WELCOME BACK");//開機畫面
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());//開機顯示區域網IP
  delay(5000);
  lcd.clear(); 
  }
  befored=d;//把這次迴圈指撥開關2的狀態存入befored 以供下一次迴圈使用
  state();//輸出最終風扇狀態 
//-----------------------------指撥開關2 ON 進入ip模式 OFF 一般模式-----------
  if(d==1)
  {
    iplcd();//顯示ip
  }
  else
  {
    mylcd();//一般模式
  }
//----------------------------判斷全球網OR區網連線--------------------------- 

   if(b==0){ //全球網連線(指撥開關1 OFF)
    
//----------------------------接收firebase資料-----------------------------
       
    a=Firebase.getString("a").toInt(); //取得資料庫標籤"a"回傳值 從字串轉整數
    c=Firebase.getString("c").toInt();
    pm25max=Firebase.getString("maxpm25").toInt();
    iftttmax=Firebase.getString("iftttmax").toInt(); 
    Serial.println(a);
    Serial.println(c); 
    Serial.println(pm25max);
   
//----------------------------firebase上傳資料----------------------------- 
       
    Firebase.setInt("Temp",temperature);//發送資料到firebase
    Firebase.setInt("Humidity",humidity);
    Firebase.setInt("pm25",pm25);
    
//----------------------------thingspeak上傳資料------------------------------------

           if (client.connect(server1,80))
                     {  
                            
                           String postStr = "/update?api_key=";//製作GET網址
                             postStr+=apiKey;
                             postStr +="&field1=";
                             postStr += temperature;
                             postStr +="&field2=";
                             postStr += pm25;
                             postStr +="&field3=";
                             postStr += humidity;
                   client.print(String("GET ") + postStr + " HTTP/1.1\r\n" +
               "Host: " + server1 + "\r\n" + 
               "Connection: close\r\n\r\n");
 
                             Serial.print("Temperature: ");
                             Serial.print(temperature);
                             Serial.print("  Humidity: ");
                             Serial.print(humidity);
                             Serial.print("  pm2.5: ");
                             Serial.print(pm25);
                             Serial.println("  Send to Thingspeak.");
                      }
}
//-------------------------------------區網連線(指撥開關1 ON)----------------------------------------
   
else{
  
 WiFiClient client = server.available(); //網路伺服器
   if(!client)//手機控制清淨機開關
     {
      return;
     }
  String request = client.readStringUntil('\r');//取得get網址資料
  Serial.println(request);
  
//--------------------------html網頁----------------------------------- 
  client.flush();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println(""); //  do not forget this one
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  
  client.println("<br><br>");
  client.println("<a href=\"/FAN=ON\"\"><button style='FONT-SIZE: 13px; HEIGHT: 50px;WIDTH: 75px; 32px; Z-INDEX: 0; TOP: 50px;'>FAN ON </button></a>");
  client.println("<br><a href=\"/FAN=OFF\"\"><button style='FONT-SIZE: 13px; HEIGHT: 50px;WIDTH: 75px; 32px; Z-INDEX: 0; TOP: 50px;'>FAN OFF </button></a><br />");  
  client.println("<a href=\"/FAN=auto\"\"><button style='FONT-SIZE: 13px; HEIGHT: 50px;WIDTH: 75px; 32px; Z-INDEX: 0; TOP: 50px;'>AUTO </button></a><br />");
  client.println("PM2.5:");
  client.println(pm25);
  client.println("<br/>");//換行
  client.println("temperature:");
  client.println(temperature);
  client.println("<br/>");
  client.println("humidity:");
  client.print(humidity);
  client.println("<br/>");  
  client.println("air fresher made by TCIVS ELECTRONIC<br/>"); 
  client.println("</html>"); 
  client.println("FAN is now: ");
//---------------------------------區網判斷風扇開關----------------------------------- 
  
  if(request.indexOf("/FAN=ON") !=-1){//強制開
    a=1;//風扇on or off
    c=1;//手動1 自動0
  
    client.println("ON");
  }
   else if(request.indexOf("/FAN=OFF") !=-1)  {//強制關
    a=0;
    c=1;
    client.println("OFF");  
  }
 else if(request.indexOf("/FAN=auto") !=-1)  {//自動模式
    
    c=0;
    client.println("auto");  
  }
   client.println("</html>");
   Serial.print("a:");
   Serial.println(a);
   Serial.print("c:");
   Serial.println(c);
   Serial.println("");
   
 } 
}
 
//------------------------------副程式--------------------------------

//------------------------------重新連接firebase----------------------

void firebasereconnect(){
  
  Serial.println("Trying to reconnect");
 Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
}

//----------------------------判斷是否開啟風扇------------------------

void state(){
  //---------------------------判斷PM2.5值是大於開啟值-----------------
   if(pm25>=pm25max)
   {
   t=1;
   }  
   else  
  {  
   t=0;
  }
  if(pm25>=iftttmax)
  {
  ifttt(event);//發給ifttt在傳到LINE   
  }
  if(c==1)
  {   
   digitalWrite(fanPin,a);//手動模式   
  }
  else 
  {        
   digitalWrite(fanPin,t);//自動模式       
  } 
}
void nowifistate()
{
     if(pm25>=pm25max)
   {
   digitalWrite(fanPin,1);
   }  
   else  
  {  
   digitalWrite(fanPin,0);
  }
}
//-------------------------------LCD畫面(LCD一般模式)-----------------------------

void mylcd(){
  
    lcd.setCursor(0,0);//設定游標(x軸,y軸)
    lcd.print("PM2.5:");
    if(pm25>=100){
    lcd.print(pm25);
    }
    else{
    lcd.print(pm25);
    lcd.print(" ");  
    }
    lcd.setCursor(0,1);
    lcd.print("temp:"); 
    lcd.print(temperature);
    lcd.setCursor(7,1);
    lcd.print("  ");  
    lcd.setCursor(9,1);
    lcd.print("hum:");
    lcd.print(humidity);
    lcd.setCursor(15,1);    
    lcd.print(" ");
    lcd.setCursor(9,0);
    if(digitalRead(fanPin)==1)
    {   
    lcd.print("fan ON ");  //設定游標(x軸,y軸)
    }
    else
    {  
    lcd.print("fan OFF");    
    } 
}
//-------------------------------無連接WIFI時的LCD畫面(LCD無wifi模式)-----------------------------

void nowifilcd()
{         
    lcd.setCursor(0,0);
    lcd.print("NO WIFI connect ");   
    getvalue();//把感測器數值存入變數中   
    lcd.setCursor(0,1);//設定游標(x軸,y軸)
    lcd.print("PM:");
    lcd.print(pm25);
    lcd.print("  ");
    lcd.setCursor(7,1);
    lcd.print("T:"); 
    lcd.print(temperature);
    lcd.setCursor(12,1);
    lcd.print("H:");
    lcd.print(humidity);    
    
}
//-------------------------------取得感測器數值PMS3003,dht11-----------------------------
void getvalue(){ 
  if(pms.readUntil(data))
  {
    pm25=data.PM_AE_UG_2_5;
    Serial.print("PM 2.5 (ug/m3): ");
    Serial.println(data.PM_AE_UG_2_5); 
  } 
  temperature = dht.getTemperature();
  humidity = dht.getHumidity();
  if(temperature>100||humidity>100)//讓DHT11的亂碼不要儲存
  {
    temperature=beforet;
    humidity=beforeh;
  }
   beforet=temperature;
   beforeh=humidity;
}
//-------------------------------用PM2.5值判斷RGB顏色-----------------------------
void RGB(){
  if(pm25<=11){
     led.setColor(153, 255, 104);//淺綠    
    }
    else if(pm25<=23){
     led.setColor(0, 255, 0);//綠
    }
    else if(pm25<=35){
      led.setColor(0, 153, 0);//墨綠 
    }    
    else if(pm25<=41){
     led.setColor(255, 255, 0);//黃
    }
    else if(pm25<=47){
     led.setColor(255, 178, 102);//褐色 
    }    
    else if(pm25<=53){
     led.setColor(255, 128, 0);//橘色
    }
    else if(pm25<=58){
     led.setColor(255, 102, 102);//淺紅 
    }
    else if(pm25<=64){
     led.setColor(255, 0, 0);//紅
    }
    else if(pm25<=70){
     led.setColor(153, 0, 0);//暗紅 
    }
    else {//紫色
     led.setColor(255, 0, 255); 
    }
}
//-------------------------------get webhook網址觸發ifttt-----------------------------
void ifttt(char* x){
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) { 
    Serial.println("connection failed");
    return;
  }
  String url = "/trigger/";//製作GET網址
  url += x;
  url += "/with/key/";
  url += privateKey;
  url += "?";
  url += "value1=";
  url += pm25;
  url += "&";
  url += "value2=";
  url += temperature;
  url += "&";
  url += "value3=";
  url += humidity; 
  Serial.print("Requesting URL: ");
  Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
}
//-----------------------------------LCD顯示ip模式(LCD ip模式)---------------------------------
void iplcd()
{
    lcd.setCursor(0,0);
    lcd.print(WiFi.localIP());
    lcd.setCursor(0,1);//設定游標(x軸,y軸)
    lcd.print("PM:");
    lcd.print(pm25);
    lcd.setCursor(7,1);
    lcd.print("T:");    
    lcd.print(temperature);
    lcd.setCursor(12,1);
    lcd.print("H:");
    lcd.print(humidity);    
    
}

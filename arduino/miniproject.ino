
#define DEBUG
//#define DEBUG_WIFI

#include <WiFiEsp.h>
#include <SoftwareSerial.h>
#include <MsTimer2.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

#define AP_SSID "iot1"
#define AP_PASS "iot10000"
#define SERVER_NAME "10.10.141.64"
#define SERVER_PORT 5000
#define LOGID "KMJA_ARD"
#define PASSWD "PASSWD"

#define dustPIN A0
#define dustLEDPIN 12
#define DHTPIN 4
#define WIFIRX 8 //6:RX-->ESP8266 TX
#define WIFITX 7  //7:TX -->ESP8266 RX

#define CMD_SIZE 50
#define ARR_CNT 5
#define DHTTYPE DHT11
bool timerIsrFlag = false;
boolean dustFlag = false;

char sendId[10] = "KMJA_ARD";
char sendBuf[CMD_SIZE];
char lcdLine1[17] = "MINI Project";
char lcdLine2[17] = "WiFi Connecting!";

//float dust =0;
float dustVal = 0;
float dustValue;
unsigned int secCount;

char getSensorId[10];
int sensorTime;
float temp = 0.0;
float humi = 0.0;
bool updatTimeFlag = false;

DHT dht(DHTPIN, DHTTYPE);
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  lcd.init();
  lcd.backlight();
  lcdDisplay(0, 0, lcdLine1);
  lcdDisplay(0, 1, lcdLine2);

   pinMode(dustPIN, INPUT);    // 먼지센서 핀을 입력으로 설정 (생략 가능)
   pinMode(dustLEDPIN , OUTPUT);

#ifdef DEBUG
  Serial.begin(115200); //DEBUG
#endif
  wifi_Setup();

  MsTimer2::set(1000, timerIsr); // 1000ms period
  MsTimer2::start();

  dht.begin();

}

void loop() {
  if (client.available()) {
    socketEvent();
  }
  if (timerIsrFlag) //1초에 한번씩 실행
  {
    timerIsrFlag = false;
    if (!(secCount % 5)) //5초에 한번씩 실행
    {
      digitalWrite(dustLEDPIN ,LOW);
      delayMicroseconds(280);
      dustVal = analogRead(dustPIN);
      delayMicroseconds(40);
      digitalWrite(dustLEDPIN,HIGH);
      delayMicroseconds(9680); 
      

      // if (dustVal > 36.455) 
      // { 
        //dustValue = (float(dustVal/1024)-0.0356)*120000*0.035;
          dustValue =abs((0.17*(dustVal*(5.0/1024))-0.1))*1000;
        // sprintf(sendBuf, "[%s]dust@%s\n", sendId, dustValue);
        // client.write(sendBuf, strlen(sendBuf));
        // client.flush();
      //} 
      humi = dht.readHumidity();
      temp = dht.readTemperature();
#ifdef DEBUG
            Serial.print("Dust: ");
            Serial.print(dustValue);
            Serial.print(" Humidity: ");
            Serial.print(humi);
            Serial.print(" Temperature: ");
            Serial.println(temp);
#endif
      sprintf(lcdLine1, "Dust:%2d", (int)dustValue);
      lcdDisplay(0, 0, lcdLine1);
      sprintf(lcdLine2, "T:%2d, H:%2d", (int)temp, (int)humi);
      lcdDisplay(0, 1, lcdLine2);

      if (!client.connected()) {
        lcdDisplay(0, 1, "Server Down");
        server_Connect();
      }
    }
    if (sensorTime != 0 && !(secCount % sensorTime ))
    {
      //sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d\r\n", getSensorId, cds, (int)temp, (int)humi);
            char dustStr[5];
            char tempStr[5];
            char humiStr[5];
            dtostrf(dustValue,4 , 1, dustStr);
            dtostrf(humi, 4, 1, humiStr);  //50.0 4:전체자리수,1:소수이하 자리수
            dtostrf(temp, 4, 1, tempStr);  //25.1
            sprintf(sendBuf,"[%s]SENSOR@%s@%s@%s\n",getSensorId,dustStr,tempStr,humiStr);
      
          client.write(sendBuf, strlen(sendBuf));
          client.flush();
          
    }
    if (updatTimeFlag)
    {
      client.print("[GETTIME]\n");
      updatTimeFlag = false;
    }

  }
}

void socketEvent()
{
  int i = 0;
  char * pToken;
  char * pArray[ARR_CNT] = {0};
  char recvBuf[CMD_SIZE] = {0};
  int len;

  sendBuf[0] = '\0';
  len = client.readBytesUntil('\n', recvBuf, CMD_SIZE);
  client.flush();
#ifdef DEBUG
  Serial.print("recv : ");
  Serial.print(recvBuf);
#endif
  pToken = strtok(recvBuf, "[@]");
  while (pToken != NULL)
  {
    pArray[i] =  pToken;
    if (++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }
  if (!strncmp(pArray[1], " New", 4)) // New Connected
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    updatTimeFlag = true;
    return ;
  }
  else if (!strncmp(pArray[1], " Alr", 4)) //Already logged
  {
#ifdef DEBUG
    Serial.write('\n');
#endif
    client.stop();
    server_Connect();
    return ;
  }
  else if (!strncmp(pArray[1], "GETSENSOR", 9)) {
    if (pArray[2] != NULL) {
      sensorTime = atoi(pArray[2]);
      strcpy(getSensorId, pArray[0]);
      return;
    } else {
      sensorTime = 0;
      sprintf(sendBuf, "[%s]%s@%d@%d\n", pArray[0], pArray[1],  (int)temp, (int)humi);
    }
  }
  else
    return;

  client.write(sendBuf, strlen(sendBuf));
  client.flush();

#ifdef DEBUG
  Serial.print(", send : ");
  Serial.print(sendBuf);
#endif
}

void wifi_Setup() {
  wifiSerial.begin(38400);
  wifi_Init();
  server_Connect();
}

void wifi_Init()
{
  do {
    WiFi.init(&wifiSerial);
    if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI
      Serial.println("WiFi shield not present");
#endif
    }
    else
      break;
  } while (1);

#ifdef DEBUG_WIFI
  Serial.print("Attempting to connect to WPA SSID: ");
  Serial.println(AP_SSID);
#endif
  while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(AP_SSID);
#endif
  }
#ifdef DEBUG_WIFI
  Serial.println("You're connected to the network");
  printWifiStatus();
#endif
}
int server_Connect()
{
#ifdef DEBUG_WIFI
  Serial.println("Starting connection to server...");
#endif

  if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
    Serial.println("Connect to server");
#endif
    client.print("["LOGID":"PASSWD"]");
  }
  else
  {
#ifdef DEBUG_WIFI
    Serial.println("server connection failure");
#endif
  }
}

void printWifiStatus()
{
  // print the SSID of the network you're attached to

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
void timerIsr()
{
  timerIsrFlag = true;
  secCount++;
 
}
void lcdDisplay(int x, int y, char * str)
{
  int len = 16 - strlen(str);
  lcd.setCursor(x, y);
  lcd.print(str);
  for (int i = len; i > 0; i--)
    lcd.write(' ');
}


#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <SoftwareSerial.h>
#include "Fingerprint_Waveshare.h"

uint8_t id;
uint8_t finger_RxBuf[9];      
uint8_t finger_TxBuf[9];  

uint8_t  Finger_SleepFlag = 0;
int user_id;

SoftwareSerial mySerial(4, 5, false); // RX, TX 

// wifi
const char* ssid = "Wifi_SSID"; //type your WIFI information inside the quotes
const char* password = "Password_Wifi";
WiFiClient espClient;

// wifi UDP for NTP, we dont have real time and we dont trust http headers :)
WiFiUDP ntpUDP;
#define NTP_OFFSET   60 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "europe.pool.ntp.org"
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// OTA
#define SENSORNAME "Name_OTA" //change this to whatever you want to call your device
#define OTApassword "password_OTA" //the password you will need to enter to upload remotely via the ArduinoIDE yourOTApassword
int OTAport = 8266;

// MQTT
const char* mqtt_server = "192.168.1.XXX"; // IP address or dns of the mqtt
const char* mqtt_username = "MQTT_USERNAME"; //
const char* mqtt_password = "MQTT_PASSWORD";
const int mqtt_port = 1883;
PubSubClient client(espClient);

// MQTT TOPICS (change these topics as you wish) 
const char* fingerprint_topic = "home/presence/fingerprint";

void setup() {
    Serial.begin(19200);
    
    setup_wifi();
    timeClient.begin();
    client.setServer(mqtt_server, mqtt_port);

    // OTA setup
    ArduinoOTA.setHostname(SENSORNAME);
    ArduinoOTA.setPort(OTAport);
    ArduinoOTA.setPassword((const char *)OTApassword);
    ArduinoOTA.begin();

    Finger_SoftwareSerial_Init(); 
}

void setup_wifi() {
  delay(5);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // bit more power hungry, but seems stable.
  WiFi.hostname("WIFI_NAME"); // This will (probably) happear on your router somewhere.
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(SENSORNAME, mqtt_username, mqtt_password)) {
      Serial.println("connected");
    } else {
      Serial.print("MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void reading_fingerprint() {
    Serial.println("Put your finger on the sensor");
    switch(VerifyUser())
    {
      case ACK_SUCCESS: 
        Serial.print("Fingerprint No.");
        Serial.print(user_id); 
        Serial.println(" matched!");
        break;
      case ACK_NO_USER:
        Serial.println("Failed: This fingerprint doesn't exist in the base !");
        break;
      case ACK_TIMEOUT: 
        Serial.println("Failed: time out!");
        break;  
    }
   if((user_id==1) || (user_id==2) || (user_id==3) || (user_id==4) || (user_id==12) || (user_id==13) || (user_id==17)){
     if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
       Serial.print("Fingerprint of William Detected");
       client.publish(fingerprint_topic, "Fingerprint_William_detected");
       delay(1500);
       Serial.println();
      }
   }

   if((user_id==18) || (user_id==19) || (user_id==20) || (user_id==21) || (user_id==22) || (user_id==23) || (user_id==24) || (user_id==25) || (user_id==26) || (user_id==27) || (user_id==28) || (user_id==29)){
     if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
       Serial.print("Fingerprint of Marlene Detected");
       client.publish(fingerprint_topic, "Fingerprint_Marlene_detected");
       delay(1500);
       Serial.println();
      }
   }

}

void loop() {
    ArduinoOTA.handle();
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    reading_fingerprint();
    ESP.wdtFeed();
}

/***************************************************************************
* @brief      initialize the SoftwareSerial to communicate with Fingerprint module
****************************************************************************/
void Finger_SoftwareSerial_Init(void)
{
  mySerial.begin(19200);
  Serial.println(" ");  
}

/***************************************************************************
* @brief      Send a byte of data to the serial port
* @param      temp : Data to send
****************************************************************************/
void  TxByte(uint8_t temp)
{
  mySerial.write(temp);    
}

/***************************************************************************
* @brief      send a command, and wait for the response of module
* @param      Scnt: The number of bytes to send
        Rcnt: expect the number of bytes response module
        Nms: wait timeout: Delay
* @return     ACK_SUCCESS: success
          other: see the macro definition
****************************************************************************/
uint8_t TxAndRxCmd(uint8_t Scnt, uint8_t Rcnt, uint16_t Nms)
{
  uint8_t  i, j, CheckSum;
  uint16_t   uart_RxCount = 0;
  unsigned long  time_before = 0;
  unsigned long  time_after = 0;
  uint8_t   overflow_Flag = 0;; 
  
   TxByte(CMD_HEAD);     
   CheckSum = 0;
   for (i = 0; i < Scnt; i++)
   {
    TxByte(finger_TxBuf[i]);     
    CheckSum ^= finger_TxBuf[i];
   }  
   TxByte(CheckSum);
   TxByte(CMD_TAIL);  
   
   memset(finger_RxBuf,0,sizeof(finger_RxBuf));   ////////
 
   mySerial.flush();  /////
   
   // Receive time out: Nms
  time_before = millis();  
   do
   {
    overflow_Flag = 0;
    if(mySerial.available())
    {
      finger_RxBuf[uart_RxCount++] = mySerial.read();
    }
    time_after = millis();  
    if(time_before > time_after)   //if overflow (go back to zero)
    {
      time_before = millis();   // get time_before again
      overflow_Flag = 1;
    }
    
   } while (((uart_RxCount < Rcnt) && (time_after - time_before < Nms)) || (overflow_Flag == 1));

   user_id=finger_RxBuf[2]*0 +finger_RxBuf[3];
   if (uart_RxCount != Rcnt)return ACK_TIMEOUT;
   if (finger_RxBuf[0] != CMD_HEAD) return ACK_FAIL;
   if (finger_RxBuf[Rcnt - 1] != CMD_TAIL) return ACK_FAIL;
   if (finger_RxBuf[1] != (finger_TxBuf[0])) return ACK_FAIL;  
   CheckSum = 0;
   for (j = 1; j < uart_RxCount - 1; j++) CheckSum ^= finger_RxBuf[j];
   if (CheckSum != 0) return ACK_FAIL;    
   return ACK_SUCCESS;
}  

/***************************************************************************
* @brief      Query the number of existing fingerprints
* @return     0xFF: error
          other: success, the value is the number of existing fingerprints
****************************************************************************/
uint8_t GetUserCount(void)
{
  uint8_t m;
  
  finger_TxBuf[0] = CMD_USER_CNT;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;  
  
  m = TxAndRxCmd(5, 8, 200);
      
  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS)
  {
      return finger_RxBuf[3];
  }
  else
  {
    return 0xFF;
  }
}

/***************************************************************************
* @brief      Get Compare Level
* @return     0xFF: error
          other: success, the value is compare level
****************************************************************************/
uint8_t GetcompareLevel(void)
{
  uint8_t m;
  
  finger_TxBuf[0] = CMD_COM_LEV;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 1;
  finger_TxBuf[4] = 0;  
  
  m = TxAndRxCmd(5, 8, 200);
    
  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS)
  {
      return finger_RxBuf[3];
  }
  else
  {
    return 0xFF;
  }
}

/***************************************************************************
* @brief      Set Compare Level
* @param      temp: Compare Level,the default value is 5, can be set to 0-9, the bigger, the stricter
* @return     0xFF: error
          other: success, the value is compare level
****************************************************************************/
uint8_t SetcompareLevel(uint8_t temp)
{
  uint8_t m;
  
  finger_TxBuf[0] = CMD_COM_LEV;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = temp;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;  
  
  m = TxAndRxCmd(5, 8, 200);

  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS)
  {
      return finger_RxBuf[3];
  }
  else
  {
    return 0xFF;
  }
}

/***************************************************************************
* @brief      Get the time that fingerprint collection wait timeout 
* @return     0xFF: error
          other: success, the value is the time that fingerprint collection wait timeout 
****************************************************************************/
uint8_t GetTimeOut(void)
{
  uint8_t m;
  
  finger_TxBuf[0] = CMD_TIMEOUT;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 1;
  finger_TxBuf[4] = 0;  
  
  m = TxAndRxCmd(5, 8, 200);
    
  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS)
  {
      return finger_RxBuf[3];
  }
  else
  {
    return 0xFF;
  }
}

/***************************************************************************
* @brief      Register fingerprint
* @return     ACK_SUCCESS: success
          other: see the macro definition
****************************************************************************/
uint8_t AddUser(void)
{
  uint8_t m;
  
  m = GetUserCount();
  if (m >= USER_MAX_CNT)
    return ACK_FULL;


  finger_TxBuf[0] = CMD_ADD_1;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = m +1;
  finger_TxBuf[3] = 3;
  finger_TxBuf[4] = 0;    
  m = TxAndRxCmd(5, 8, 6000); 
  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS)
  {
    finger_TxBuf[0] = CMD_ADD_3;
    m = TxAndRxCmd(5, 8, 6000);
    if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS)
    {
      return ACK_SUCCESS;
    }
    else
    return ACK_FAIL;
  }
  else
    return ACK_FAIL;
}

/***************************************************************************
* @brief      Clear fingerprints
* @return     ACK_SUCCESS:  success
          ACK_FAIL:     error
****************************************************************************/
uint8_t ClearAllUser(void)
{
  uint8_t m;
  
  finger_TxBuf[0] = CMD_DEL_ALL;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;
  
  m = TxAndRxCmd(5, 8, 500);
  
  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS)
  {     
    return ACK_SUCCESS;
  }
  else
  {
    return ACK_FAIL;
  }
}

uint8_t ClearFingerprint(void)
{
  uint8_t m;

  finger_TxBuf[0] = CMD_DEL;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;
  
  m = TxAndRxCmd(5, 8, 500);
  
  if (m == ACK_SUCCESS && finger_RxBuf[4] == ACK_SUCCESS)
  {     
    return ACK_SUCCESS;
  }
  else
  {
    return ACK_FAIL;
  }
}

/***************************************************************************
* @brief      Check if user ID is between 1 and 3
* @return     TRUE
          FALSE
****************************************************************************/
uint8_t IsMasterUser(uint8_t UserID)
{
    if ((UserID == 1) || (UserID == 2) || (UserID == 3)) return TRUE;
      else  return FALSE;
}  

/***************************************************************************
* @brief      Fingerprint matching
* @return     ACK_SUCCESS: success
          other: see the macro definition
****************************************************************************/
uint8_t VerifyUser(void)
{
  uint8_t m;
  
  finger_TxBuf[0] = CMD_MATCH;
  finger_TxBuf[1] = 0;
  finger_TxBuf[2] = 0;
  finger_TxBuf[3] = 0;
  finger_TxBuf[4] = 0;
  
  m = TxAndRxCmd(5, 8, 5000);
    
  if ((m == ACK_SUCCESS) && (IsMasterUser(finger_RxBuf[4]) == TRUE) && finger_RxBuf[3] != 0)
  { 
     return ACK_SUCCESS;
  }
  else if(finger_RxBuf[4] == ACK_NO_USER)
  {
    return ACK_NO_USER;
  }
  else if(finger_RxBuf[4] == ACK_TIMEOUT)
  {
    return ACK_TIMEOUT;
  }

}

/***************************************************************************
* @brief      Wait until the fingerprint module works properly
****************************************************************************/
void Finger_Wait_Until_OK(void)
{   
    //digitalWrite(Finger_RST_Pin , LOW);
  //delay(300); 
    //digitalWrite(Finger_RST_Pin , HIGH);
  //delay(300);  // Wait for module to start
    
   // ERROR: Please ensure that the module power supply is 3.3V or 5V, 
   // the serial line is correct, the baud rate defaults to 19200,
   // and finally the power is switched off, and then power on again !
  while(SetcompareLevel(5) != 5)
  {   
    Serial.println("*Please waiting for communicating normally, if it always keep waiting here, please check your connection!*");
  }

  Serial.println(" ");
  Serial.write("*************** WaveShare Capacitive Fingerprint Reader Test ***************\r\n");
  Serial.write("Compare Level:  5    (can be set to 0-9, the bigger, the stricter)\r\n"); 
  Serial.write("Nombre d'empreintes déjà das la base :  ");  Serial.print(GetUserCount());
  Serial.write("\r\n Use the serial port to send the commands to operate the module:\r\n"); 
  Serial.write(" 1 : Voir le nombre d'empreintes enregistrées\r\n"); 
  Serial.write(" 2 : Ajouter une empreinte  (Each entry needs to be read two times: \"beep\", "); Serial.write("put the finger on sensor\r\n"); 
  Serial.write(" 3 : Vérifier les empreintes  (Send the command, then put your finger on sensor. "); Serial.write("Each time you send a command, module waits and matches once)\r\n"); 
  Serial.write(" 100 : test\r\n");
  Serial.write(" 199 : Vider la base de données\r\n"); 
  Serial.write("*************** WaveShare Capacitive Fingerprint Reader Test ***************\r\n"); 
  Serial.println(" ");
}

/***************************************************************************
* @brief      Analysis of the serial port 2 command (computer serial assistant)
****************************************************************************/

void Analysis_PC_Command() {
    if(id == 1) {
      Serial.print("Nombre d'empreintes enregistrées : "); Serial.println(GetUserCount());
    }
    
    if(id == 2) {
      //Serial.println(" Add fingerprint (Each entry needs to be read two times: "); 
      Serial.println("Placez votre doigt sur le capteur"); 
            switch(AddUser())
            {
              case ACK_SUCCESS:
                Serial.println("Empreinte enregistrée dans la base !");
                break;
              
              case ACK_FAIL:      
                Serial.println("Echec: Placer votre doigt au centre du capteur, ou cette empreinte existe déjà !");
                break;
              
              case ACK_FULL:      
                Serial.println("Echec: La base est pleine !");
                break;    
            }
       Serial.print("Nombre d'empreintes dans la base : "); Serial.println(GetUserCount());
    }

    if(id == 3) {
            Serial.println("Placez votre doigt sur le capteur");
            switch(VerifyUser())
            {
              case ACK_SUCCESS: 
                Serial.print("Empreinte N°");
                Serial.print(user_id); 
                Serial.println(" reconnue !");
                break;
              case ACK_NO_USER:
                Serial.println("Echec: Cette empreinte n'existe pas dans la base !");
                break;
              case ACK_TIMEOUT: 
                Serial.println("Echec: trop tard !");
                break;  
            }
    }
    if(id == 100) {
            ClearFingerprint();
            Serial.println("L'empreinte est supprimé de la base !");
    }
    if(id == 199) {
            ClearAllUser();
            Serial.println("Toutes les empreintes de la base sont supprimées !");
    }
  }
  
/***************************************************************************
* @brief  
     If you enter the sleep mode, then open the Automatic wake-up function of the finger,
     begin to check if the finger is pressed, and then start the module and match
****************************************************************************/
void Auto_Verify_Finger(void)
{
      mySerial.begin(19200);
      Serial.println("Vérification d'existence : Garder votre doigt sur le lecteur");
      delay(2000);   
      switch(VerifyUser())
      {       
        case ACK_SUCCESS: 
          Serial.println("Retirez votre doigt...");  
          delay(1500);
          Serial.print("Empreinte N°");
          Serial.print(user_id);
          Serial.println(" reconnue");
          break;
        case ACK_NO_USER:
          Serial.println("Retirez votre doigt...");  
          Serial.println("Echec: Cette empreinte n'existe pas dans la base !");
          break;
        case ACK_TIMEOUT: 
          Serial.println("Echec: Time out !");
          break;  
        default:
          Serial.println("Echec: erreur!");
          break;
     }
}

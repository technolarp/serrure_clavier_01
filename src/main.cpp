/*
   ----------------------------------------------------------------------------
   TECHNOLARP - https://technolarp.github.io/
   SERRURE CLAVIER 01 - https://github.com/technolarp/serrure_clavier_01
   version 1.0 - 09/2023
   ----------------------------------------------------------------------------
*/

/*
   ----------------------------------------------------------------------------
   Pour ce montage, vous avez besoin de 
   1 clavier matriciel 4*4 
   1 carte PCF8574 I2C
   1 ou + led ws2812b
   1 buzzer piezo
   ----------------------------------------------------------------------------
*/

/*
   ----------------------------------------------------------------------------
   PINOUT
   D0     NEOPIXEL
   D1     I2C SCL => SCL PCF8574
   D2     I2C SDA => SDA PCF8574
   D8     BUZZER
   ----------------------------------------------------------------------------
*/

/*
TODO version 1.1

option pour utiliser 2 led verte/rouge
*/

#include <Arduino.h>

// WIFI
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// WEBSOCKET
AsyncWebSocket ws("/ws");

char bufferWebsocket[300];
bool flagBufferWebsocket = false;

// FASTLED
#include <technolarp_fastled.h>
M_fastled aFastled;

// KEYPAD
#include <technolarp_keypad.h>
M_keypad* aKeypad;

// BUZZER
#define PIN_BUZZER D8
#include <technolarp_buzzer.h>
M_buzzer buzzer(PIN_BUZZER);

// CONFIG
#include "config.h"
M_config aConfig;

// CODE ACTUEL DE LA SERRURE
char codeSerrureActuel[MAX_SIZE_CODE] = "xxxxxxxx";

#define BUFFERSENDSIZE 600
char bufferToSend[BUFFERSENDSIZE];
char bufferUid[12];

// STATUTS DE L OBJET
enum {
  OBJET_OUVERT = 0,
  OBJET_FERME = 1,
  OBJET_BLOQUE = 2,
  OBJET_ERREUR = 3,
  OBJET_RECONFIG = 4,
  OBJET_BLINK = 5
};

// PARAM RECONFIG
enum {
  RECONFIG_CODE = 0,
  RECONFIG_ERREUR = 1,
  RECONFIG_DELAI = 2,
  RECONFIG_TAILLE_CODE = 3
};

// DIVERS
bool uneFois = true;

char bufferReconfig[8] = {'0','0','0','0','0','0','0','0'};
uint8_t modeReconfig = 0;


// HEARTBEAT
uint32_t previousMillisHB;
uint32_t intervalHB;

// FUNCTION DECLARATIONS
uint16_t checkValeur(uint16_t valeur, uint16_t minValeur, uint16_t maxValeur);
void serrureFermee();
void serrureOuverte();
void serrureErreur();
void serrureBloquee();
void serrureBlink();
void appuiClavier();
void serrureReconfig();
void checkChangementParametres();
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len); 
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len); 
void handleWebsocketBuffer();
void notFound(AsyncWebServerRequest *request);
void checkCharacter(char* toCheck, const char* allowed, char replaceChar);
void sendObjectConfig();
void writeObjectConfig();
void sendNetworkConfig();
void writeNetworkConfig();
void sendUptime();
void sendStatut();
void sendMaxLed();
void convertStrToRGB(const char * source, uint8_t* r, uint8_t* g, uint8_t* b);



/*
   ----------------------------------------------------------------------------
   SETUP
   ----------------------------------------------------------------------------
*/
void setup()
{
  Serial.begin(115200);

  // VERSION
  delay(500);
  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("----------------------------------------------------------------------------"));
  Serial.println(F("TECHNOLARP - https://technolarp.github.io/"));
  Serial.println(F("SERRURE CLAVIER 01 - https://github.com/technolarp/serrure_clavier_01"));
  Serial.println(F("version 1.0 - 09/2023"));
  Serial.println(F("----------------------------------------------------------------------------"));

  // I2C RESET
  aConfig.i2cReset();
  
  // CONFIG OBJET
  Serial.println(F(""));
  Serial.println(F(""));
  aConfig.mountFS();
  aConfig.listDir("/");
  aConfig.listDir("/config");
  aConfig.listDir("/www");
  
  aConfig.printJsonFile("/config/objectconfig.txt");
  aConfig.readObjectConfig("/config/objectconfig.txt");

  aConfig.printJsonFile("/config/networkconfig.txt");
  aConfig.readNetworkConfig("/config/networkconfig.txt");

  // FASTLED
  aFastled.setNbLed(aConfig.objectConfig.activeLeds);
  aFastled.setControlBrightness(aConfig.objectConfig.scintillementOnOff);
  aFastled.setIntervalControlBrightness(aConfig.objectConfig.intervalScintillement);
  
  // animation led de depart
  aFastled.animationDepart(50, aFastled.getNbLed()*2, CRGB::Blue);

  // KEYPAD
  aKeypad = new M_keypad();

  // CHECK RESET OBJECT CONFIG  
  if (aKeypad->checkReset('*'))
  {
    aFastled.allLedOn(CRGB::Yellow, true);
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET OBJECT CONFIG !!!"));
    Serial.println(F(""));
    aConfig.writeDefaultObjectConfig("/config/objectconfig.txt");
    sendObjectConfig();

    delay(1000);
  }

  // CHECK RESET NETWORK CONFIG  
  if (aKeypad->checkReset('D'))
  {
    aFastled.allLedOn(CRGB::Cyan, true);
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET NETWORK CONFIG !!!"));
    Serial.println(F(""));
    aConfig.writeDefaultNetworkConfig("/config/networkconfig.txt");
    sendNetworkConfig();

    delay(1000);
  }

  // WIFI
  WiFi.disconnect(true);

  Serial.println(F(""));
  Serial.println(F("connecting WiFi"));
  
  // AP MODE
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(aConfig.networkConfig.apIP, aConfig.networkConfig.apIP, aConfig.networkConfig.apNetMsk);
  bool apRC = WiFi.softAP(aConfig.networkConfig.apName, aConfig.networkConfig.apPassword);

  if (apRC)
  {
    Serial.println(F("AP WiFi OK"));
  }
  else
  {
    Serial.println(F("AP WiFi failed"));
  }

  // Print ESP soptAP IP Address
  Serial.print(F("softAPIP: "));
  Serial.println(WiFi.softAPIP());
  
  /*
  // CLIENT MODE POUR DEBUG
  const char* ssid = "SID";
  const char* password = "PASSWORD";
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  if (WiFi.waitForConnectResult() != WL_CONNECTED) 
  {
    Serial.println(F("WiFi Failed!"));
  }
  else
  {
    Serial.println(F("WiFi OK"));
  }

  // Print ESP Local IP Address
  Serial.print(F("localIP: "));
  Serial.println(WiFi.localIP());
  */
    
  // WEB SERVER
  // Route for root / web page
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("config.html");
  server.serveStatic("/config", LittleFS, "/config/");
  server.onNotFound(notFound);

  // WEBSOCKET
  ws.onEvent(onEvent);
  server.addHandler(&ws);

  // Start server
  server.begin();

  // BUZZER
  buzzer.doubleBeep();

  // HEARTBEAT
  previousMillisHB = millis();
  intervalHB = 5000;

  // SERIAL
  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("START !!!"));
}
/*
   ----------------------------------------------------------------------------
   FIN DU SETUP
   ----------------------------------------------------------------------------
*/




/*
   ----------------------------------------------------------------------------
   LOOP
   ----------------------------------------------------------------------------
*/
void loop()
{  
  // AVOID WATCHDOG
  yield();
  
  // WEBSOCKET
  ws.cleanupClients();

  // FASTLED
  aFastled.updateAnimation();

  // CONTROL BRIGHTNESS
  aFastled.controlBrightness(aConfig.objectConfig.brightness);
  
  // BUZZER
  buzzer.update();

  // gerer le statut de la serrure
  switch (aConfig.objectConfig.statutActuel)
  {
    case OBJET_FERME:
      // la serrure est fermee
      serrureFermee();
      break;

    case OBJET_OUVERT:
      // la serrure est ouverte
      serrureOuverte();
      break;

    case OBJET_BLOQUE:
      // la serrure est bloquee
      serrureBloquee();
      break;

    case OBJET_ERREUR:
      // un code incorrect a ete entrer
      serrureErreur();
      break;

    case OBJET_RECONFIG:
      // un parametre doit etre modifie
      serrureReconfig();
      break;

    case OBJET_BLINK:
      // blink led pour identification
      serrureBlink();
      break;
      
    default:
      // nothing
      break;
  }

  // traite le buffer du websocket
  if (flagBufferWebsocket)
  {
    flagBufferWebsocket = false;
    handleWebsocketBuffer();
  }

  // HEARTBEAT
  if(millis() - previousMillisHB > intervalHB)
  {
    previousMillisHB = millis();
    
    // send new value to html
    sendUptime();
  }
}
/*
   ----------------------------------------------------------------------------
   FIN DU LOOP
   ----------------------------------------------------------------------------
*/





/*
   ----------------------------------------------------------------------------
   FONCTIONS ADDITIONNELLES
   ----------------------------------------------------------------------------
*/

void serrureFermee()
{
  if (uneFois)
  {
    uneFois = false;

    // on allume les led rouge
    aFastled.allLedOn(aConfig.objectConfig.couleurs[0], true);

    Serial.print(F("SERRURE FERMEE"));
    Serial.println();
  }

  // on check si une touche du clavier a ete activee
  appuiClavier();

  // check changement de parametres via le keypad
  checkChangementParametres();
}

void serrureOuverte()
{
  if (uneFois)
  {
    uneFois = false;

    // on allume les led verte
    aFastled.allLedOn(aConfig.objectConfig.couleurs[1], true);

    Serial.print(F("SERRURE OUVERTE"));
    Serial.println();
  }

  // on check si une touche du clavier a ete activee
  appuiClavier();

  // check changement de parametres via le keypad
  checkChangementParametres();
}

void serrureErreur()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("SERRURE ERREUR"));

    aFastled.animationBlink02Start(100, 1100, aConfig.objectConfig.couleurs[0], CRGB::Black);
  }

  // fin de l'animation erreur 
  if(!aFastled.isAnimActive()) 
  {
    uneFois = true;
    
    // si il y a eu trop de faux codes
    if (aConfig.objectConfig.nbErreurCode >= aConfig.objectConfig.nbErreurCodeMax)
    {
      // on bloque la serrure
      aConfig.objectConfig.statutActuel = OBJET_BLOQUE;
    }
    else
    {
      aConfig.objectConfig.statutActuel = aConfig.objectConfig.statutPrecedent;
    }
    
    writeObjectConfig();
    sendObjectConfig();
  }
}

void serrureBloquee()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("SERRURE BLOQUEE"));

    aFastled.animationBlink02Start(500, aConfig.objectConfig.delaiBlocage*1000, aConfig.objectConfig.couleurs[0], aConfig.objectConfig.couleurs[1]);
  }

  // fin de l'animation blocage
  if(!aFastled.isAnimActive()) 
  {
    uneFois = true;

    aConfig.objectConfig.statutActuel = aConfig.objectConfig.statutPrecedent;
    aConfig.objectConfig.nbErreurCode = 0;

    writeObjectConfig();
    sendObjectConfig();

    Serial.println(F("END BLOCAGE "));
  }
}

void serrureBlink()
{
  if (uneFois)
  {
    uneFois = false;
    Serial.println(F("SERRURE BLINK"));

    aFastled.animationBlink02Start(100, 3000, CRGB::Blue, CRGB::Black);
  }

  // fin de l'animation blink
  if(!aFastled.isAnimActive()) 
  {
    uneFois = true;

    aConfig.objectConfig.statutActuel = aConfig.objectConfig.statutPrecedent;

    writeObjectConfig();
    sendObjectConfig();

    Serial.println(F("END BLINK "));
  }
}

void appuiClavier()
{
  // KEYPAD
  char customKey = aKeypad->getChar();

  // une touche a ete pressee
  if (customKey != NO_KEY)
  {
    buzzer.shortBeep();

    // la touche pressee n'est pas un #
    if (customKey != '#')
    {
      Serial.print(customKey);

      // on met a jour codeSerrureActuel[] en decalant chaque caractere
      for (uint8_t i = 0; i < aConfig.objectConfig.tailleCode - 1; i++)
      {
        codeSerrureActuel[i] = codeSerrureActuel[i + 1];        
      }
      codeSerrureActuel[aConfig.objectConfig.tailleCode - 1] = customKey;
    }
    else
    // la touche pressee est un #
    {
      Serial.println();
      Serial.println(customKey);      
      
      // on compare codeSerrureActuel[] et codeSerrure[]
      Serial.println(F("actuel : attendu"));
      bool codeOK = true;
      for (uint8_t i = 0; i < aConfig.objectConfig.tailleCode; i++)
      {
        if (codeSerrureActuel[i] != aConfig.objectConfig.codeSerrure[i])
        {
          // ce caractere est faux, le code n'est pas bon
          codeOK = false;          
        }

        Serial.print(codeSerrureActuel[i]);
        Serial.print(" : ");
        Serial.println(aConfig.objectConfig.codeSerrure[i]);
      }

      // le code est correct, on change le statut de la serrure
      if (codeOK)
      {
        buzzer.shortBeep();

        aConfig.objectConfig.statutActuel = !aConfig.objectConfig.statutActuel;
        aConfig.objectConfig.nbErreurCode = 0;
      }
      // le code est incorrect
      else
      {
        // on beep long
        buzzer.longBeep();

        // on augmente le compteur de code faux
        aConfig.objectConfig.nbErreurCode += 1;

        aConfig.objectConfig.statutPrecedent = aConfig.objectConfig.statutActuel;

        // on demarre l'anim faux code
        aConfig.objectConfig.statutActuel = OBJET_ERREUR;
      }

      strlcpy(  codeSerrureActuel,
                "xxxxxxxx",
                MAX_SIZE_CODE);

      uneFois = true;

      writeObjectConfig();
      sendObjectConfig();
    }
  }
}

void serrureReconfig()
{
  if (uneFois)
  {
    uneFois = false;

    buzzer.doubleBeep();

    CRGB couleurTmp = CRGB::Blue;
    
    if (!aKeypad->getUneFoisFlag(RECONFIG_CODE))
    {
      couleurTmp = CRGB::Blue;
      modeReconfig = RECONFIG_CODE;
    }
    else if (!aKeypad->getUneFoisFlag(RECONFIG_ERREUR))
    {
      couleurTmp = CRGB::Yellow;
      modeReconfig = RECONFIG_ERREUR;
    }
    else if (!aKeypad->getUneFoisFlag(RECONFIG_DELAI))
    {
      couleurTmp = CRGB::Cyan;
      modeReconfig = RECONFIG_DELAI;
    }
    else if (!aKeypad->getUneFoisFlag(RECONFIG_TAILLE_CODE))
    {
      couleurTmp = CRGB::Purple;
      modeReconfig = RECONFIG_TAILLE_CODE;
    }

    aFastled.animationSerpent02Start(100, 8000, couleurTmp, CRGB::Black);
      

    for (uint8_t i = 0; i < 8; i++)
    {
      bufferReconfig[i]='0';
    }
    
    Serial.println(F("SERRURE RECONFIG"));    
  }

  // check touche
  // KEYPAD
  char customKey = aKeypad->getChar();

  // une touche a ete pressee
  if (customKey != NO_KEY)
  {
    buzzer.shortBeep();
    if (customKey != '#')
    {
      for (uint8_t i=0;i<7;i++)
      {
        bufferReconfig[i] = bufferReconfig[i+1];
        Serial.print(bufferReconfig[i]);
      }
      bufferReconfig[7] = customKey;
      Serial.println(bufferReconfig[7]);
    }
    else
    {
      Serial.println(F("validation"));
      
      if (modeReconfig == RECONFIG_CODE)
      // modification du code de la serrure
      {
        Serial.print(F("NOUVEAU CODE :"));
        for (uint8_t i = 0; i < aConfig.objectConfig.tailleCode; i++)
        {
          aConfig.objectConfig.codeSerrure[i] = bufferReconfig[i+8-aConfig.objectConfig.tailleCode];
          Serial.print(aConfig.objectConfig.codeSerrure[i]);
        }
        Serial.println(F(""));
      }
      else if (modeReconfig == RECONFIG_ERREUR)
      // modification du nombre max d erreur de code
      {
        // taille min=1, taille max=9
        // ascii code, 1=49, 9=57        
        if (bufferReconfig[7]>=49 && bufferReconfig[7]<=57)
        {
          aConfig.objectConfig.nbErreurCodeMax=bufferReconfig[7]-48;          
        }
        else
        {
          // valeur par defaut
          aConfig.objectConfig.nbErreurCodeMax=3;
        }

        Serial.print(F("NOUVEAU NB MAX ERREUR : "));
        Serial.println(aConfig.objectConfig.nbErreurCodeMax);
      }
      else if (modeReconfig == RECONFIG_DELAI)
      // modification du delai de blocage
      {
        uint16_t nouveauDelai=0;
        
        for (uint8_t i=0;i<8;i++)
        {
          // ascii code, 0=48, 9=57
          nouveauDelai*=10;
          if (bufferReconfig[i]>=48 && bufferReconfig[i]<=57)
          {
            nouveauDelai+=bufferReconfig[i]-48;
          }
        }
        
        aConfig.objectConfig.delaiBlocage = checkValeur(nouveauDelai,1,300);

        Serial.print(F("NOUVEAU DELAI BLOCAGE : "));
        Serial.print(aConfig.objectConfig.delaiBlocage);
        Serial.println(F(" SECONDES"));
      }
      else if (modeReconfig == RECONFIG_TAILLE_CODE)
      // modification de la taille du code
      {
        // taille min=1, taille max=8
        // ascii code, 1=49, 8=56        
        if (bufferReconfig[7]>=49 && bufferReconfig[7]<=56)
        {
          aConfig.objectConfig.tailleCode=bufferReconfig[7]-48;          
        }
        else
        {
          // valeur par defaut
          aConfig.objectConfig.tailleCode=4;
        }

        for (uint8_t i=0;i<8;i++)
        {
          codeSerrureActuel[i]='0';
        }

        Serial.print(F("NOUVELLE TAILLE CODE : "));
        Serial.println(aConfig.objectConfig.tailleCode);
      }

      // stop la reconfig
      aFastled.setAnimation(0);
    }
  }
  
  
  // fin de l'animation serpent
  if(!aFastled.isAnimActive()) 
  {
    uneFois = true;

    aConfig.objectConfig.statutActuel = aConfig.objectConfig.statutPrecedent;

    writeObjectConfig();
    sendObjectConfig();

    Serial.println(F("END RECONFIG"));
  }
}


void checkChangementParametres()
{
  if (aConfig.objectConfig.statutActuel == OBJET_OUVERT)
  {
    if (aKeypad->checkCombo('1','A',RECONFIG_CODE))
    {
      Serial.println();
      Serial.println(F("combo 1 A - changement de code"));

      uneFois = true;

      aConfig.objectConfig.statutPrecedent = aConfig.objectConfig.statutActuel;
      aConfig.objectConfig.statutActuel = OBJET_RECONFIG;
    }
  
    if (aKeypad->checkCombo('4','B',RECONFIG_ERREUR))
    {
      Serial.println();
      Serial.println(F("combo 4 B - changement du nombre max d erreur"));

      uneFois = true;

      aConfig.objectConfig.statutPrecedent = aConfig.objectConfig.statutActuel;
      aConfig.objectConfig.statutActuel = OBJET_RECONFIG;
    }
  
    if (aKeypad->checkCombo('7','C',RECONFIG_DELAI))
    {
      Serial.println();
      Serial.println(F("combo 7 C - changement du delai de blocage"));

      uneFois = true;

      aConfig.objectConfig.statutPrecedent = aConfig.objectConfig.statutActuel;
      aConfig.objectConfig.statutActuel = OBJET_RECONFIG;
    }
  
    if (aKeypad->checkCombo('*','D',RECONFIG_TAILLE_CODE))
    {
      Serial.println();
      Serial.println(F("combo * D - changement de la taille du code"));

      uneFois = true;

      aConfig.objectConfig.statutPrecedent = aConfig.objectConfig.statutActuel;
      aConfig.objectConfig.statutActuel = OBJET_RECONFIG;
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) 
{
   switch (type) 
    {
      case WS_EVT_CONNECT:
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        // send config value to html
        sendObjectConfig();
        sendNetworkConfig();
        
        // send volatile info
        sendUptime();
        sendMaxLed();
        break;
        
      case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
        
      case WS_EVT_DATA:
        handleWebSocketMessage(arg, data, len);
        break;
        
      case WS_EVT_PONG:
      case WS_EVT_ERROR:
        break;
  }
}


void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) 
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
  {
    data[len] = 0;
    sprintf(bufferWebsocket,"%s\n", (char*)data);
    Serial.print(len);
    Serial.print(bufferWebsocket);
    flagBufferWebsocket = true;
  }
}

void handleWebsocketBuffer()
{
    DynamicJsonDocument doc(JSONBUFFERSIZE);
    
    DeserializationError error = deserializeJson(doc, bufferWebsocket);
    if (error)
    {
      Serial.println(F("Failed to deserialize buffer"));
    }
    else
    {
      // write config or not
      bool writeObjectConfigFlag = false;
      bool sendObjectConfigFlag = false;
      bool writeNetworkConfigFlag = false;
      bool sendNetworkConfigFlag = false;
      
      // **********************************************
      // modif object config
      // **********************************************
      if (doc.containsKey("new_objectName"))
      {
        strlcpy(  aConfig.objectConfig.objectName,
                  doc["new_objectName"],
                  SIZE_ARRAY);

        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_objectId")) 
      {
        uint16_t tmpValeur = doc["new_objectId"];
        aConfig.objectConfig.objectId = checkValeur(tmpValeur,1,1000);

        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_groupId")) 
      {
        uint16_t tmpValeur = doc["new_groupId"];
        aConfig.objectConfig.groupId = checkValeur(tmpValeur,1,1000);
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_activeLeds")) 
      {
        aFastled.allLedOff();
        
        uint16_t tmpValeur = doc["new_activeLeds"];
        aConfig.objectConfig.activeLeds = checkValeur(tmpValeur,1,NB_LEDS_MAX);
        aFastled.setNbLed(aConfig.objectConfig.activeLeds);
        
        uneFois = true;

        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_brightness"))
      {
        uint16_t tmpValeur = doc["new_brightness"];
        aConfig.objectConfig.brightness = checkValeur(tmpValeur,0,255);
        aFastled.setBrightness(aConfig.objectConfig.brightness);
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_intervalScintillement"))
      {
        uint16_t tmpValeur = doc["new_intervalScintillement"];
        aConfig.objectConfig.intervalScintillement = checkValeur(tmpValeur,0,1000);
        aFastled.setIntervalControlBrightness(aConfig.objectConfig.intervalScintillement);
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }
      
      if (doc.containsKey("new_scintillementOnOff"))
      {
        uint16_t tmpValeur = doc["new_scintillementOnOff"];
        aConfig.objectConfig.scintillementOnOff = checkValeur(tmpValeur,0,1);
        aFastled.setControlBrightness(aConfig.objectConfig.scintillementOnOff);
        
        if (aConfig.objectConfig.scintillementOnOff == 0)
        {
          FastLED.setBrightness(aConfig.objectConfig.brightness);
        }
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_tailleCode")) 
      {
        uint16_t tmpValeur = doc["new_tailleCode"];
        aConfig.objectConfig.tailleCode = checkValeur(tmpValeur,1,8);
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_codeSerrure")) 
      {
        strlcpy(  aConfig.objectConfig.codeSerrure,
                  doc["new_codeSerrure"],
                  MAX_SIZE_CODE);

        // check for unsupported char
        const char listeCheck[] = "0123456789ABCD*";
        checkCharacter(aConfig.objectConfig.codeSerrure, listeCheck, '0');
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_nbErreurCodeMax")) 
      {
        uint16_t tmpValeur = doc["new_nbErreurCodeMax"];
        aConfig.objectConfig.nbErreurCodeMax = checkValeur(tmpValeur,1,50);
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }
      
      if (doc.containsKey("new_delaiBlocage")) 
      {
        uint16_t tmpValeur = doc["new_delaiBlocage"];
        aConfig.objectConfig.delaiBlocage = checkValeur(tmpValeur,5,300);
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }
      
      if (doc.containsKey("new_statutActuel"))
      {
        uint16_t tmpValeur = doc["new_statutActuel"];
        aConfig.objectConfig.statutPrecedent=aConfig.objectConfig.statutActuel;
        aConfig.objectConfig.statutActuel=tmpValeur;

        aFastled.setAnimation(0);
    
        uneFois=true;
        
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if ( doc.containsKey("new_resetErreur") && doc["new_resetErreur"]==1 )
      {
        Serial.println(F("Reset erreurs"));
        aConfig.objectConfig.nbErreurCode = 0;

        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }

      if (doc.containsKey("new_couleurs")) 
      {
        JsonArray newCouleur = doc["new_couleurs"];

        uint8_t i = newCouleur[0];
        char newColorStr[8];
        strlcpy(newColorStr, newCouleur[1], 8);
          
        uint8_t r;
        uint8_t g;
        uint8_t b;
          
        convertStrToRGB(newColorStr, &r, &g, &b);
        aConfig.objectConfig.couleurs[i].red=r;
        aConfig.objectConfig.couleurs[i].green=g;
        aConfig.objectConfig.couleurs[i].blue=b;
        
        uneFois=true;
          
        writeObjectConfigFlag = true;
        sendObjectConfigFlag = true;
      }
        
      // **********************************************
      // modif network config
      // **********************************************
      if (doc.containsKey("new_apName")) 
      {
        strlcpy(  aConfig.networkConfig.apName,
                  doc["new_apName"],
                  sizeof(aConfig.networkConfig.apName));
      
        // check for unsupported char
        const char listeCheck[] = "ABCDEFGHIJKLMNOPQRSTUVWYXZ0123456789_-";
        checkCharacter(aConfig.networkConfig.apName, listeCheck, 'A');
        
        writeNetworkConfigFlag = true;
        sendNetworkConfigFlag = true;
      }
      
      if (doc.containsKey("new_apPassword")) 
      {
        strlcpy(  aConfig.networkConfig.apPassword,
                  doc["new_apPassword"],
                  sizeof(aConfig.networkConfig.apPassword));
      
        writeNetworkConfigFlag = true;
        sendNetworkConfigFlag = true;
      }
      
      if (doc.containsKey("new_apIP")) 
      {
        char newIPchar[16] = "";
      
        strlcpy(  newIPchar,
                  doc["new_apIP"],
                  sizeof(newIPchar));
      
        IPAddress newIP;
        if (newIP.fromString(newIPchar))
        {
          Serial.println("valid IP");
          aConfig.networkConfig.apIP = newIP;
      
          writeNetworkConfigFlag = true;
        }
        
        sendNetworkConfigFlag = true;
      }
      
      if (doc.containsKey("new_apNetMsk")) 
      {
        char newNMchar[16] = "";
      
        strlcpy(  newNMchar,
                  doc["new_apNetMsk"],
                  sizeof(newNMchar));
      
        IPAddress newNM;
        if (newNM.fromString(newNMchar)) 
        {
          Serial.println("valid netmask");
          aConfig.networkConfig.apNetMsk = newNM;
      
          writeNetworkConfigFlag = true;
        }
      
        sendNetworkConfigFlag = true;
      }
      
      // actions sur le esp8266
      if ( doc.containsKey("new_restart") && doc["new_restart"]==1 )
      {
        Serial.println(F("RESTART RESTART RESTART"));
        ESP.restart();
      }
      
      if ( doc.containsKey("new_refresh") && doc["new_refresh"]==1 )
      {
        Serial.println(F("REFRESH"));
      
        sendObjectConfigFlag = true;
        sendNetworkConfigFlag = true;
      }
      
      if ( doc.containsKey("new_defaultObjectConfig") && doc["new_defaultObjectConfig"]==1 )
      {
        aConfig.writeDefaultObjectConfig("/config/objectconfig.txt");
        Serial.println(F("reset to default object config"));
      
        aFastled.allLedOff();
        aFastled.setNbLed(aConfig.objectConfig.activeLeds);          
        aFastled.setControlBrightness(aConfig.objectConfig.scintillementOnOff);
        aFastled.setIntervalControlBrightness(aConfig.objectConfig.intervalScintillement);
        
        sendObjectConfigFlag = true;
        uneFois = true;
      }
      
      if ( doc.containsKey("new_defaultNetworkConfig") && doc["new_defaultNetworkConfig"]==1 )
      {
        aConfig.writeDefaultNetworkConfig("/config/networkconfig.txt");
        Serial.println(F("reset to default network config"));          
        
        sendNetworkConfigFlag = true;
      }
      
      // modif config
      // write object config
      if (writeObjectConfigFlag)
      {
        writeObjectConfig();
      
        // update statut
        uneFois = true;
      }
      
      // resend object config
      if (sendObjectConfigFlag)
      {
        sendObjectConfig();
      }
      
      // write network config
      if (writeNetworkConfigFlag)
      {
        writeNetworkConfig();
      }
      
      // resend network config
      if (sendNetworkConfigFlag)
      {
        sendNetworkConfig();
      }
    }
 
    // clear json buffer
    doc.clear();
}

void notFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not found");
}

void checkCharacter(char* toCheck, const char* allowed, char replaceChar)
{
  for (uint8_t i = 0; i < strlen(toCheck); i++)
  {
    if (!strchr(allowed, toCheck[i]))
    {
      toCheck[i]=replaceChar;
    }
    Serial.print(toCheck[i]);
  }
  Serial.println("");
}

uint16_t checkValeur(uint16_t valeur, uint16_t minValeur, uint16_t maxValeur)
{
  return(min(max(valeur,minValeur), maxValeur));
}

void sendObjectConfig()
{
  aConfig.stringJsonFile("/config/objectconfig.txt", bufferToSend, BUFFERSENDSIZE);
  ws.textAll(bufferToSend);
}

void writeObjectConfig()
{
  aConfig.writeObjectConfig("/config/objectconfig.txt");
}

void sendNetworkConfig()
{
  aConfig.stringJsonFile("/config/networkconfig.txt", bufferToSend, BUFFERSENDSIZE);
  ws.textAll(bufferToSend);
}

void writeNetworkConfig()
{
  aConfig.writeNetworkConfig("/config/networkconfig.txt");
}

void sendUptime()
{
  uint32_t now = millis() / 1000;
  uint16_t days = now / 86400;
  uint16_t hours = (now%86400) / 3600;
  uint16_t minutes = (now%3600) / 60;
  uint16_t seconds = now % 60;
    
  char toSend[100];
  snprintf(toSend, 100, "{\"uptime\":\"%id %ih %im %is\"}", days, hours, minutes, seconds);

  ws.textAll(toSend);
  //Serial.println(toSend);
}

void sendStatut()
{
  char toSend[100];
  snprintf(toSend, 100, "{\"statutActuel\":%i}", aConfig.objectConfig.statutActuel); 

  ws.textAll(toSend);
}

void sendMaxLed()
{
  char toSend[20];
  snprintf(toSend, 20, "{\"maxLed\":%i}", NB_LEDS_MAX);
  
  ws.textAll(toSend);
}

void convertStrToRGB(const char * source, uint8_t* r, uint8_t* g, uint8_t* b)
{
  uint32_t  number = (uint32_t) strtol( &source[1], NULL, 16);
  
  // Split them up into r, g, b values
  *r = number >> 16;
  *g = number >> 8 & 0xFF;
  *b = number & 0xFF;
}

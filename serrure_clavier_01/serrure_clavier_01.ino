/*
   ----------------------------------------------------------------------------
   TECHNOLARP - https://technolarp.github.io/
   SERRURE CLAVIER 01 - https://github.com/technolarp/serrure_clavier_01
   version 1.0 - 09/2021
   ----------------------------------------------------------------------------
*/

/*
   ----------------------------------------------------------------------------
   Pour ce montage, vous avez besoin de 
   1 clavier matricel 4*4 
   1 carte PCF8574 I2C
   1 ou + led neopixel
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
#include <Arduino.h>

// WIFI
//#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

//const char* ssid = "SERRURE";
//const char* password = "";

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

// TASK SCHEDULER
#define _TASK_OO_CALLBACKS
#include <TaskScheduler.h>
Scheduler globalScheduler;

// LED RGB
#include <technolarp_fastled.h>
M_fastled* aFastled;

// KEYPAD
#include <technolarp_keypad.h>
M_keypad* aKeypad;

// BUZZER
#define PIN_BUZZER D8
#include <technolarp_buzzer.h>
M_buzzer* buzzer;

// CONFIG
#include "config.h"
M_config aConfig;

// CODE ACTUEL DE LA SERRURE
char codeSerrureActuel[8] = {'0', '0', '0', '0', '0', '0', '0', '0'};

// STATUTS DE LA SERRURE
enum {
  SERRURE_OUVERTE = 0,
  SERRURE_FERMEE = 1,
  SERRURE_BLOQUEE = 2,
  SERRURE_ERREUR = 3,
  SERRURE_RECONFIG = 4
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
  Serial.println(F("version 1.0 - 09/2021"));
  Serial.println(F("----------------------------------------------------------------------------"));
  
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

  Serial.println("------------------------------------");
  Serial.println(aConfig.networkConfig.apName);
  Serial.println(aConfig.networkConfig.apPassword);
  Serial.println(aConfig.networkConfig.apIP);
  Serial.println(aConfig.networkConfig.apNetMsk);
  Serial.println("------------------------------------");

  // LED RGB
  aFastled = new M_fastled(&globalScheduler);
  aFastled->setNbLed(aConfig.objectConfig.activeLeds);

  // animation led de depart
  
  aFastled->allLedOff();
  for (int i = 0; i < aConfig.objectConfig.activeLeds * 2; i++)
  {
    aFastled->ledOn(i % aConfig.objectConfig.activeLeds, CRGB::Blue);
    delay(50);
    aFastled->ledOn(i % aConfig.objectConfig.activeLeds, CRGB::Black);
  }
  aFastled->allLedOff();
  

  // KEYPAD
  aKeypad = new M_keypad();

  // CHECK RESET CONFIG
  
  if (aKeypad->checkReset())
  {
    for (int i = 0; i < aConfig.objectConfig.activeLeds; i++)
    {
      aFastled->setLed(i, CRGB::Yellow);
    }
    aFastled->ledShow();
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET CONFIG !!!"));
    Serial.println(F(""));
    aConfig.writeDefaultObjectConfig("/config/objectconfig.txt");
    aConfig.printJsonFile("/config/objectconfig.txt");

    delay(1000);
  }

  // WIFI
  //IPAddress apIP(192,168,1,1);
  //IPAddress apNetMsk(255, 255, 255, 0);
  
  // AP MODE
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(aConfig.networkConfig.apIP, aConfig.networkConfig.apIP, aConfig.networkConfig.apNetMsk);
  WiFi.softAP(aConfig.networkConfig.apName);

  // WEB SERVER
  // Print ESP Local IP Address
  Serial.print(F("localIP: "));
  Serial.println(WiFi.localIP());
  Serial.print(F("softAPIP: "));
  Serial.println(WiFi.softAPIP());

  // Route for root / web page
  /*
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    //request->send_P(200, "text/html", index_html, processor);
    request->send(LittleFS, "/www/index.html");
  });
  */
  
  /*
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    request->send(LittleFS, "/www/index.html", "text/html");
  });
  */
  
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile("index.html").setTemplateProcessor(processor);
  server.onNotFound(notFound);

  // Start server
  server.begin();

  // BUZZER
  buzzer = new M_buzzer(PIN_BUZZER, &globalScheduler);
  buzzer->doubleBeep();

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
  // manage task scheduler
  globalScheduler.execute();

  // gerer le statut de la serrure
  switch (aConfig.objectConfig.statutSerrureActuel)
  {
    case SERRURE_FERMEE:
      // la serrure est fermee
      serrureFermee();
      break;

    case SERRURE_OUVERTE:
      // la serrure est ouverte
      serrureOuverte();
      break;

    case SERRURE_BLOQUEE:
      // la serrure est bloquee
      serrureBloquee();
      break;

    case SERRURE_ERREUR:
      // un code incorrect a ete entrer
      serrureErreur();
      break;

    case SERRURE_RECONFIG:
      // un parametre doit etre modifie
      serrureReconfig();
      break;

    default:
      // nothing
      break;
  }

  // check changement de parametres via le keypad
  checkChangementParametres();
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
    aFastled->allLedOff();
    for (int i = 0; i < aConfig.objectConfig.activeLeds; i++)
    {
      aFastled->setLed(i, CRGB::Red);
    }
    aFastled->ledShow();

    Serial.print(F("SERRURE FERMEE"));
    Serial.println();
  }

  // on check si une touche du clavier a ete activee
  appuiClavier();
}

void serrureOuverte()
{
  if (uneFois)
  {
    uneFois = false;

    // on allume les led verte
    aFastled->allLedOff();
    for (int i = 0; i < aConfig.objectConfig.activeLeds; i++)
    {
      aFastled->setLed(i, CRGB::Green);
    }
    aFastled->ledShow();

    Serial.print(F("SERRURE OUVERTE"));
    Serial.println();
  }

  // on check si une touche du clavier a ete activee
  appuiClavier();
}

void serrureErreur()
{
  if (!aFastled->isEnabled() && uneFois)
  {
    uneFois = false;
    aFastled->startAnimSerrureErreur(10, 100);
  }

  if (!aFastled->isAnimActive())
  {
    Serial.println(F("END TASK ERREUR"));

    uneFois = true;

    // si il y a eu trop de faux codes
    if (aConfig.objectConfig.nbErreurCode >= aConfig.objectConfig.nbErreurCodeMax)
    {
      // on bloque la serrure
      aConfig.objectConfig.statutSerrureActuel = SERRURE_BLOQUEE;
    }
    else
    {
      aConfig.objectConfig.statutSerrureActuel = aConfig.objectConfig.statutSerrurePrecedent;
    }

    // ecrire la config sur littleFS
    aConfig.writeObjectConfig("/config/objectconfig.txt");
  }
}

void serrureBloquee()
{
  if (!aFastled->isEnabled() && uneFois)
  {
    uneFois = false;
    aFastled->startAnimSerrureBloquee(aConfig.objectConfig.intervalBlocage*2, 500);
  }

  if (!aFastled->isAnimActive())
  {
    Serial.print(F("END TASK BLOCAGE "));
    Serial.println();
    aConfig.objectConfig.statutSerrureActuel = aConfig.objectConfig.statutSerrurePrecedent;
    aConfig.objectConfig.nbErreurCode = 0;

    uneFois = true;

    // ecrire la config sur littleFS
    aConfig.writeObjectConfig("/config/objectconfig.txt");
  }
}

void serrureReconfig()
{
  if (!aFastled->isEnabled() && uneFois)
  {
    uneFois = false;
    
    if (!aKeypad->getUneFoisFlag(RECONFIG_CODE))
    {
      aFastled->startAnimSerpent(0, 50, 50, CRGB::Blue);
      modeReconfig = RECONFIG_CODE;
    }
    else if (!aKeypad->getUneFoisFlag(RECONFIG_ERREUR))
    {
      aFastled->startAnimSerpent(0, 50, 50, CRGB::Yellow);
      modeReconfig = RECONFIG_ERREUR;
    }
    else if (!aKeypad->getUneFoisFlag(RECONFIG_DELAI))
    {
      aFastled->startAnimSerpent(0, 50, 50, CRGB::Cyan);
      modeReconfig = RECONFIG_DELAI;
    }
    else if (!aKeypad->getUneFoisFlag(RECONFIG_TAILLE_CODE))
    {
      aFastled->startAnimSerpent(0, 50, 50, CRGB::Purple);
      modeReconfig = RECONFIG_TAILLE_CODE;
    }

    for (int i = 0; i < 8; i++)
    {
      bufferReconfig[i]='0';
    }
    
    Serial.println(F("START TASK RECONFIG"));
  }

  // check touche
  // KEYPAD
  char customKey = aKeypad->getChar();

  // une touche a ete pressee
  if (customKey != NO_KEY)
  {
    if (customKey != '#')
    {
      for (int i=0;i<7;i++)
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
        for (int i = 0; i < aConfig.objectConfig.tailleCode; i++)
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
        
        for (int i=0;i<8;i++)
        {
          // ascii code, 0=48, 9=57
          nouveauDelai*=10;
          if (bufferReconfig[i]>=48 && bufferReconfig[i]<=57)
          {
            nouveauDelai+=bufferReconfig[i]-48;
            Serial.println(nouveauDelai);
          }
        }
        
        nouveauDelai=max<int>(nouveauDelai,1);
        nouveauDelai=min<int>(nouveauDelai,300);

        aConfig.objectConfig.intervalBlocage=nouveauDelai;

        Serial.print(F("NOUVEAU DELAI BLOCAGE : "));
        Serial.print(aConfig.objectConfig.intervalBlocage);
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

        for (int i=0;i<8;i++)
        {
          codeSerrureActuel[i]='0';
        }

        Serial.print(F("NOUVELLE TAILLE CODE : "));
        Serial.println(aConfig.objectConfig.tailleCode);
      }

      // stop la reconfig
      aFastled->disable();
    }
  }

  // Check fin de la reconfig
  if (!aFastled->isAnimActive())
  {
    Serial.print(F("END TASK RECONFIG"));
    Serial.println();
    aConfig.objectConfig.statutSerrureActuel = aConfig.objectConfig.statutSerrurePrecedent;
    uneFois = true;

    // ecrire la config sur littleFS
    aConfig.writeObjectConfig("/config/objectconfig.txt");
    aConfig.printJsonFile("/config/objectconfig.txt");
  }
}

void appuiClavier()
{
  // KEYPAD
  char customKey = aKeypad->getChar();

  // une touche a ete pressee
  if (customKey != NO_KEY)
  {
    buzzer->shortBeep();

    // la touche pressee n'est pas un #
    if (customKey != '#')
    {
      Serial.print(customKey);

      // on met a jour codeSerrureActuel[] en decalant chaque caractere
      for (int i = 0; i < aConfig.objectConfig.tailleCode - 1; i++)
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
      for (int i = 0; i < aConfig.objectConfig.tailleCode; i++)
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
        buzzer->shortBeep();

        aConfig.objectConfig.statutSerrureActuel = !aConfig.objectConfig.statutSerrureActuel;
        aConfig.objectConfig.nbErreurCode = 0;

        for (int i = 0; i < aConfig.objectConfig.tailleCode; i++)
        {
          codeSerrureActuel[i] = '0';
        }
      }
      // le code est incorrect
      else
      {
        // on beep long
        buzzer->longBeep();

        // on augmente le compteur de code faux
        aConfig.objectConfig.nbErreurCode += 1;

        aConfig.objectConfig.statutSerrurePrecedent = aConfig.objectConfig.statutSerrureActuel;

        // on demarre l'anim faux code
        aConfig.objectConfig.statutSerrureActuel = SERRURE_ERREUR;
      }

      uneFois = true;

      // ecrire la config sur littleFS
      aConfig.writeObjectConfig("/config/objectconfig.txt");
    }
  }
}

void checkChangementParametres()
{
if (aConfig.objectConfig.statutSerrureActuel == SERRURE_OUVERTE)
{
    if (aKeypad->checkCombo('1','A',RECONFIG_CODE))
    {
      Serial.println();
      Serial.println(F("combo 1 A - changement de code"));

      uneFois = true;

      aConfig.objectConfig.statutSerrurePrecedent = aConfig.objectConfig.statutSerrureActuel;
      aConfig.objectConfig.statutSerrureActuel = SERRURE_RECONFIG;
    }
  
    if (aKeypad->checkCombo('4','B',RECONFIG_ERREUR))
    {
      Serial.println();
      Serial.println(F("combo 4 B - changement du nombre max d erreur"));

      uneFois = true;

      aConfig.objectConfig.statutSerrurePrecedent = aConfig.objectConfig.statutSerrureActuel;
      aConfig.objectConfig.statutSerrureActuel = SERRURE_RECONFIG;
    }
  
    if (aKeypad->checkCombo('7','C',RECONFIG_DELAI))
    {
      Serial.println();
      Serial.println(F("combo 7 C - changement du delai de blocage"));

      uneFois = true;

      aConfig.objectConfig.statutSerrurePrecedent = aConfig.objectConfig.statutSerrureActuel;
      aConfig.objectConfig.statutSerrureActuel = SERRURE_RECONFIG;
    }
  
    if (aKeypad->checkCombo('*','D',RECONFIG_TAILLE_CODE))
    {
      Serial.println();
      Serial.println(F("combo * D - changement de la taille du code"));

      uneFois = true;

      aConfig.objectConfig.statutSerrurePrecedent = aConfig.objectConfig.statutSerrureActuel;
      aConfig.objectConfig.statutSerrureActuel = SERRURE_RECONFIG;
    }
  }
}




String processor(const String& var)
{
  if(var == "IP")
    return(WiFi.softAPIP().toString());
  return String();
}

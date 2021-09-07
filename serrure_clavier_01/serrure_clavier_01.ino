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

#include <arduino.h>

// TASK SCHEDULER
#define _TASK_OO_CALLBACKS
#include <TaskScheduler.h>
Scheduler globalScheduler;

// LED RGB
#include <technolarp_neopixel.h>
M_neopixel* neopixels;

// KEYPAD
#include "technolarp_keypad.h"
M_keypad* aKeypad;

// BUZZER
#define PIN_BUZZER D8
#include <technolarp_buzzer.h>
M_buzzer* buzzer;

// CONFIG
#include "config.h"
M_config m_config;

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
  
  // CONFIG
  Serial.println(F(""));
  Serial.println(F(""));
  m_config.mountFS();
  m_config.listDir("/");
  m_config.listDir("/config");
  m_config.printJsonFile("/config/objectconfig.txt");
  m_config.readObjectConfig("/config/objectconfig.txt");


  // LED RGB
  neopixels = new M_neopixel(&globalScheduler);
  neopixels->setNbLed(m_config.myConfig.activeLeds);

  // animation led de depart
  neopixels->allLedOff();
  for (int i = 0; i < m_config.myConfig.activeLeds * 2; i++)
  {
    neopixels->ledOn(i % m_config.myConfig.activeLeds, CRGB::Blue);
    delay(50);
    neopixels->ledOn(i % m_config.myConfig.activeLeds, CRGB::Black);
  }
  neopixels->allLedOff();

  // KEYPAD
  aKeypad = new M_keypad();

  // CHECK RESET CONFIG
  if (aKeypad->checkReset())
  {
    for (int i = 0; i < m_config.myConfig.activeLeds; i++)
    {
      neopixels->setLed(i, CRGB::Yellow);
    }
    neopixels->ledShow();
    
    Serial.println(F(""));
    Serial.println(F("!!! RESET CONFIG !!!"));
    Serial.println(F(""));
    m_config.writeDefaultObjectConfig("/config/objectconfig.txt");
    m_config.printJsonFile("/config/objectconfig.txt");

    delay(1000);
  }

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
  switch (m_config.myConfig.statutSerrureActuel)
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
    neopixels->allLedOff();
    for (int i = 0; i < m_config.myConfig.activeLeds; i++)
    {
      neopixels->setLed(i, CRGB::Red);
    }
    neopixels->ledShow();

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
    neopixels->allLedOff();
    for (int i = 0; i < m_config.myConfig.activeLeds; i++)
    {
      neopixels->setLed(i, CRGB::Green);
    }
    neopixels->ledShow();

    Serial.print(F("SERRURE OUVERTE"));
    Serial.println();
  }

  // on check si une touche du clavier a ete activee
  appuiClavier();
}

void serrureErreur()
{
  if (!neopixels->isEnabled() && uneFois)
  {
    uneFois = false;
    neopixels->startAnimSerrureErreur(10, 100);
  }

  if (!neopixels->isAnimActive())
  {
    Serial.println(F("END TASK ERREUR"));

    uneFois = true;

    // si il y a eu trop de faux codes
    if (m_config.myConfig.nbErreurCode >= m_config.myConfig.nbErreurCodeMax)
    {
      // on bloque la serrure
      m_config.myConfig.statutSerrureActuel = SERRURE_BLOQUEE;
    }
    else
    {
      m_config.myConfig.statutSerrureActuel = m_config.myConfig.statutSerrurePrecedent;
    }

    // ecrire la config sur littleFS
    m_config.writeObjectConfig("/config/objectconfig.txt");
  }
}

void serrureBloquee()
{
  if (!neopixels->isEnabled() && uneFois)
  {
    uneFois = false;
    neopixels->startAnimSerrureBloquee(m_config.myConfig.intervalBlocage*2, 500);
  }

  if (!neopixels->isAnimActive())
  {
    Serial.print(F("END TASK BLOCAGE "));
    Serial.println();
    m_config.myConfig.statutSerrureActuel = m_config.myConfig.statutSerrurePrecedent;
    m_config.myConfig.nbErreurCode = 0;

    uneFois = true;

    // ecrire la config sur littleFS
    m_config.writeObjectConfig("/config/objectconfig.txt");
  }
}

void serrureReconfig()
{
  if (!neopixels->isEnabled() && uneFois)
  {
    uneFois = false;
    
    if (!aKeypad->getUneFoisFlag(0))
    {
      neopixels->startAnimSerpent(0, 50, 50, CRGB::Blue);
      modeReconfig = 0;
    }
    else if (!aKeypad->getUneFoisFlag(1))
    {
      neopixels->startAnimSerpent(0, 50, 50, CRGB::Yellow);
      modeReconfig = 1;
    }
    else if (!aKeypad->getUneFoisFlag(2))
    {
      neopixels->startAnimSerpent(0, 50, 50, CRGB::Cyan);
      modeReconfig = 2;
    }
    else if (!aKeypad->getUneFoisFlag(3))
    {
      neopixels->startAnimSerpent(0, 50, 50, CRGB::Purple);
      modeReconfig = 3;
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
      
      if (modeReconfig == 0)
      // modification du code de la serrure
      {
        Serial.print(F("NOUVEAU CODE :"));
        for (int i = 0; i < m_config.myConfig.tailleCode; i++)
        {
          m_config.myConfig.codeSerrure[i] = bufferReconfig[i+8-m_config.myConfig.tailleCode];
          Serial.print(m_config.myConfig.codeSerrure[i]);
        }
        Serial.println(F(""));
      }
      else if (modeReconfig == 1)
      // modification du nombre max d erreur de code
      {
        // taille min=1, taille max=9
        // ascii code, 1=49, 9=57        
        if (bufferReconfig[7]>=49 && bufferReconfig[7]<=57)
        {
          m_config.myConfig.nbErreurCodeMax=bufferReconfig[7]-48;          
        }
        else
        {
          // valeur par defaut
          m_config.myConfig.nbErreurCodeMax=3;
        }

        Serial.print(F("NOUVEAU NB MAX ERREUR : "));
        Serial.println(m_config.myConfig.nbErreurCodeMax);
      }
      else if (modeReconfig == 2)
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

        m_config.myConfig.intervalBlocage=nouveauDelai;

        Serial.print(F("NOUVEAU DELAI BLOCAGE : "));
        Serial.print(m_config.myConfig.intervalBlocage);
        Serial.println(F(" SECONDES"));
      }
      else if (modeReconfig == 3)
      // modification de la taille du code
      {
        // taille min=1, taille max=8
        // ascii code, 1=49, 8=56        
        if (bufferReconfig[7]>=49 && bufferReconfig[7]<=56)
        {
          m_config.myConfig.tailleCode=bufferReconfig[7]-48;          
        }
        else
        {
          // valeur par defaut
          m_config.myConfig.tailleCode=4;
        }

        for (int i=0;i<8;i++)
        {
          codeSerrureActuel[i]='0';
        }

        Serial.print(F("NOUVELLE TAILLE CODE : "));
        Serial.println(m_config.myConfig.tailleCode);
      }

      // stop la reconfig
      neopixels->disable();
    }
  }

  // Check fin de la reconfig
  if (!neopixels->isAnimActive())
  {
    Serial.print(F("END TASK RECONFIG"));
    Serial.println();
    m_config.myConfig.statutSerrureActuel = m_config.myConfig.statutSerrurePrecedent;
    uneFois = true;

    // ecrire la config sur littleFS
    m_config.writeObjectConfig("/config/objectconfig.txt");
    m_config.printJsonFile("/config/objectconfig.txt");
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
      for (int i = 0; i < m_config.myConfig.tailleCode - 1; i++)
      {
        codeSerrureActuel[i] = codeSerrureActuel[i + 1];        
      }
      codeSerrureActuel[m_config.myConfig.tailleCode - 1] = customKey;
    }
    else
      // la touche pressee est un #
    {
      Serial.println();
      Serial.println(customKey);      
      
      // on compare codeSerrureActuel[] et codeSerrure[]
      Serial.println(F("actuel : attendu"));
      bool codeOK = true;
      for (int i = 0; i < m_config.myConfig.tailleCode; i++)
      {
        if (codeSerrureActuel[i] != m_config.myConfig.codeSerrure[i])
        {
          // ce caractere est faux, le code n'est pas bon
          codeOK = false;          
        }

        Serial.print(codeSerrureActuel[i]);
        Serial.print(" : ");
        Serial.println(m_config.myConfig.codeSerrure[i]);
      }

      // le code est correct, on change le statut de la serrure
      if (codeOK)
      {
        buzzer->shortBeep();

        m_config.myConfig.statutSerrureActuel = !m_config.myConfig.statutSerrureActuel;
        m_config.myConfig.nbErreurCode = 0;

        for (int i = 0; i < m_config.myConfig.tailleCode; i++)
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
        m_config.myConfig.nbErreurCode += 1;

        m_config.myConfig.statutSerrurePrecedent = m_config.myConfig.statutSerrureActuel;

        // on demarre l'anim faux code
        m_config.myConfig.statutSerrureActuel = SERRURE_ERREUR;
      }

      uneFois = true;

      // ecrire la config sur littleFS
      m_config.writeObjectConfig("/config/objectconfig.txt");
    }
  }
}

void checkChangementParametres()
{
if (m_config.myConfig.statutSerrureActuel == SERRURE_OUVERTE)
{
    if (aKeypad->checkCombo('1','A',0))
    {
      Serial.println();
      Serial.println(F("combo 1 A - changement de code"));

      uneFois = true;

      m_config.myConfig.statutSerrurePrecedent = m_config.myConfig.statutSerrureActuel;
      m_config.myConfig.statutSerrureActuel = SERRURE_RECONFIG;
    }
  
    if (aKeypad->checkCombo('4','B',1))
    {
      Serial.println();
      Serial.println(F("combo 4 B - changement du nombre max d erreur"));

      uneFois = true;

      m_config.myConfig.statutSerrurePrecedent = m_config.myConfig.statutSerrureActuel;
      m_config.myConfig.statutSerrureActuel = SERRURE_RECONFIG;
    }
  
    if (aKeypad->checkCombo('7','C',2))
    {
      Serial.println();
      Serial.println(F("combo 7 C - changement du delai de blocage"));

      uneFois = true;

      m_config.myConfig.statutSerrurePrecedent = m_config.myConfig.statutSerrureActuel;
      m_config.myConfig.statutSerrureActuel = SERRURE_RECONFIG;
    }
  
    if (aKeypad->checkCombo('*','D',3))
    {
      Serial.println();
      Serial.println(F("combo * D - changement de la taille du code"));

      uneFois = true;

      m_config.myConfig.statutSerrurePrecedent = m_config.myConfig.statutSerrureActuel;
      m_config.myConfig.statutSerrureActuel = SERRURE_RECONFIG;
    }
  }
}

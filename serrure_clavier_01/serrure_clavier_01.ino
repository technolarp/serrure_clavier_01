/*
   ----------------------------------------------------------------------------
   For this exemple you will need
   1 4*4 matrix keypad and 1 i2c PCF8574 board
   1 neopixel ring
   1 piezo buzzer
   ----------------------------------------------------------------------------
*/

/*
   ----------------------------------------------------------------------------
   PINOUT
   A0     PHOTORESISTOR + pulldown / POTENTIOMETER
   D0     NEOPIXEL
   D1     I2C SCL
   D2     I2C SDA
   D3     TM1637 DIO
   D4     BUILTIN LED
   D5     TM1637 CLK
   D6     ROTARY_CLK
   D7     ROTARY_DT
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
char codeSerrureActuel[4] = {'0', '0', '0', '0'};

// STATUTS DE LA SERRURE
enum {
  SERRURE_OUVERTE = 0,
  SERRURE_FERMEE = 1,
  SERRURE_BLOQUEE = 2,
  SERRURE_ERREUR = 3
};

// DIVERS
unsigned long int refTime = 0;
bool uneFois = true;

/*
   ----------------------------------------------------------------------------
   SETUP
   ----------------------------------------------------------------------------
*/
void setup()
{
  Serial.begin(115200);

  // CONFIG
  delay(500);
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

  if (aKeypad->checkReset())
  {
    for (int i = 0; i < m_config.myConfig.activeLeds; i++)
    {
      neopixels->setLed(i, CRGB::Yellow);
    }
    neopixels->ledShow();
    
    Serial.println("");
    Serial.println("!!! RESET CONFIG !!!");
    Serial.println("");
    m_config.writeDefaultObjectConfig("/config/objectconfig.txt");
    m_config.printJsonFile("/config/objectconfig.txt");
    //m_config.readObjectConfig("/config/objectconfig.txt");

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
  }

  // on check si une touche du clavier a ete activee
  appuiClavier();
}

void serrureBloquee()
{
  if (!neopixels->isEnabled())
  {
    refTime = millis();    
    neopixels->startAnimSerrureBloquee(m_config.myConfig.intervalBlocage*2, 500);
  }

  if (neopixels->isLastIteration())
  {
    Serial.print(F("END TASK BLOCAGE : "));
    Serial.print(millis() - refTime);
    Serial.println();
    m_config.myConfig.statutSerrureActuel = m_config.myConfig.statutSerrurePrecedent;
    m_config.myConfig.nbErreurCode = 0;

    // ecrire la config sur littleFS
    m_config.writeObjectConfig("/config/objectconfig.txt");
  }
}

void serrureErreur()
{
  if (!neopixels->isEnabled())
  {
    neopixels->startAnimSerrureErreur(10, 100);
  }

  if (neopixels->isLastIteration())
  {
    Serial.println(F("END TASK ERREUR"));

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
      Serial.println("actuel : attendu");
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

      // ecrire laconfig sur littleFS
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
      Serial.println("combo 1 A");
    }
  
    if (aKeypad->checkCombo('4','B',1))
    {
      Serial.println();
      Serial.println("combo 4 B");
    }
  
    if (aKeypad->checkCombo('7','C',2))
    {
      Serial.println();
      Serial.println("combo 7 C");
    }
  
    if (aKeypad->checkCombo('*','D',3))
    {
      Serial.println();
      Serial.println("combo * D");
    }
  }
}

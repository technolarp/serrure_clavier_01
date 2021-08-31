/*
 * ----------------------------------------------------------------------------
 * For this exemple you will need
 * 1 4*4 matrix keypad and 1 i2c PCF8574 board
 * 1 neopixel ring
 * ----------------------------------------------------------------------------
 */

/*
 * ----------------------------------------------------------------------------
 * PINOUT
 * A0     PHOTORESISTOR + pulldown / POTENTIOMETER
 * D0     NEOPIXEL
 * D1     I2C SCL     
 * D2     I2C SDA     
 * D3     TM1637 DIO  /  MRF522 RST_PIN
 * D4     BUILTIN LED  /  MRF522 SS_PIN-SDA 
 * D5     TM1637 CLK  /  MRF522 SCK
 * D6     ROTARY_CLK  /  MRF522 MISO
 * D7     ROTARY_DT  /  MRF522 MOSI
 * D8     BUZZER 
 * ----------------------------------------------------------------------------
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


// DIVERS
// CODE DE LA SERRURE
char codeSerrure[4]={'1','2','3','4'};
char codeSerrureActuel[4]={'0','0','0','0'};

// BLOCAGE DE LA SERRURE
int erreurCodeMax = 3;
int nbErreur=0;
int tempoBlocage = 5;

// STATUTS DE LA SERRURE
enum {
  SERRURE_OUVERTE=0, 
  SERRURE_FERMEE=1,
  SERRURE_BLOQUEE=2,
  SERRURE_ERREUR=3
  };

byte statutSerrureActuel=SERRURE_FERMEE;
byte statutSerrurePrecedent=SERRURE_FERMEE;


unsigned long int refTime = 0;

bool uneFois = true;

/*
 * ----------------------------------------------------------------------------
 * SETUP
 * ----------------------------------------------------------------------------
 */
void setup()
{
  Serial.begin(115200);

  // LED RGB
  neopixels = new M_neopixel(&globalScheduler);
  
  // animation led de depart
  for (int i=0;i<NB_LEDS*2;i++)
  {
    neopixels->ledOn(i%NB_LEDS, CRGB::Blue);
    delay(50);
    neopixels->ledOn(i%NB_LEDS, CRGB::Black);
  }
  neopixels->allLedOff();

  // KEYPAD
  aKeypad = new M_keypad();

  aKeypad->checkReset();

  // BUZZER
  buzzer = new M_buzzer(PIN_BUZZER, &globalScheduler);
  buzzer->doubleBeep();

  // SERIAL
  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("START !!!"));  
}
/*
 * ----------------------------------------------------------------------------
 * FIN DU SETUP
 * ----------------------------------------------------------------------------
 */




/*
 * ----------------------------------------------------------------------------
 * LOOP
 * ----------------------------------------------------------------------------
 */  
void loop()
{
  // manage task scheduler
  globalScheduler.execute();
  
  // gerer le statut de la serrure
  switch (statutSerrureActuel)
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
}
/*
 * ----------------------------------------------------------------------------
 * FIN DU LOOP
 * ----------------------------------------------------------------------------
 */





/*
 * ----------------------------------------------------------------------------
 * FONCTIONS ADDITIONNELLES
 * ----------------------------------------------------------------------------
 */

 void serrureFermee()
{
  if (uneFois)
  {
    uneFois = false;

    // on allume les led rouge
    neopixels->allLedOff();
    for (int i=0;i<NB_LEDS;i++)
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
    for (int i=0;i<NB_LEDS;i++)
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
    refTime=millis();
    neopixels->startAnimSerrureBloquee(10,500);
  }
  
  if (neopixels->isLastIteration())
  {
    Serial.print(F("END TASK BLOCAGE : "));
    Serial.print(millis()-refTime);
    Serial.println();
    statutSerrureActuel=statutSerrurePrecedent;
    nbErreur=0;
    
  }
}

void serrureErreur()
{
  if (!neopixels->isEnabled())
  {
    neopixels->startAnimSerrureErreur(10,100);
  }
  
  if (neopixels->isLastIteration())
  {
    Serial.println(F("END TASK ERREUR"));
    
    // si il y a eu trop de faux codes
    if (nbErreur>=erreurCodeMax)
    {
      // on bloque la serrure
      statutSerrureActuel=SERRURE_BLOQUEE;
    }
    else
    {
      statutSerrureActuel=statutSerrurePrecedent;
    }
  }
}

void appuiClavier()
{
  // KEYPAD
  char customKey = aKeypad->getChar();
  
  // une touche a ete pressee
  if (customKey != NO_KEY)
  {
    Serial.println(customKey);
    buzzer->shortBeep();

    // la touche pressee n'est pas un #
    if (customKey!='#')
    {
      // on met a jour codeSerrureActuel[] en decalant chaque caractere
      codeSerrureActuel[0] = codeSerrureActuel[1];
      codeSerrureActuel[1] = codeSerrureActuel[2];
      codeSerrureActuel[2] = codeSerrureActuel[3];
      codeSerrureActuel[3] = customKey;
    }
    else
    // la touche pressee est un #
    {
      // on compare codeSerrureActuel[] et codeSerrure[]
      bool codeOK=true;
      for (int i=0;i<4;i++)
      {
         if (codeSerrureActuel[i] != codeSerrure[i])
         {
          // ce caractere est faux, le code n'est pas bon
          codeOK=false;
         }
      }

      // le code est correct, on change le statut de la serrure
      if (codeOK)
      {
        buzzer->shortBeep();
        
        statutSerrureActuel=!statutSerrureActuel;
        nbErreur=0;
        
        codeSerrureActuel[0] = 0;
        codeSerrureActuel[1] = 0;
        codeSerrureActuel[2] = 0;
        codeSerrureActuel[3] = 0;
      }
      // le code est incorrect
      else
      {
        // on beep long
        buzzer->longBeep();

        // on augmente le compteur de code faux
        nbErreur+=1;

        statutSerrurePrecedent=statutSerrureActuel;
        
        // on demarre l'anim faux code
        statutSerrureActuel=SERRURE_ERREUR;        
      }

      uneFois = true;
    }
  }
}

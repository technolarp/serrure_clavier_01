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
  SERRURE_BLOQUEE=2
  };

byte statutSerrureActuel=SERRURE_FERMEE;
byte statutSerrurePrecedent=SERRURE_FERMEE;

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
  
  for (int i=0;i<8;i++)
  {
    neopixels->ledOn(i, CRGB::Blue);
    delay(50);
    neopixels->ledOn(i, CRGB::Black);
  }

  // KEYPAD
  aKeypad = new M_keypad();

  // BUZZER
  buzzer = new M_buzzer(PIN_BUZZER, &globalScheduler);

  // SERIAL
  Serial.println(F(""));
  Serial.println(F(""));
  Serial.println(F("START !!!"));

  buzzer->doubleBeep();
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
  // on allume les led rouge, on eteint les led verte
  for (int i=0;i<NB_LEDS;i++)
  {
    if (i%2==0)
    {
      neopixels->ledOn(i, CRGB::Red);
    }
    else
    {
      neopixels->ledOn(i, CRGB::Black);
    }
  }

  // on check si une touche du clavier a ete activee
  appuiClavier();
}

void serrureOuverte()
{
  // on eteint les led rouge, on allume les led verte
  for (int i=0;i<NB_LEDS;i++)
  {
    if (i%2==0)
    {
      neopixels->ledOn(i, CRGB::Black);
    }
    else
    {
      neopixels->ledOn(i, CRGB::Green);
    }
  }

  // on check si une touche du clavier a ete activee
  appuiClavier();
}

void serrureBloquee()
{
  if (!neopixels->isEnabled())
  {
    neopixels->startAnimSerrureBloquee(5);
  }
  
  if (neopixels->isLastIteration())
  {
    Serial.println(F("END TASK"));
    statutSerrureActuel=statutSerrurePrecedent;
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

        // on clignote la led rouge
        neopixels->startAnimSerrureErreur(10);

        // si il y a eu trop de faux codes
        if (nbErreur>=erreurCodeMax)
        {
          // on bloque la serrure
          statutSerrurePrecedent=statutSerrureActuel;
          statutSerrureActuel=SERRURE_BLOQUEE;
        }
      }
    }
  }
}

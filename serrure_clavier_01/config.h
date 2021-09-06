#include <LittleFS.h>
#include <ArduinoJson.h> // arduino json v6  // https://github.com/bblanchon/ArduinoJson

// to upload config dile : https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/releases
#define SIZE_ARRAY 20
#define MAX_SIZE_CODE 9

class M_config
{
  public:
  // structure stockage
  struct CONFIG_STRUCT
  {
    uint16_t objectId;
	uint16_t groupId;
    
	char objectName[SIZE_ARRAY];
  char codeSerrure[MAX_SIZE_CODE];
	
	uint8_t activeLeds;
	uint8_t tailleCode;
	uint8_t nbErreurCodeMax;
	uint8_t nbErreurCode;
	uint16_t intervalBlocage;
	uint8_t statutSerrureActuel;
	uint8_t statutSerrurePrecedent;
  };
  
  // creer une structure
  CONFIG_STRUCT myConfig;
  
  M_config()
  {
  }
  
  void mountFS()
  {
    Serial.println(F("Mount LittleFS"));
    if (!LittleFS.begin()) 
    {
      Serial.println(F("LittleFS mount failed"));
      return;
    }
  }
  
  void printJsonFile(const char * filename)
  {
    // Open file for reading
    File file = LittleFS.open(filename, "r");
    if (!file) 
    {
      Serial.println(F("Failed to open file for reading"));
    }
      
    StaticJsonDocument<1024> doc;
    
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
      Serial.println(F("Failed to deserialize file"));
      Serial.println(error.c_str());
    }
    else
    {
      serializeJsonPretty(doc, Serial);
      Serial.println();
    }
    
    // Close the file (File's destructor doesn't close the file)
    file.close();
  }
  
  void listDir(const char * dirname)
  {
    Serial.printf("Listing directory: %s", dirname);
    Serial.println();
  
    Dir root = LittleFS.openDir(dirname);
  
    while (root.next())
	  {
      File file = root.openFile("r");
      Serial.print(F("  FILE: "));
      Serial.print(root.fileName());
      Serial.print(F("  SIZE: "));
      Serial.print(file.size());
      Serial.println();
      file.close();
    }

    Serial.println();
  }
  
  
  void readObjectConfig(const char * filename)
  {
    // lire les données depuis le fichier littleFS
    // Open file for reading
    File file = LittleFS.open(filename, "r");
    if (!file) 
    {
      Serial.println(F("Failed to open file for reading"));
      return;
    }
    else
    {
      Serial.println(F("File opened"));
    }
  
    StaticJsonDocument<1024> doc;
    
	  // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, file);
    if (error)
    {
      Serial.println(F("Failed to deserialize file"));
      Serial.println(error.c_str());
    }
    else
    {
		// Copy values from the JsonObject to the Config
		myConfig.objectId = doc["objectId"];
		myConfig.groupId = doc["groupId"];
		myConfig.activeLeds = doc["activeLeds"];
		myConfig.tailleCode = doc["tailleCode"];
		myConfig.nbErreurCodeMax = doc["nbErreurCodeMax"];
		myConfig.nbErreurCode = doc["nbErreurCode"];
		myConfig.intervalBlocage = doc["intervalBlocage"];
		myConfig.statutSerrureActuel = doc["statutSerrureActuel"];
		myConfig.statutSerrurePrecedent = doc["statutSerrurePrecedent"];
  		
  		if (doc.containsKey("objectName"))
  		{ 
  			strlcpy(  myConfig.objectName,
  			          doc["objectName"],
  			          sizeof(myConfig.objectName));
  		}
  		
  		if (doc.containsKey("codeSerrure"))
  		{ 
  			strlcpy(  myConfig.codeSerrure,
  			          doc["codeSerrure"],
  			          sizeof(myConfig.codeSerrure));
  		}
    }
  		
      // Close the file (File's destructor doesn't close the file)
      file.close();
  }
  
  void writeObjectConfig(const char * filename)
  {
    // Delete existing file, otherwise the configuration is appended to the file
    LittleFS.remove(filename);
  
    // Open file for writing
    File file = LittleFS.open(filename, "w");
    if (!file) 
    {
      Serial.println(F("Failed to create file"));
      return;
    }

    // Allocate a temporary JsonDocument
    StaticJsonDocument<1024> doc;

    doc["objectId"] = myConfig.objectId;
    doc["groupId"] = myConfig.groupId;
    doc["activeLeds"] = myConfig.activeLeds;
    doc["tailleCode"] = myConfig.tailleCode;
    doc["nbErreurCodeMax"] = myConfig.nbErreurCodeMax;
    doc["nbErreurCode"] = myConfig.nbErreurCode;
    doc["intervalBlocage"] = myConfig.intervalBlocage;
    doc["statutSerrureActuel"] = myConfig.statutSerrureActuel;
    doc["statutSerrurePrecedent"] = myConfig.statutSerrurePrecedent;
    
    String newObjectName="";
    String newcodeSerrure="";
    
    for (int i=0;i<SIZE_ARRAY;i++)
    {
      newObjectName+= myConfig.objectName[i];
    }
	
	for (int i=0;i<MAX_SIZE_CODE;i++)
    {
      newcodeSerrure+= myConfig.codeSerrure[i];
    }	
    
    doc["objectName"] = newObjectName;
    doc["codeSerrure"] = newcodeSerrure;
    

    // Serialize JSON to file
    if (serializeJson(doc, file) == 0) 
    {
      Serial.println(F("Failed to write to file"));
    }

    // Close the file (File's destructor doesn't close the file)
    file.close();
  }
  
  void writeDefaultObjectConfig(const char * filename)
  {
	myConfig.objectId = 1;
	myConfig.groupId = 1;
	myConfig.activeLeds = 8;
	myConfig.tailleCode = 4;
	myConfig.nbErreurCodeMax = 3;
	myConfig.nbErreurCode = 0;
	myConfig.intervalBlocage = 10;
	myConfig.statutSerrureActuel = 1;
	myConfig.statutSerrurePrecedent = 1;
	
	strlcpy(  myConfig.objectName,
  			          "serrure",
  			          sizeof("serrure"));
	
	strlcpy(  myConfig.codeSerrure,
  			          "12345678",
  			          sizeof("12345678"));	
		
	writeObjectConfig(filename);
  }
	
};

#include <LittleFS.h>
#include <ArduinoJson.h> // arduino json v6  // https://github.com/bblanchon/ArduinoJson

// to upload config dile : https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/releases
#define SIZE_ARRAY 20
#define MAX_SIZE_CODE 9

#include <IPAddress.h>

class M_config
{
  public:
  // structure stockage
  struct OBJECT_CONFIG_STRUCT
  {
    uint16_t objectId;
  	uint16_t groupId;
      
  	char objectName[SIZE_ARRAY];
    char codeSerrure[MAX_SIZE_CODE];
  	
  	uint8_t activeLeds;
    uint8_t brightness;
  	uint8_t tailleCode;
  	uint8_t nbErreurCodeMax;
  	uint8_t nbErreurCode;
  	uint16_t intervalBlocage;
  	uint8_t statutSerrureActuel;
  	uint8_t statutSerrurePrecedent;
  };
  
  // creer une structure
  OBJECT_CONFIG_STRUCT objectConfig;

  struct NETWORK_CONFIG_STRUCT
  {
    IPAddress apIP;
    IPAddress apNetMsk;
    char apName[SIZE_ARRAY];
    char apPassword[SIZE_ARRAY];
  };
  
  // creer une structure
  NETWORK_CONFIG_STRUCT networkConfig;
  
  
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
  		objectConfig.objectId = doc["objectId"];
  		objectConfig.groupId = doc["groupId"];
  		objectConfig.activeLeds = doc["activeLeds"];
      objectConfig.brightness = doc["brightness"];      
  		objectConfig.tailleCode = doc["tailleCode"];
  		objectConfig.nbErreurCodeMax = doc["nbErreurCodeMax"];
  		objectConfig.nbErreurCode = doc["nbErreurCode"];
  		objectConfig.intervalBlocage = doc["intervalBlocage"];
  		objectConfig.statutSerrureActuel = doc["statutSerrureActuel"];
  		objectConfig.statutSerrurePrecedent = doc["statutSerrurePrecedent"];
    		
  		if (doc.containsKey("objectName"))
  		{ 
  			strlcpy(  objectConfig.objectName,
  			          doc["objectName"],
  			          sizeof(objectConfig.objectName));
  		}
  		
  		if (doc.containsKey("codeSerrure"))
  		{ 
  			strlcpy(  objectConfig.codeSerrure,
  			          doc["codeSerrure"],
  			          sizeof(objectConfig.codeSerrure));
  		}
    }
  		
    // Close the file (File's destructor doesn't close the file)
    file.close();
  }

  void readNetworkConfig(const char * filename)
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
      if (doc.containsKey("apIP"))
      { 
        JsonArray apIP = doc["apIP"];
        
        networkConfig.apIP[0] = apIP[0];
        networkConfig.apIP[1] = apIP[1];
        networkConfig.apIP[2] = apIP[2];
        networkConfig.apIP[3] = apIP[3];
      }

      if (doc.containsKey("apNetMsk"))
      { 
        JsonArray apNetMsk = doc["apNetMsk"];
        
        networkConfig.apNetMsk[0] = apNetMsk[0];
        networkConfig.apNetMsk[1] = apNetMsk[1];
        networkConfig.apNetMsk[2] = apNetMsk[2];
        networkConfig.apNetMsk[3] = apNetMsk[3];
      }
          
      if (doc.containsKey("apName"))
      { 
        strlcpy(  networkConfig.apName,
                  doc["apName"],
                  sizeof(networkConfig.apName));
      }

      if (doc.containsKey("apPassword"))
      { 
        strlcpy(  networkConfig.apPassword,
                  doc["apPassword"],
                  sizeof(networkConfig.apPassword));
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

    doc["objectId"] = objectConfig.objectId;
    doc["groupId"] = objectConfig.groupId;
    doc["activeLeds"] = objectConfig.activeLeds;
    doc["brightness"] = objectConfig.brightness;    
    doc["tailleCode"] = objectConfig.tailleCode;
    doc["nbErreurCodeMax"] = objectConfig.nbErreurCodeMax;
    doc["nbErreurCode"] = objectConfig.nbErreurCode;
    doc["intervalBlocage"] = objectConfig.intervalBlocage;
    doc["statutSerrureActuel"] = objectConfig.statutSerrureActuel;
    doc["statutSerrurePrecedent"] = objectConfig.statutSerrurePrecedent;
    
    String newObjectName="";
    String newcodeSerrure="";
    
    for (int i=0;i<SIZE_ARRAY;i++)
    {
      newObjectName+= objectConfig.objectName[i];
    }
	
	for (int i=0;i<MAX_SIZE_CODE;i++)
    {
      newcodeSerrure+= objectConfig.codeSerrure[i];
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

  void writeNetworkConfig(const char * filename)
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

    String newApName="";
    String newApPassword="";
    
    for (int i=0;i<SIZE_ARRAY;i++)
    {
      newApName+= networkConfig.apName[i];
      newApPassword+= networkConfig.apPassword[i];
    }
    
    doc["apName"] = newApName;
    doc["apPassword"] = newApPassword;

    StaticJsonDocument<128> docIp;
    JsonArray arrayIp = docIp.to<JsonArray>();
    arrayIp.add(networkConfig.apIP[0]);
    arrayIp.add(networkConfig.apIP[1]);
    arrayIp.add(networkConfig.apIP[2]);
    arrayIp.add(networkConfig.apIP[3]);

    StaticJsonDocument<128> docNetMask;
    JsonArray arrayNetMask = docNetMask.to<JsonArray>();
    arrayNetMask.add(networkConfig.apNetMsk[0]);
    arrayNetMask.add(networkConfig.apNetMsk[1]);
    arrayNetMask.add(networkConfig.apNetMsk[2]);
    arrayNetMask.add(networkConfig.apNetMsk[3]);
    
    doc["apIP"]=arrayIp;
    doc["apNetMsk"]=arrayNetMask;

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
	objectConfig.objectId = 1;
	objectConfig.groupId = 1;
	objectConfig.activeLeds = 8;
  objectConfig.brightness = 80;  
	objectConfig.tailleCode = 4;
	objectConfig.nbErreurCodeMax = 3;
	objectConfig.nbErreurCode = 0;
	objectConfig.intervalBlocage = 10;
	objectConfig.statutSerrureActuel = 1;
	objectConfig.statutSerrurePrecedent = 1;
	
	strlcpy(  objectConfig.objectName,
  			          "serrure",
  			          sizeof("serrure"));
	
	strlcpy(  objectConfig.codeSerrure,
  			          "12345678",
  			          sizeof("12345678"));	
		
	writeObjectConfig(filename);
  }

  void writeDefaultNetworkConfig(const char * filename)
  {
  strlcpy(  networkConfig.apName,
                  "SERRURE",
                  sizeof("SERRURE"));
  
  strlcpy(  networkConfig.apPassword,
                  "",
                  sizeof(""));

  networkConfig.apIP[0]=192;
  networkConfig.apIP[1]=168;
  networkConfig.apIP[2]=1;
  networkConfig.apIP[3]=1;

  networkConfig.apNetMsk[0]=255;
  networkConfig.apNetMsk[1]=255;
  networkConfig.apNetMsk[2]=255;
  networkConfig.apNetMsk[3]=0;
    
  writeNetworkConfig(filename);
  }

  String stringJsonFile(const char * filename)
  {
    String result = "";
    
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
      serializeJson(doc, result);
    }
    
    // Close the file (File's destructor doesn't close the file)
    file.close();
    return(result);
  }
};

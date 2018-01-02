#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <rBase64.h>

#define SSID_FILE "etc/ssid" // Path to the SSID file 
#define PASSWORD_FILE "etc/pass" // Path to the password filfe

#define CONNECT_TIMEOUT_SECS 30 // Timeout time for connecting to the access point

#define AP_SSID "neato" // Initial or fallback access point SSID
#define AP_PASSWD "neatootaen" // Initial or fallback password for tha access point "neato"

//#define MAX_BUFFER 8192 // max. serial buffer size

//int bufferSize = 0; // acctual buffer size of serial return
//uint8_t serialBuffer[MAX_BUFFER+1]; // Buffer for the serial return

ESP8266WebServer server = ESP8266WebServer(80); // WebServer to connect to


/**
 * @brief: Send a command to the robot and reads an incomming serial from the reboot
 * 
 * After a command is send to the robot a return value is send to the serial interface. 
 * This function reads outgoing message and sends it as answer via the REsT-Interface. 
 */
void execCommand() {
  if (server.args() == 1) {
		if (server.argName(0) == "command") {

      Serial.println(server.arg(0));
      
      String answer = Serial.readString();
      server.send(200, "text/html", answer);
      
    } else {
      handleNotFound();
    }
  } else {
    handleNotFound();
  }
}

/**
 * @brief: Simple WiFi setup or reconfiguration 
 * 
 * The function sends a simple HTML site for configuring the access point and the password. 
 * The password is never displayed for safety reasons. If no SSID is configured the SSID field will show "SSID"
 */
void setupNeato() {
  char ssid[256];
  File ssid_file = SPIFFS.open(SSID_FILE, "r");
  
  if(!ssid_file) {
    strcpy(ssid, "SSID");
  } else {
    ssid_file.readString().toCharArray(ssid, 256);
    ssid_file.close();
  }

  server.send(200, "text/html", String() + 
  "<!DOCTYPE html><html> <body>" +
  "<form action=\"\" method=\"post\" style=\"display: inline;\">" +
  "Access Point SSID:<br />" +
  "<input type=\"text\" name=\"ssid\" value=\"" + ssid + "\"> <br />" +
  "WPA2 Password:<br />" +
  "<input type=\"password\" name=\"password\" value= *** \"> <br />" +
  "<br />" +
  "<input type=\"submit\" value=\"Submit\"> </form>" +
  "<form action=\"http://neato.local/reboot\" style=\"display: inline;\">" +
  "<input type=\"submit\" value=\"Reboot\" />" +
  "</form>" +
  "<p>Enter the details for your access point. After you submit, the controller will reboot to apply the settings.</p>" +
  "<a href=\"https://www.neatorobotics.com/resources/programmersmanual_20140305.pdf\">Command Documentation</a></p>" +
  "</body></html>\n");
}

/**
 * @brief: Save AP credentails to ESP8266 file system
 * 
 * The function will save the credentials of the access point (SSID and password) to the file system. 
 * If saving could not be executed correctly an error page will be send out displaying the error. 
 */
void saveAccessPointCredentials() {
  String user_ssid = server.arg("ssid");
  String user_password = server.arg("password");
  SPIFFS.format();

  if(user_ssid != "" && user_password != "") {
    
    /*// Save SSID
    File ssid_file = SPIFFS.open(SSID_FILE, "w");
    if (!ssid_file) {
      server.send(400, "text/html", "<!DOCTYPE html><html> <body> Setting Access Point SSID failed!</body> </html>");
      return;
    }
    ssid_file.print(user_ssid);
    ssid_file.close();
    
    // Save passwd
    File passwd_file = SPIFFS.open(PASSWORD_FILE, "w");
    if (!passwd_file) {
      server.send(400, "text/html", "<!DOCTYPE html><html> <body> Setting Access Point password failed!</body> </html>");
      return;
    }
    passwd_file.print(user_password);
    passwd_file.close();
  */
    // SSID
    if (!saveToFileSystem(SSID_FILE, user_ssid)) {
      server.send(400, "text/html", "<!DOCTYPE html><html> <body> Setting Access Point SSID failed!</body> </html>");
        return;
    }

    // Passwd
    if (!saveToFileSystem(PASSWORD_FILE, user_password)) {
      server.send(400, "text/html", "<!DOCTYPE html><html> <body> Setting Access Point password failed!</body> </html>");
      return;
    }

    server.send(200, "text/html", String() + 
    "<!DOCTYPE html><html> <body>" +
    "Setting Access Point SSID / password was successful! <br />" +
    "<br />SSID was set to \"" + user_ssid + "\" with the password *** \". <br />" +
    "<br /> The controller will now reboot. Please re-connect to your Wi-Fi network.<br />" +
    "If the SSID or password was incorrect, the controller will return to Access Point mode." +
    "</body> </html>");

    ESP.reset();
  } else {
    server.send(400, "text/html", "<!DOCTYPE html><html> <body> Please provide a password and a SSID.</body> </html>");
  }
}

/**
 * @brief: Save a string to the file system
 * 
 * The function will save a given string to the file system. If it fails the funtions returns false, true otherwise. 
 * @param: String file - The filesystem to be saved to
 * @param: String content - The content to be saved to the filesystme
 * @return: false on error, true otherwise
 */ 
boolean saveToFileSystem(String file, String content) {
    
    File passwd_file = SPIFFS.open(file, "w");
    if (!passwd_file) {
      return false;
    }

    passwd_file.print(content);
    passwd_file.close();

    return true;
}

/**
 * @brief: Reboot the ESP8266
 * 
 * The function will send a message the webserver explaining the reboot. Afterwards the esp8266 is rebooted.
 */
void rebootEvent() {
  server.send(200, "text/html", String() + 
  "<!DOCTYPE html><html> <body>" +
  "The controller will now reboot.<br />" +
  "If the SSID or password is set but is incorrect, the controller will return to Access Point mode." +
  "</body> </html>");
  ESP.reset();
}


/**
 * @brief: Display an error page if the command was not found.
 * 
 * If an enpoint is triggerd that is not implemented an error page is displayed showing the wrong input paramters. 
 */
void handleNotFound() {
	String message = "Command not found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for (uint8_t i = 0; i < server.args(); i++) {
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}

	server.send(404, "text/plain", message);
}


/**
 * @brief Start service neato.local 
 * 
 * This function sets a MDS for the esp8266 to be reachable via neato.local. 
 */
void startMDS() {
  if (!MDNS.begin("neato")) {
    ESP.reset(); // if MDNS fail => restart and try over
  }

  MDNS.addService("http", "tcp", 80);
}

/**
 * @brief. Set up server endpoints
 * 
 * All server endpoints a configurated via this function. 
 */
void setServerEndpoints() {
  //server.on("/console", serverEvent);
  server.on("/sendcommand", execCommand);
  server.on("/", HTTP_POST, saveAccessPointCredentials);
  server.on("/", HTTP_GET, setupNeato);
  server.on("/reboot", HTTP_GET, rebootEvent);
  server.onNotFound(handleNotFound);
}

/**
* @brief: Mount the file system
*
* This function will mount the file system. If it fails, it formats the storage and will initialize the filesystem
*/
void mountFileSystem() {
  if(!SPIFFS.begin()) {
    SPIFFS.format();
    SPIFFS.begin();
  }
}


/**
 * @brief: Initialize the WiFi connection
 * 
 * Check if a SSID and PW was set correctly. If not boot up an AP for further configuration.
 */
void initWiFiConnection() {
 if(SPIFFS.exists(SSID_FILE) && SPIFFS.exists(PASSWORD_FILE)) {

    // Load SSID
    File ssid_file = SPIFFS.open(SSID_FILE, "r");
    char ssid[256];
    ssid_file.readString().toCharArray(ssid, 256);
    ssid_file.close();

    // Load password
    File passwd_file = SPIFFS.open(PASSWORD_FILE, "r");
    char passwd[256];
    passwd_file.readString().toCharArray(passwd, 256);
    passwd_file.close();

    // attempt station connection
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passwd);

    // Wait some seconds to connect to local WiFi
    for(int i = 0; i < CONNECT_TIMEOUT_SECS * 20 && WiFi.status() != WL_CONNECTED; i++) {
      delay(50);
    }
  }

  //start AP mode if either the AP / password do not exist, or cannot be connected to within CONNECT_TIMEOUT_SECS seconds.
  if(WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);

    if(!WiFi.softAP(AP_SSID, AP_PASSWD)) {
      ESP.reset(); //reset because there's no good reason for setting up an AP to fail
    }
  }
}

// General Arduino setup routine 
void setup() {

  // botvac serial console is 115200 baud, 8 data bits, no parity, one stop bit (8N1)
  Serial.begin(115200);
  
  mountFileSystem();
 
  initWiFiConnection();

  setServerEndpoints();
  server.begin();

  startMDS();
}

// General Arduino loop  routine 
void loop() { 
  server.handleClient();
  delay(1);
}

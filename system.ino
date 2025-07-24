// ---------------------------------------------------------------------------------------
//
// Code for a webserver on the ESP32 to control LEDs (device used for tests: ESP32-WROOM-32D).
// The code allows user to switch between three LEDs and set the intensity of the LED selected
//
// For installation, the following libraries need to be installed:
// * Websockets by Markus Sattler (can be tricky to find -> search for "Arduino Websockets"
// * ArduinoJson by Benoit Blanchon
//
// Written by mo thunderz (last update: 19.11.2021)
//
// ---------------------------------------------------------------------------------------

#include <WiFi.h>                                     // needed to connect to WiFi
#include <WebServer.h>                                // needed to create a simple webserver (make sure tools -> board is set to ESP32, otherwise you will get a "WebServer.h: No such file or directory" error)
#include <WebSocketsServer.h>                         // needed for instant communication between client and server through Websockets
#include <ArduinoJson.h>                              // needed for JSON encapsulation (send multiple variables with one string)
#include <ESP32Servo.h>

// SSID and password of Wifi connection:
const char* ssid = "";  
const char* password = "";

// The String below "webpage" contains the complete HTML code that is sent to the client whenever someone connects to the webserver
// NOTE 27.08.2022: I updated in the webpage "slider.addEventListener('click', slider_changed);" to "slider.addEventListener('change', slider_changed);" -> the "change" did not work on my phone.
String SendHTML()
{
  String ptr = "<!DOCTYPE html>\n";
  ptr +="<html>\n";
  ptr +="<head>\n";
  ptr +="<title>ESP32_rotor_control</title>\n";
  ptr +="</head>\n";
  ptr +="<body style='background-color: #EEEEEE;'>\n";
  ptr +="\n";
  ptr +="<span style='color: #003366;'>\n";
  ptr +="\n";
  ptr +="<h1>Servo rotation control</h1>\n";
  ptr +="\n";
  ptr +="<br>\n";
  ptr +="\n";
  ptr +="Set rotation speed: <br>\n";
  ptr +="<input type='range' min='0' max='180' value='90' class='slider' id='ID_LED_INTENSITY'>\n";
  ptr +="Current value: <span id='ID_LED_INTENSITY_VALUE'>-</span><br>\n";
  ptr +="\n";
  ptr +="</span>\n";
  ptr +="\n";
  ptr +="</body>\n";
  ptr +="<script>\n";
  ptr +="\n";
  ptr +="  var slider = document.getElementById('ID_LED_INTENSITY');\n";
  ptr +="  var output = document.getElementById('ID_LED_INTENSITY_VALUE');\n";
  ptr +="  slider.addEventListener('change', slider_changed);\n";
  ptr +="\n";
  ptr +="  var Socket;\n";
  ptr +="\n";
  ptr +="  function init() {\n";
  ptr +="    Socket = new WebSocket('ws://' + window.location.hostname + ':81/');\n";
  ptr +="    Socket.onmessage = function(event) {\n";
  ptr +="      processCommand(event);\n";
  ptr +="    };\n";
  ptr +="  }\n";
  ptr +="\n";
  ptr +="  function slider_changed () {\n";
  ptr +="    var l_LED_intensity = slider.value;\n";
  ptr +="    console.log(l_LED_intensity);\n";
  ptr +="    var msg = { \n";
  ptr +="      type: 'LED_intensity',\n";
  ptr +="      value: l_LED_intensity\n";
  ptr +="    };\n";
  ptr +="    Socket.send(JSON.stringify(msg)); \n";
  ptr +="  }\n";
  ptr +="\n";
  ptr +="  function processCommand(event) {\n";
  ptr +="	  var obj = JSON.parse(event.data);\n";
  ptr +="   var type = obj.type;\n";
  ptr +="   if (type.localeCompare(\"LED_intensity\") == 0) { \n";
  ptr +="     var l_LED_intensity = parseInt(obj.value); \n";
  ptr +="     console.log(l_LED_intensity); \n";
  ptr +="     slider.value = l_LED_intensity; \n";
  ptr +="     output.innerHTML = l_LED_intensity;\n";
  ptr +="   }\n";
  ptr +="  }\n";
  ptr +="\n";
  ptr +="  window.onload = function(event) {\n";
  ptr +="    init();\n";
  ptr +="  }\n";
  ptr +="</script>\n";
  ptr +="</html>\n";
  
  return ptr;
}

// global variables of the LED selected and the intensity of that LED
int LED_intensity = 50;

// some standard stuff needed to do "analogwrite" on the ESP32 -> an ESP32 does not have "analogwrite" and uses ledcWrite instead
const int freq = 5000;
const int resolution = 8;

// The JSON library uses static memory, so this will need to be allocated:
// -> in the video I used global variables for "doc_tx" and "doc_rx", however, I now changed this in the code to local variables instead "doc" -> Arduino documentation recomends to use local containers instead of global to prevent data corruption

// Initialization of webserver and websocket
WebServer server(80);                                 // the server uses port 80 (standard port for websites
WebSocketsServer webSocket = WebSocketsServer(81);    // the websocket uses port 81 (standard port for websockets

Servo serv; //Create instance of servo

void setup() {

  Serial.begin(115200);                               // init serial port for debugging

  WiFi.begin(ssid, password);                         // start WiFi interface
  Serial.println("Establishing connection to WiFi with SSID: " + String(ssid));     // print SSID to the serial interface for debugging
 
  while (WiFi.status() != WL_CONNECTED) {             // wait until WiFi is connected
    delay(1000);
    Serial.print(".");
  }
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());                     // show IP address that the ESP32 has received from router
  
  server.on("/", []() {                               // define here wat the webserver needs to do
    server.send(200, "text/html", SendHTML());           //    -> it needs to send out the HTML string "webpage" to the client
  });
  server.begin();                                     // start server
  
  webSocket.begin();                                  // start websocket
  webSocket.onEvent(webSocketEvent);                  // define a callback function -> what does the ESP32 need to do when an event from the websocket is received? -> run function "webSocketEvent()"

  serv.attach(4); //attaching servo

}

void loop() {
  server.handleClient();                              // Needed for the webserver to handle all clients
  webSocket.loop();                                   // Update function for the webSockets 
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {      // the parameters of this callback function are always the same -> num: id of the client who send the event, type: type of message, payload: actual data sent and length: length of payload
  switch (type) {                                     // switch on the type of information sent
    case WStype_DISCONNECTED:                         // if a client is disconnected, then type == WStype_DISCONNECTED
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:                            // if a client is connected, then type == WStype_CONNECTED
      Serial.println("Client " + String(num) + " connected");
      
      // send LED_intensity and LED_select to clients -> as optimization step one could send it just to the new client "num", but for simplicity I left that out here
      sendJson("LED_intensity", String(LED_intensity));
      break;
    case WStype_TEXT:                                 // if a client has sent data, then type == WStype_TEXT
      // try to decipher the JSON string received
      StaticJsonDocument<200> doc;                    // create JSON container 
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }
      else {
        // JSON string was received correctly, so information can be retrieved:
        const char* l_type = doc["type"];
        const int l_value = doc["value"];
        Serial.println("Type: " + String(l_type));
        Serial.println("Value: " + String(l_value));

        // if LED_intensity value is received -> update and write to LED
        if(String(l_type) == "LED_intensity") {
          LED_intensity = int(l_value);
          sendJson("LED_intensity", String(l_value));
          serv.write(l_value);
        }

      }
      Serial.println("");
      break;
  }
}

// Simple function to send information to the web clients
void sendJson(String l_type, String l_value) {
    String jsonString = "";                           // create a JSON string for sending data to the client
    StaticJsonDocument<200> doc;                      // create JSON container
    JsonObject object = doc.to<JsonObject>();         // create a JSON Object
    object["type"] = l_type;                          // write data into the JSON object -> I used "type" to identify if LED_selected or LED_intensity is sent and "value" for the actual value
    object["value"] = l_value;
    serializeJson(doc, jsonString);                // convert JSON object to string
    webSocket.broadcastTXT(jsonString);               // send JSON string to all clients
}

#ifndef CHESSGRID_WEBSERVER_H
#define CHESSGRID_WEBSERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

// ============================================================
// CONFIGURATION
// ============================================================

String configuredSSID = "";
String configuredPassword = "";

String websocketHost = "";

// ============================================================
// WIFI SETUP PAGE
// ============================================================

const char WIFI_PAGE[] PROGMEM = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
   content="width=device-width, initial-scale=1">

<title>ChessGrid WiFi Setup</title>

<style>

body {
  margin: 0;
  padding: 30px;
  background: #111;
  color: white;
  font-family: Arial, sans-serif;
}

.container {
  max-width: 500px;
  margin: auto;
}

h1 {
  text-align: center;
}

input {
  width: 100%;
  box-sizing: border-box;
  padding: 12px;
  margin-top: 8px;
  font-size: 18px;
}

label {
  display: block;
  margin-top: 20px;
}

button {
  width: 100%;
  padding: 14px;
  margin-top: 25px;
  font-size: 18px;
}

</style>

</head>

<body>

<div class="container">

<h1>ChessGrid</h1>

<p>WiFi Configuration</p>

<form action="/savewifi" method="GET">

<label>WiFi SSID</label>

<input
type="text"
name="ssid"
required
>

<label>WiFi Password</label>

<input
type="password"
name="password"
>

<button type="submit">
Connect to WiFi
</button>

</form>

</div>

</body>

</html>

)rawliteral";

// ============================================================
// DEVICE CONFIGURATION PAGE
// ============================================================

String buildConfigPage() {

String page = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
   content="width=device-width, initial-scale=1">

<title>ChessGrid Configuration</title>

<style>

body {
  margin: 0;
  padding: 30px;
  background: #111;
  color: white;
  font-family: Arial, sans-serif;
}

.container {
  max-width: 500px;
  margin: auto;
}

h1 {
  text-align: center;
}

.section {
  background: #222;
  padding: 20px;
  margin-top: 20px;
  border-radius: 12px;
}

label {
  display: block;
  margin-top: 15px;
}

input {
  width: 100%;
  box-sizing: border-box;
  padding: 12px;
  margin-top: 8px;
  font-size: 18px;
}

button {
  width: 100%;
  padding: 14px;
  margin-top: 25px;
  font-size: 18px;
}

.status {
  color: #aaa;
  margin-top: 20px;
}

</style>

</head>

<body>

<div class="container">

<h1>ChessGrid</h1>

<div class="section">

<h2>WiFi</h2>

<p>
Status: Connected
</p>

<p>
IP Address:
)rawliteral";

page += WiFi.localIP().toString();

page += R"rawliteral(

</p>

</div>

<div class="section">

<h2>Live Server</h2>

<form action="/saveconfig" method="GET">

<label>Server</label>

<input
type="text"
name="host"
value=")rawliteral";

page += websocketHost;

page += R"rawliteral("
placeholder="live.chessnuts.fun"
required
>

<button type="submit">
Save Configuration
</button>

</form>

</div>

<div class="status">

ChessGrid Configuration

</div>

</div>

</body>

</html>

)rawliteral";

return page;
}

// ============================================================
// ROOT
// ============================================================

void handleRoot() {

if (
WiFi.getMode() == WIFI_AP
) {

server.send(
  200,
  "text/html",
  WIFI_PAGE
);

}

else {

server.send(
  200,
  "text/html",
  buildConfigPage()
);

}

}

// ============================================================
// SAVE WIFI
// ============================================================

void handleSaveWiFi() {

configuredSSID =
server.arg("ssid");

configuredPassword =
server.arg("password");

Serial.println();
Serial.println("==============================");
Serial.println("WIFI CONFIGURATION");
Serial.println("==============================");

Serial.print("SSID: ");
Serial.println(configuredSSID);

// ==========================================================
// STOP AP
// ==========================================================

Serial.println();
Serial.println("Stopping AP...");

WiFi.softAPdisconnect(true);

delay(500);

// ==========================================================
// CONNECT TO ROUTER
// ==========================================================

Serial.println("Connecting to WiFi...");

WiFi.mode(WIFI_STA);

WiFi.begin(
configuredSSID.c_str(),
configuredPassword.c_str()
);

int attempts = 0;

while (
WiFi.status() != WL_CONNECTED &&
attempts < 30
) {

delay(500);

Serial.print(".");

attempts++;

}

Serial.println();

// ==========================================================
// SUCCESS
// ==========================================================

if (
WiFi.status() == WL_CONNECTED
) {

Serial.println();
Serial.println("WIFI CONNECTED!");

Serial.print("ESP32 IP: ");
Serial.println(
  WiFi.localIP()
);
if (MDNS.begin("chessgrid")) {

    Serial.println("mDNS started!");

    Serial.println(
        "Open: http://chessgrid.local"
    );

} else {

    Serial.println(
        "mDNS failed!"
    );

}

server.send(
  200,
  "text/html",
  "<h1>WiFi Connected!</h1>"
  "<p>ChessGrid is now connected.</p>"
  "<p>Open the new ESP32 IP address.</p>"
);

}

// ==========================================================
// FAILED
// ==========================================================

else {

Serial.println();
Serial.println("WIFI CONNECTION FAILED");

WiFi.mode(WIFI_AP);

WiFi.softAP(
  "ChessGrid"
);

server.send(
  200,
  "text/html",
  "<h1>Connection Failed</h1>"
  "<p>Please reconnect to ChessGrid and try again.</p>"
);

}

}

// ============================================================
// SAVE DEVICE CONFIGURATION
// ============================================================

void handleSaveConfig() {

websocketHost =
server.arg("host");

websocketHost.trim();

Serial.println();
Serial.println("==============================");
Serial.println("DEVICE CONFIGURATION");
Serial.println("==============================");

Serial.print("WebSocket Host: ");
Serial.println(websocketHost);

server.send(
200,
"text/html",
"<h1>Configuration Saved</h1>"
"<p>ChessGrid configuration updated.</p>"
"<p><a href='/'>Back</a></p>"
);

}

// ============================================================
// 404
// ============================================================

void handleNotFound() {

server.send(
404,
"text/plain",
"404 - Not Found"
);

}

// ============================================================
// START AP + WEB SERVER
// ============================================================

void setupWebServer() {

Serial.println();
Serial.println("Starting ChessGrid AP...");

WiFi.mode(WIFI_AP);

bool apStarted =
WiFi.softAP(
"ChessGrid"
);

if (MDNS.begin("chessgrid")) {

  Serial.println();
  Serial.println("mDNS started in AP mode!");

  Serial.println(
    "Open: http://chessgrid.local"
  );

} else {

  Serial.println(
    "mDNS FAILED in AP mode!"
  );

}

if (apStarted) {

Serial.println("AP started!");

}

else {

Serial.println("AP FAILED!");

}

Serial.print("AP IP: ");

Serial.println(
WiFi.softAPIP()
);

// ==========================================================
// ROUTES
// ==========================================================

server.on(
"/",
HTTP_GET,
handleRoot
);

server.on(
"/savewifi",
HTTP_GET,
handleSaveWiFi
);

server.on(
"/saveconfig",
HTTP_GET,
handleSaveConfig
);

server.onNotFound(
handleNotFound
);

server.begin();

Serial.println(
"WebServer started!"
);

}

// ============================================================
// HANDLE SERVER
// ============================================================

void handleWebServer() {

  server.handleClient();
}

#endif
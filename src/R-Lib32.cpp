/*
R-Lib32 by Tim Fischbach
Functions:
- User based Wifi config
- Sys intern Wifi config
- Update Functions over 3 ways! (Stable, Beta or Dev)
- Data Transmission to Update-server for stats.

TO DO:
- Redirection (Log In WIFI)
*/
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "R-Lib32.h"

const String LIBVERSION = "v1.0.0";

String strinit, initlink, binlink, content, st, DEVICENAME, VERSION, dllink, devlink, DEVTAG, SSID, PASSWORD;
int serverstatus, statusCode;
int totalLength;
int currentLength = 0;
bool beta = false;
bool dev = false;

WebServer server(80);
WiFiClient updatewificlient;
HTTPClient http;

String checkUpdate();
String split(String s, char parser, int index);
String performUpdate();
String dataTransmission();
void connectWIFI(String ssid, String passwd);
bool checkWIFI();
void resetWIFI();
void disconnectWIFI();
void saveWIFI(String ssid, String password);
String loadWIFI(String mode);
void connectWIFIUser(String ssid, String password);
void createWebServer();
String getLibVersion();
void connectWIFIUserHandle();
void endWIFIUser();
String getLibVersion();
void connectWIFIUserHandle();
void endWIFIUser();
void setDeviceName(String DevName);
String getDeviceName();
void setVersion(String ver);
String getVersion();
void setDlLink(String DLL);
String getDlLink();
void setDevLink(String DEL);
String getDevLink();
void setDevTag(String DTAG);
String getDevTag();
void setBetaState(bool sbeta);
bool getBetaState();
void setDevState(bool sdev);
bool getDevState();
bool varCheck();
void UFFunction(uint8_t *data, size_t len);


void update_started()
{
  Serial.println("[Update] HTTP update process started");
}

void update_finished()
{
  Serial.print("\n");
  Serial.println("[Update] HTTP update process finished");
}

void update_progress(int cur, int total)
{
  int progress = cur * 100 / total;
  Serial.printf("[Update] HTTP update process at %d of %d bytes. Total Progress: %d % \r", cur, total, progress);
}

void update_error(int err)
{
  Serial.printf("[Update] HTTP update fatal error code %d\r", err);
}

String split(String s, char parser, int index)
{
  String rs = "";
  int parserIndex = index;
  int parserCnt = 0;
  int rFromIndex = 0, rToIndex = -1;
  while (parserIndex >= parserCnt)
  {
    rFromIndex = rToIndex + 1;
    rToIndex = s.indexOf(parser, rFromIndex);
    if (parserIndex == parserCnt)
    {
      if (rToIndex == 0 || rToIndex == -1)
        return "";
      return s.substring(rFromIndex, rToIndex);
    }
    else
      parserCnt++;
  }
  return rs;
}

String checkUpdate()
{
  if (varCheck() == false)
  {
    return "NO_VARIABLES_SET";
  }
  else
  {
    Serial.println("[Update] Update Check Started!\r");
    Serial.println("[Update] Creating initlink...");
    if (beta == false and dev == false)
    {
      Serial.println("[Update] Stable selected");
      initlink = dllink + "init";
    }
    else if (dev == true)
    {
      Serial.println("[Update] Developer mode selected");
      Serial.println("[Update] Downloading from Devtag!");
      binlink = devlink + DEVTAG + "/firmware.bin";
      Serial.println("[Update] Link created: " + binlink);
      return "UPDATE_AVAILABLE";
    }
    else if (beta == true)
    {
      Serial.println("[Update] Beta selected");
      initlink = dllink + "beta/init";
    }
    Serial.println("[Update] Link created: " + initlink);
    http.begin(updatewificlient, initlink);
    Serial.println("[Update] Connecting to Update Server...");
    serverstatus = http.GET();
    if (serverstatus == 200)
    {
      strinit = http.getString();
      http.end();
      String name = split(strinit, '!', 0);
      String filename = split(strinit, '!', 1);
      String newversion = split(strinit, '!', 2);
      Serial.println("[Update] Successfully connected to the update server!\r");
      Serial.println("[Update] ***READING UPDATE INFOS***");
      Serial.println("[Update] Name of the device: " + name);
      Serial.println("[Update] Update Version: " + newversion);
      Serial.println("[Update] ***READING SYSTEM SETTINGS***");
      Serial.println("[Update] Device Name: " + DEVICENAME);
      Serial.println("[Update] Installed Version: " + VERSION);
      Serial.println("[Update] Beta enabled: " + String(beta));
      Serial.println("[Update] Dev enabled: " + String(dev));
      if (dev == true)
      {
        Serial.println("[Update] DevTag: " + DEVTAG);
      }
      if (VERSION == newversion)
      {
        Serial.println("[Update] No Update available!\r");
        return "NO_UPDATE_AVAILABLE";
      }
      else
      {
        Serial.println("[Update] Update available!");
        Serial.println("[Update] Creating DLLink: ");
        if (beta == true)
        {
          binlink = dllink + "beta/" + filename + newversion + ".bin";
        }
        else
        {
          binlink = dllink + filename + newversion + ".bin";
        }
        Serial.println("[Update] " + binlink);
        return "UPDATE_AVAILABLE";
      }
    }
    else
    {
      http.end();
      Serial.println("[Update] ERROR: UPDATE SERVER DOWN!");
      return "UPDATE_SERVER_DOWN";
    }
  }
}

String performUpdate()
{
  if (varCheck() == false)
  {
    return "NO_VARIABLES_SET";
  }
  else
  {
    http.begin(binlink);
    if (http.GET() != 200)
    {
      Serial.println("[Update] HTTP Error: File not found or connection broken.");
      return "HTTP_ERROR";
    }
    else
    {
      Serial.println("[Update] Init Update...");
      totalLength = http.getSize();
      int len = totalLength;
      Update.begin(UPDATE_SIZE_UNKNOWN);
      Serial.printf("[Update] FW Size: %u\n", totalLength);
      uint8_t buff[128] = {0};
      WiFiClient *stream = http.getStreamPtr();
      Serial.println("[Update] Updating firmware...");
      while (http.connected() && (len > 0 || len == -1))
      {
        // get available data size
        size_t size = stream->available();
        if (size)
        {
          // read up to 128 byte
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          // pass to function
          UFFunction(buff, c);
          if (len > 0)
          {
            len -= c;
          }
        }
        delay(1);
      }
    }
    http.end();
  }

  return "HTTP_UPDATE_OK";
}


String dataTransmission()
{
  String postData = dllink + "datareceive.php?mac=" + WiFi.macAddress() + "&devicename=" + DEVICENAME + "&fwver=" + VERSION;
  http.begin(updatewificlient, postData);
  int httpCode = http.GET();
  if (httpCode == 200)
  {
    Serial.println("[Update] UserData successfully transmitted!");
    return "SUCCESS";
  }
  else
  {
    Serial.println("[Update] Error transmitting UserData!");
    return "ERROR";
  }
}

void connectWIFI(String ssid, String passwd)
{
  Serial.println("[WIFI] Connecting to WIFI...");
  SSID = ssid;
  PASSWORD = passwd;
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(ssid.c_str(), passwd.c_str());
}

bool checkWIFI()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    return false;
  }
  else
  {
    return true;
  }
}

void resetWIFI()
{
  WiFi.disconnect();
  SSID = "";
  PASSWORD = "";
  EEPROM.begin(512);
  for (int i = 0; i < 96; ++i)
  {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  Serial.println("[WIFI] Successfully reset WiFi!");
}
void disconnectWIFI() {
    WiFi.disconnect();
    Serial.println("[WIFI] Successfully disconnected from WIFI");
}
void saveWIFI(String ssid, String password)
{
  EEPROM.begin(512);
  Serial.println("[WIFI] Writing SSID to EEPROM:");
  for (int i = 0; i < ssid.length(); ++i)
  {
    EEPROM.write(i, ssid[i]);
    Serial.print(ssid[i]);
  }
  Serial.println("[WIFI] Done writing SSID to EEPROM.");
  Serial.println("[WIFI] Writing Password to EEPROM:");
  for (int i = 0; i < password.length(); ++i)
  {
    EEPROM.write(32 + i, password[i]);
    Serial.print(password[i]);
  }
  EEPROM.commit();
  Serial.println("[WIFI] Done writing SSID to EEPROM.");
}
String loadWIFI(String mode)
{
  EEPROM.begin(512);
  if (mode == "ssid" or mode == "SSID")
  {
    String esid;
    for (int i = 0; i < 32; ++i)
    {
      esid += char(EEPROM.read(i));
    }
    SSID = esid.c_str();
    Serial.print("[WIFI] Loaded SSID from EEPROM: ");
    Serial.println(SSID);
    return SSID;
  }
  else if (mode == "password" or mode == "PASSWORD" or mode == "pass" or mode == "PASS")
  {
    String epass;
    for (int i = 32; i < 96; ++i)
    {
      epass += char(EEPROM.read(i));
    }
    PASSWORD = epass.c_str();
    Serial.print("[WIFI] Loaded Password from EEPROM: ");
    Serial.println(PASSWORD);
    return PASSWORD;
  }
  else
  {
    return "";
  }
}

void connectWIFIUser(String ssid, String password)
{
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Serial.println("[WIFI] Scan done");
  if (n == 0)
  {
    Serial.println("[WIFI] No networks found");
  }
  else
  {
    Serial.print("[WIFI] ");
    Serial.print(n);
    Serial.println(" networks found!");
  }
  Serial.println("");
  st = "<ol>";
  for (int i = 0; i < n; ++i)
  {
    // Print SSID and RSSI for each network found
    st += "<li>";
    st += WiFi.SSID(i);
    st += " (";
    st += WiFi.RSSI(i);

    st += ")";
    st += (WiFi.encryptionType(i)) ? " " : "*";
    st += "</li>";
  }
  st += "</ol>";
  delay(100);
  ssid = ssid + "-" + String(random(999));
  WiFi.softAP(ssid.c_str(), password.c_str());
  Serial.println("[WIFI] Starting user config mode...");
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED)
    Serial.println("[WIFI] Accesspoint created!");
  Serial.print("[WIFI] Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("[WIFI] SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
  createWebServer();
  // Start the server
  server.begin();
  Serial.println("[WIFI] Server started");
  Serial.println("[WIFI] User config mode successfully started!");
}

void createWebServer()
{
  {
    server.on("/", []()
              {
                IPAddress ip = WiFi.softAPIP();
                String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
                content = "<!DOCTYPE HTML>\r\n<html>R-Lib8266 " + LIBVERSION + " by Tim Fischbach at ";
                content += ipStr;
                content += "<form action=\"/scan\" method=\"POST\"><input type=\"submit\" value=\"scan\"></form>";
                content += "<p>";
                content += st;
                content += "</p><form method='get' action='setting'><label>SSID: </label><input name='ssid' length=32><label> Password: </label><input name='pass' length=64><input type='submit'></form>";
                content += "</html>";
                server.send(200, "text/html", content); });
    server.on("/scan", []()
              {
                //setupAP();
                IPAddress ip = WiFi.softAPIP();
                String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

                content = "<!DOCTYPE HTML>\r\n<html>Scan success! Please go back!";
                content += "<form action=\"/\" method=\"POST\"><input type=\"submit\" value=\"back\"></form>";
                server.send(200, "text/html", content); });

    server.on("/setting", []()
              {
                String qsid = server.arg("ssid");
                String qpass = server.arg("pass");
                if (qsid.length() > 0 && qpass.length() > 0)
                {
                  Serial.println("[WIFI] Clearing EEPROM");
                  for (int i = 0; i < 96; ++i)
                  {
                    EEPROM.write(i, 0);
                  }
                  EEPROM.commit();
                  saveWIFI(qsid, qpass);

                  content = "{\"Success\":\"saved to eeprom... reset to boot into new wifi\"}";
                  statusCode = 200;
                  ESP.restart();
                }
                else
                {
                  content = "{\"Error\":\"404 not found\"}";
                  statusCode = 404;
                  Serial.println("Sending 404");
                }
                server.sendHeader("Access-Control-Allow-Origin", "*");
                server.send(statusCode, "application/json", content); });
  }
}

String getLibVersion()
{
  return LIBVERSION;
}

void connectWIFIUserHandle()
{
  server.handleClient();
}

void endWIFIUser()
{
  server.stop();
}

void setDeviceName(String DevName)
{
  DEVICENAME = DevName;
}

String getDeviceName()
{
  return DEVICENAME;
}

void setVersion(String ver)
{
  VERSION = ver;
}

String getVersion()
{
  return VERSION;
}

void setDlLink(String DLL)
{
  dllink = DLL;
}

String getDlLink()
{
  return dllink;
}

void setDevLink(String DEL)
{
  devlink = DEL;
}

String getDevLink()
{
  return devlink;
}

void setDevTag(String DTAG)
{
  DEVTAG = DTAG;
}

String getDevTag()
{
  return DEVTAG;
}

void setBetaState(bool sbeta)
{
  beta = sbeta;
}

bool getBetaState()
{
  return beta;
}

void setDevState(bool sdev)
{
  dev = sdev;
}

bool getDevState()
{
  return dev;
}

bool varCheck()
{
  if (dev == false)
  {
    if (DEVICENAME.length() == 0 or VERSION.length() == 0 or dllink.length() == 0)
    {
      return false;
    }
    else
    {
      return true;
    }
  }
  else
  {
    if (DEVICENAME.length() == 0 or VERSION.length() == 0 or dllink.length() == 0 or devlink.length() == 0 or DEVTAG.length() == 0)
    {
      return false;
    }
    else
    {
      return true;
    }
  }
}

void UFFunction(uint8_t *data, size_t len)
{
  Update.write(data, len);
  currentLength += len;
  // Print dots while waiting for update to finish
  Serial.print('.');
  // if current length of written firmware is not equal to total firmware size, repeat
  if (currentLength != totalLength)
    return;
  Update.end(true);
  Serial.printf("\n[Update] Update Success, Total Size: %u\nRebooting...\n", currentLength);
  // Restart ESP32 to see changes
  ESP.restart();
}
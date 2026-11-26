#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

const char* host = "esp32";
const char* ssid = "YOUR_WIFI_SSID";         // Enter your WiFi SSID
const char* password = "YOUR_WIFI_PASSWORD"; // Enter your WiFi Password

// Secure Web Login Credentials
const char* www_username = "admin";
const char* www_password = "admin";

WebServer server(80);

/*
- Server Index Page (Using Raw String Literals to fix quote errors)
 */
const char* serverIndex = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js"></script>
</head>
<body style="font-family: Arial; text-align: center; margin-top: 50px;">
  <h2>ESP32 Secure OTA Web Updater</h2>
  <form method="POST" action="#" enctype="multipart/form-data" id="upload_form">
    <input type="file" name="update" accept=".bin">
    <input type="submit" value="Update Firmware">
  </form>
  <br>
  <div id="prg" style="font-weight: bold;">Progress: 0%</div>
  <script>
    $('form').submit(function(e){
      e.preventDefault();
      var form = $('#upload_form')[0];
      var data = new FormData(form);
      $.ajax({
        url: '/update',
        type: 'POST',
        data: data,
        contentType: false,
        processData: false,
        xhr: function() {
          var xhr = new window.XMLHttpRequest();
          xhr.upload.addEventListener('progress', function(evt) {
            if (evt.lengthComputable) {
              var per = evt.loaded / evt.total;
              $('#prg').html('Progress: ' + Math.round(per*100) + '%');
            }
          }, false);
          return xhr;
        },
        success:function(d, s) {
          $('#prg').html('Update Success! Rebooting...');
          console.log('success!');
        },
        error: function (a, b, c) {
          $('#prg').html('Update Failed!');
        }
      });
    });
  </script>
</body>
</html>
)rawliteral";

void setup(void) {
  // Setup code that runs once
  pinMode(5, OUTPUT);
  Serial.begin(115200);
  
  // Connect to WiFi network
  WiFi.begin(ssid, password);
  Serial.println("");
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup mDNS
  if (!MDNS.begin(host)) { 
    Serial.println("Error setting up MDNS responder!");
    while (1) { delay(1000); }
  }
  Serial.println("mDNS responder started");

  // Root URL redirects to serverIndex
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Location", "/serverIndex");
    server.send(302, "text/plain", "");
  });

  // Secure Server Index Route
  server.on("/serverIndex", HTTP_GET, []() {
    if (!server.authenticate(www_username, www_password)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

  // Secure Update Handling
  server.on("/update", HTTP_POST, []() {
    if (!server.authenticate(www_username, www_password)) {
      return server.requestAuthentication();
    }
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    if (!server.authenticate(www_username, www_password)) {
      return; 
    }
    
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { 
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { 
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  
  server.begin();
}

void loop(void) {
  server.handleClient();
  
  // Continuous loop code
  digitalWrite(5, HIGH);
  delay(1000);
  digitalWrite(5, LOW);
  delay(1000);
}

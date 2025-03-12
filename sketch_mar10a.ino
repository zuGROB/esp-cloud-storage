#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <SD.h>

// Wi-Fi credentials go brrrr
const char* ssid = "DIR-615-0bd1";
const char* password = "31415926";

// AsyncWebServer on port 8008 (like "boob" hehehehehe)
AsyncWebServer server(8008);

// Global variables, yada-yada
unsigned long bootTime;
String chatLog = "";
File uploadFile;
bool uploadError = false;

// Function to get used bytes on SD card
uint64_t getUsedBytes() {
  uint64_t used = 0;
  File root = SD.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        used += file.size();
      }
      file.close();
      file = root.openNextFile();
    }
  }
  return used;
}

// Handler for the main page
void handleRoot(AsyncWebServerRequest *request) {
  // SD card memory status
  uint64_t totalBytes = SD.cardSize();
  uint64_t usedBytes = getUsedBytes();
  uint64_t freeBytes = totalBytes - usedBytes;
  
  String totalMB = String(totalBytes / (1024 * 1024)) + " MB";
  String freeMB = String(freeBytes / (1024 * 1024)) + " MB";
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>not-yet-Cloud 1.0</title>
  <link href="https://fonts.googleapis.com/css?family=Roboto:400,700" rel="stylesheet">
  <style>
    body {
      font-family: 'Roboto', sans-serif;
      background: linear-gradient(135deg, #89f7fe, #66a6ff);
      margin: 0;
      padding: 0;
      color: #333;
    }
    header {
      text-align: center;
      padding: 20px;
      background: rgba(255, 255, 255, 0.8);
      box-shadow: 0 2px 4px rgba(0,0,0,0.1);
    }
    h1 {
      margin: 0;
      font-size: 2.8em;
    }
    section {
      margin: 20px auto;
      padding: 20px;
      max-width: 900px;
      background: #fff;
      border-radius: 8px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.1);
    }
    .timer {
      font-size: 1.5em;
      text-align: center;
    }
    .chat-box {
      height: 250px;
      overflow-y: auto;
      border: 1px solid #ddd;
      padding: 10px;
      background: #f4f4f4;
      border-radius: 4px;
      margin-bottom: 10px;
    }
    input[type="text"], input[type="file"] {
      width: 100%;
      padding: 10px;
      margin: 5px 0 15px;
      border: 1px solid #ccc;
      border-radius: 4px;
      box-sizing: border-box;
    }
    .button {
      padding: 10px 20px;
      background: #28a745;
      color: #fff;
      border: none;
      border-radius: 4px;
      cursor: pointer;
      font-size: 1em;
      transition: background 0.3s;
    }
    .button:hover {
      background: #218838;
    }
    ul {
      list-style-type: none;
      padding: 0;
    }
    li {
      margin-bottom: 10px;
      display: flex;
      align-items: center;
      justify-content: space-between;
    }
    .file-info {
      flex-grow: 1;
    }
    .status {
      margin-left: 10px;
      font-size: 0.9em;
      color: #555;
    }
  </style>
</head>
<body>
  <header>
    <h1>TrueCloud 1.0 (not yet in air)</h1>
    <p>The first real cloud storage in the sky! (not yet)</p>
  </header>
  <section>
    <h2>Uptime</h2>
    <div class="timer" id="timer">Loading...</div>
  </section>
  <section>
    <h2>Chat</h2>
    <div class="chat-box" id="chatLog">)rawliteral";
  html += chatLog;
  html += R"rawliteral(
    </div>
    <form id="chatForm" method="POST" action="/chat">
      <input type="text" name="msg" placeholder="Enter your message" required>
      <button type="submit" class="button">Send</button>
    </form>
  </section>
  
  <section>
    <h2>File Manager</h2>
    <p>Memory status: Free <strong>)rawliteral";
  html += freeMB;
  html += R"rawliteral(</strong> out of <strong>)rawliteral";
  html += totalMB;
  html += R"rawliteral(</strong></p>
    <h3>Files:</h3>
    <ul>
)rawliteral";
  // List files from SD card
  File root = SD.open("/");
  if (root) {
    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        String fileName = String(file.name());
        // Make a safe ID for HTML by removing slash, whatever i had to do???
        String safeName = fileName;
        safeName.replace("/", "");
        html += "<li><span class='file-info'>" + fileName +
                "</span> <button onclick=\"downloadFileJS('" + fileName + "')\" class='button'>Download</button> <span id='downloadStatus_" + safeName + "' class='status'></span></li>";
      }
      file.close();
      file = root.openNextFile();
    }
  }
  html += R"rawliteral(
    </ul>
    <h3>Upload File</h3>
    <input type="file" id="fileInput">
    <button onclick="uploadFileJS()" class="button">Upload</button>
    <div id="uploadStatus" class="status"></div>
  </section>
  <script>
    // Update uptime every second
    function updateTimer() {
      fetch('/time')
        .then(response => response.text())
        .then(data => { document.getElementById('timer').innerText = data; });
    }
    setInterval(updateTimer, 1000);
    updateTimer();
    
    // Upload file with progress
    function uploadFileJS() {
      var fileInput = document.getElementById('fileInput');
      if (!fileInput.files.length) { alert("Select a file to upload"); return; }
      var file = fileInput.files[0];
      var xhr = new XMLHttpRequest();
      var startTime = Date.now();
      xhr.open("POST", "/upload", true);
      xhr.upload.onprogress = function(e) {
        if(e.lengthComputable) {
          var percent = (e.loaded / e.total * 100).toFixed(2);
          var elapsed = (Date.now() - startTime) / 1000;
          var speed = (e.loaded / 1024 / elapsed).toFixed(2); // KB/s
          document.getElementById('uploadStatus').innerText = "Uploaded: " + percent + "%, speed: " + speed + " KB/s";
        }
      };
      xhr.onload = function() {
        if(xhr.status == 200 || xhr.status == 303) {
          document.getElementById('uploadStatus').innerText = "Upload complete!";
          setTimeout(() => location.reload(), 1000);
        } else {
          document.getElementById('uploadStatus').innerText = "Upload error: " + xhr.status;
        }
      };
      var formData = new FormData();
      formData.append("upload", file);
      xhr.send(formData);
    }
    
    // Download file with progress
    function downloadFileJS(fileName) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/download?file=" + encodeURIComponent(fileName), true);
      xhr.responseType = "blob";
      var startTime = Date.now();
      var safeName = fileName.replace("/", "");
      xhr.onprogress = function(e) {
        if(e.lengthComputable) {
          var percent = (e.loaded / e.total * 100).toFixed(2);
          var elapsed = (Date.now() - startTime) / 1000;
          var speed = (e.loaded / 1024 / elapsed).toFixed(2);
          document.getElementById('downloadStatus_' + safeName).innerText = "Downloaded: " + percent + "%, speed: " + speed + " KB/s";
        }
      };
      xhr.onload = function() {
        if(xhr.status == 200) {
          document.getElementById('downloadStatus_' + safeName).innerText = "Download complete!";
          var url = window.URL.createObjectURL(xhr.response);
          var a = document.createElement('a');
          a.href = url;
          a.download = fileName;
          document.body.appendChild(a);
          a.click();
          a.remove();
          window.URL.revokeObjectURL(url);
        } else {
          document.getElementById('downloadStatus_' + safeName).innerText = "Download error: " + xhr.status;
        }
      };
      xhr.send();
    }
  </script>
</body>
</html>
  )rawliteral";
  
  request->send(200, "text/html", html);
}

// Chat handler (POST)
void handleChat(AsyncWebServerRequest *request) {
  if (request->hasArg("msg")) {
    String msg = request->arg("msg");
    unsigned long elapsed = (millis() - bootTime) / 1000;
    String timeStr = String(elapsed) + "s";
    chatLog += "<p>[" + timeStr + "] " + msg + "</p>";
  }
  request->redirect("/");
}

// Uptime handler (/time)
void handleTime(AsyncWebServerRequest *request) {
  unsigned long elapsed = (millis() - bootTime) / 1000;
  unsigned long hours = elapsed / 3600;
  unsigned long minutes = (elapsed % 3600) / 60;
  unsigned long seconds = elapsed % 60;
  char buffer[20];
  sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
  request->send(200, "text/plain", buffer);
}

// File roulette handler (/roulette) (DEPRECAED AND REMOVED FROM HTML BECAUSE IT WONT WORK FUUUUUUUUCK)
void handleRoulette(AsyncWebServerRequest *request) {
  File root = SD.open("/");
  if (!root) {
    request->send(500, "text/plain", "Failed to open SD card root");
    return;
  }
  const int maxFiles = 50;
  String files[maxFiles];
  int fileCount = 0;
  File file = root.openNextFile();
  while (file && fileCount < maxFiles) {
    if (!file.isDirectory()) {
      files[fileCount] = String(file.name());
      fileCount++;
    }
    file.close();
    file = root.openNextFile();
  }
  if (fileCount == 0) {
    request->send(404, "text/plain", "No files found");
    return;
  }
  int idx = random(fileCount);
  String fileName = files[idx];
  // it wont work too
  request->redirect("/download?file=" + fileName);
}

// Download handler (/download)
void handleDownload(AsyncWebServerRequest *request) {
  if (!request->hasParam("file")) {
    request->send(400, "text/plain", "Missing file parameter");
    return;
  }
  const AsyncWebParameter* p = request->getParam("file");
  String origFileName = p->value();
  
  String safeFileName = origFileName;
  if (safeFileName.charAt(0) == '/') {
    safeFileName = safeFileName.substring(1);
  }
  
  String filePath = origFileName;
  if (filePath.charAt(0) != '/') {
    filePath = "/" + filePath;
  }
  
  File downloadFile = SD.open(filePath, FILE_READ);
  if (!downloadFile) {
    request->send(404, "text/plain", "File not found");
    return;
  }
  request->send(downloadFile, safeFileName, "application/octet-stream");
}
// Example FreeRTOS task for multithreading (turns out - useless shit)
void backgroundTask(void * parameter) {
  while (1) {
    Serial.println("Background task running on core: " + String(xPortGetCoreID()));
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  bootTime = millis();
  
  if (!SD.begin(5)) {
    Serial.println("SD Card initialization failed!");
  } else {
    Serial.println("SD Card initialized.");
  }

  // init SD BEFORE!!!!!! INITING WIFI, OR IT FUCKING BREAKS!!
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
  
  xTaskCreatePinnedToCore(
    backgroundTask,   // Task function
    "BackgroundTask", // Task name
    10000,            // Stack size
    NULL,             // Parameters
    1,                // Priority
    NULL,             // Task handle
    0                 // Core (0 or 1)
  );
  
  // Set up AsyncWebServer routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/chat", HTTP_POST, handleChat);
  server.on("/time", HTTP_GET, handleTime);
  server.on("/roulette", HTTP_GET, handleRoulette);
  server.on("/download", HTTP_GET, handleDownload);
  
  // Handle file upload via asynchronous onFileUpload callback
  server.on(
  "/upload", HTTP_POST,
  [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Upload Complete");
  },
  [](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (index == 0) {
      Serial.printf("Upload Start: %s\n", filename.c_str());
      String path = "/" + filename;
      if (SD.exists(path)) {
        SD.remove(path);
      }
      uploadFile = SD.open(path, FILE_WRITE);
      if (!uploadFile) {
        Serial.printf("Failed to open file for writing: %s\n", path.c_str());
        return;
      }
    }
    if (uploadFile) {
      uploadFile.write(data, len);
    }
    if (final) {
      if (uploadFile) {
        uploadFile.close();
      }
      Serial.printf("Upload End: %s\n", filename.c_str());
    }
  }
);
  
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // its async, fuck you, loop())
}

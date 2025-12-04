#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SPIFFS.h>

#include "md4c-html.h"
#include "content_parser.h"

const char* ssid = "EduBridge";  // Name of the WiFi
const char* password = "";       // No password for open access (or set one if preferred)

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1); // The default IP for ESP AP
DNSServer dnsServer;
WebServer server(80);
ContentParser contentParser;

String html = "";

// Callback function to process md4c HTML output
void process_output(const MD_CHAR* text, MD_SIZE size, void* userdata) {
  // Cast userdata back to String pointer
  String* output = (String*)userdata;
  
  // Append the text chunk to the output string
  for (MD_SIZE i = 0; i < size; i++) {
    *output += text[i];
  }
}

void convertMarkdownToHtml(const char* markdown) {
  html = ""; // Clear previous content
  
  // Call md_html with our callback function
  // The &html is passed as userdata so the callback can append to it
  int result = md_html(
    markdown,                    // input markdown text
    strlen(markdown),            // input size
    process_output,              // callback function
    &html,                       // userdata (pointer to our html String)
    0,                          // parser_flags (0 for default)
    0                           // renderer_flags (0 for default)
  );
  
  if (result != 0) {
    Serial.println("Error converting markdown to HTML");
  }
}

// --- The HTML/JS Quiz Content ---
// We store the HTML in a raw string literal for easy editing
const char index_html[] PROGMEM = R"rawliteral(
  
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>EduBridge Quiz</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; padding: 20px; }
    .container { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); max-width: 600px; margin: auto; }
    h1 { color: #2c3e50; }
    .question { margin: 20px 0; text-align: left; }
    button { background-color: #27ae60; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }
    button:hover { background-color: #219150; }
    #result { margin-top: 20px; font-weight: bold; font-size: 1.2em; }
    .footer { margin-top: 30px; font-size: 0.8em; color: #777; }
  </style>
</head>
<body>

<div class="container">
  <h1>EduBridge Knowledge Check</h1>
  <p>Answer the questions below to test your skills.</p>
  <hr>

  <form id="quizForm">
    <div class="question">
      <p>1. What does HTML stand for?</p>
      <input type="radio" name="q1" value="a"> Hyper Text Markup Language<br>
      <input type="radio" name="q1" value="b"> High Tech Modern Language<br>
      <input type="radio" name="q1" value="c"> Hyper Transfer Mode Link<br>
    </div>

    <div class="question">
      <p>2. Which component is the 'brain' of the computer?</p>
      <input type="radio" name="q2" value="a"> RAM<br>
      <input type="radio" name="q2" value="b"> CPU<br>
      <input type="radio" name="q2" value="c"> Hard Drive<br>
    </div>

    <div class="question">
      <p>3. In programming, what is a 'Loop'?</p>
      <input type="radio" name="q3" value="a"> An error in the code<br>
      <input type="radio" name="q3" value="b"> A variable that stores text<br>
      <input type="radio" name="q3" value="c"> A sequence that repeats<br>
    </div>

    <button type="button" onclick="gradeQuiz()">Submit Answers</button>
  </form>

  <div id="result"></div>
</div>

<div class="footer">Powered by ESP32 EduBridge</div>

<script>
  // This is the JavaScript Logic
  function gradeQuiz() {
    var score = 0;
    var total = 3;
    var form = document.forms['quizForm'];
    
    // Check Answers
    if(form.elements['q1'].value == 'a') score++;
    if(form.elements['q2'].value == 'b') score++;
    if(form.elements['q3'].value == 'c') score++;

    // Display Result
    var resultDiv = document.getElementById('result');
    resultDiv.innerHTML = "You scored " + score + " out of " + total;
    
    if(score == total) {
      resultDiv.style.color = "green";
      resultDiv.innerHTML += "<br>Excellent work!";
    } else {
      resultDiv.style.color = "red";
      resultDiv.innerHTML += "<br>Keep studying!";
    }
  }
</script>

</body>
</html>

)rawliteral";

void setup() {
  Serial.begin(115200);
  
  // Initialize content parser and load modules
  Serial.println("Initializing content parser...");
  if (contentParser.initialize()) {
    contentParser.loadModules();
    
    // Print loaded modules info
    for (int i = 0; i < contentParser.getModuleCount(); i++) {
      contentParser.printModuleInfo(i);
    }
  } else {
    Serial.println("Failed to initialize content parser");
  }
  
  // 1. Set up the Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password); // If password is "", it is an open network
  
  // Wait a moment for AP to start
  delay(100);
  
  Serial.println("Access Point Started");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("IP Address: "); Serial.println(WiFi.softAPIP());

  // 2. Start DNS Server (Redirect all traffic to this device)
  // The "*" forces all domain requests to resolve to the ESP's IP
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // 3. Setup Web Server Handlers
  
  // Serve module list on root path
  server.on("/", []() {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;padding:20px;background:#f4f4f9;}";
    html += ".module{background:white;padding:15px;margin:10px 0;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += "a{text-decoration:none;color:#27ae60;font-weight:bold;}</style></head><body>";
    html += "<h1>EduBridge - Available Modules</h1>";
    
    for (int i = 0; i < contentParser.getModuleCount(); i++) {
      Module* mod = contentParser.getModule(i);
      if (mod && mod->isValid) {
        html += "<div class='module'>";
        html += "<h2>" + mod->name + "</h2>";
        html += "<p>Lessons: " + String(mod->lessonCount) + "</p>";
        html += "<a href='/module?id=" + mod->id + "'>View Module</a>";
        html += "</div>";
      }
    }
    
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  
  // Serve specific module with lessons
  server.on("/module", []() {
    if (!server.hasArg("id")) {
      server.send(400, "text/plain", "Missing module id");
      return;
    }
    
    String moduleId = server.arg("id");
    Module* mod = contentParser.getModuleById(moduleId);
    
    if (!mod || !mod->isValid) {
      server.send(404, "text/plain", "Module not found");
      return;
    }
    
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;padding:20px;background:#f4f4f9;}";
    html += ".lesson{background:white;padding:15px;margin:10px 0;border-radius:8px;}";
    html += "a{text-decoration:none;color:#27ae60;font-weight:bold;margin:0 10px;}</style></head><body>";
    html += "<a href='/'>← Back to Modules</a>";
    html += "<h1>" + mod->name + "</h1>";
    
    // List lessons
    for (int i = 0; i < mod->lessonCount; i++) {
      Lesson& lesson = mod->lessons[i];
      if (lesson.isValid) {
        html += "<div class='lesson'>";
        html += "<h3>Lesson " + String(lesson.id) + ": " + lesson.title + "</h3>";
        html += "<a href='/lesson?module=" + mod->id + "&lesson=" + String(lesson.id) + "'>View Lesson</a>";
        html += "</div>";
      }
    }
    
    // Quiz link
    if (mod->hasQuiz) {
      html += "<div class='lesson'>";
      html += "<h3>📝 Module Quiz</h3>";
      html += "<p>" + String(mod->quizQuestionCount) + " questions</p>";
      html += "<a href='/quiz?module=" + mod->id + "'>Take Quiz</a>";
      html += "</div>";
    }
    
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  
  // Serve specific lesson content
  server.on("/lesson", []() {
    if (!server.hasArg("module") || !server.hasArg("lesson")) {
      server.send(400, "text/plain", "Missing parameters");
      return;
    }
    
    String moduleId = server.arg("module");
    int lessonId = server.arg("lesson").toInt();
    
    Module* mod = contentParser.getModuleById(moduleId);
    if (!mod || !mod->isValid) {
      server.send(404, "text/plain", "Module not found");
      return;
    }
    
    Lesson* lesson = nullptr;
    for (int i = 0; i < mod->lessonCount; i++) {
      if (mod->lessons[i].id == lessonId) {
        lesson = &mod->lessons[i];
        break;
      }
    }
    
    if (!lesson || !lesson->isValid) {
      server.send(404, "text/plain", "Lesson not found");
      return;
    }
    
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;padding:20px;background:#f4f4f9;max-width:800px;margin:auto;}";
    html += ".content{background:white;padding:20px;border-radius:8px;line-height:1.6;}";
    html += "a{text-decoration:none;color:#27ae60;font-weight:bold;}</style></head><body>";
    html += "<a href='/module?id=" + mod->id + "'>← Back to " + mod->name + "</a>";
    html += "<div class='content'>";
    html += lesson->content;
    html += "</div>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  
  // Serve quiz
  server.on("/quiz", []() {
    if (!server.hasArg("module")) {
      server.send(400, "text/plain", "Missing module id");
      return;
    }
    
    String moduleId = server.arg("module");
    Module* mod = contentParser.getModuleById(moduleId);
    
    if (!mod || !mod->isValid || !mod->hasQuiz) {
      server.send(404, "text/plain", "Quiz not found");
      return;
    }
    
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;padding:20px;background:#f4f4f9;max-width:800px;margin:auto;}";
    html += ".quiz{background:white;padding:20px;border-radius:8px;}";
    html += ".question{margin:20px 0;padding:15px;background:#f9f9f9;border-left:4px solid #27ae60;}";
    html += "button{background:#27ae60;color:white;padding:12px 30px;border:none;border-radius:5px;cursor:pointer;font-size:16px;}";
    html += "button:hover{background:#219150;}";
    html += "#result{margin-top:20px;padding:15px;border-radius:5px;font-weight:bold;font-size:1.2em;}";
    html += "a{text-decoration:none;color:#27ae60;font-weight:bold;}</style></head><body>";
    html += "<a href='/module?id=" + mod->id + "'>← Back to " + mod->name + "</a>";
    html += "<div class='quiz'>";
    html += "<h1>" + mod->name + " - Quiz</h1>";
    html += contentParser.generateQuizHtml(*mod);
    html += "</div>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });


  // AND Serve the quiz on ANY unknown path (This is the Captive Portal trick)
  // When a phone checks "connectivitycheck.gstatic.com", we send them the quiz instead.
//  server.onNotFound([]() {
//    server.send(200, "text/html", index_html);
//  });

  server.begin();
  Serial.println("Web Server Started");
}

void loop() {
  // Process DNS requests (redirecting traffic)
  dnsServer.processNextRequest();
  // Process Web requests (serving the quiz)
  server.handleClient();
}
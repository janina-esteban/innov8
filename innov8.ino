#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "md4c-html.h"

const char* ssid = "EduBridge";  // Name of the WiFi
const char* password = "";       // No password for open access (or set one if preferred)

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1); // The default IP for ESP AP
DNSServer dnsServer;
WebServer server(80);

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
  
  // 1. Set up the Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password); // If password is "", it is an open network
  
  // Wait a moment for AP to start
  delay(100);
  
  convertMarkdownToHtml("# Admin view");
  Serial.println(html.c_str());
  
  Serial.println("Access Point Started");
  Serial.print("SSID: "); Serial.println(ssid);
  Serial.print("IP Address: "); Serial.println(WiFi.softAPIP());

  // 2. Start DNS Server (Redirect all traffic to this device)
  // The "*" forces all domain requests to resolve to the ESP's IP
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // 3. Setup Web Server Handlers
  
  // Serve the quiz on the root path
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/admin", []() {
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
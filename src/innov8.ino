#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "md4c-html.h"
#include "content_parser.h"

const char* ssid = "EduBridge";
const char* password = "";

const byte DNS_PORT = 53;
DNSServer dnsServer;
WebServer server(80);
ContentParser contentParser;

void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;padding:20px;}.mod{background:#eee;padding:15px;margin-bottom:10px;border-radius:5px;}</style></head><body><h1>Available Modules</h1>";
  
  if (contentParser.getModuleCount() == 0) {
      html += "<p>No modules loaded. Check filesystem.</p>";
  }

  for (int i = 0; i < contentParser.getModuleCount(); i++) {
    Module* m = contentParser.getModule(i);
    html += "<div class='mod'><h2>" + m->name + "</h2>";
    html += "<p>Lessons: " + String(m->lessonCount) + "</p>";
    html += "<a href='/module?id=" + m->id + "'>Open Module</a></div>";
  }
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleModule() {
  if (!server.hasArg("id")) { server.send(400, "text/plain", "Missing ID"); return; }
  Module* m = contentParser.getModuleById(server.arg("id"));
  if (!m) { server.send(404, "text/plain", "Module Not Found"); return; }

  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;padding:20px;} a{display:block;margin:10px 0;font-size:18px;}</style></head><body>";
  html += "<a href='/'>&larr; Back</a><h1>" + m->name + "</h1>";
  
  for(int i=0; i<m->lessonCount; i++) {
      html += "<a href='/lesson?module=" + m->id + "&lesson=" + String(m->lessons[i].id) + "'>📄 " + m->lessons[i].title + "</a>";
  }
  if(m->hasQuiz) html += "<hr><a href='/quiz?module=" + m->id + "'>📝 Take Quiz</a>";
  
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleLesson() {
  if (!server.hasArg("module") || !server.hasArg("lesson")) { server.send(400, "text/plain", "Bad Request"); return; }
  Module* m = contentParser.getModuleById(server.arg("module"));
  if (!m) return;
  
  int lid = server.arg("lesson").toInt();
  for(int i=0; i<m->lessonCount; i++) {
      if(m->lessons[i].id == lid) {
          String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;padding:20px;line-height:1.6;}</style></head><body>";
          html += "<a href='/module?id=" + m->id + "'>&larr; Back</a>";
          html += m->lessons[i].content; // Already HTML
          html += "</body></html>";
          server.send(200, "text/html", html);
          return;
      }
  }
  server.send(404, "text/plain", "Lesson not found");
}

void handleQuiz() {
  if (!server.hasArg("module")) return;
  Module* m = contentParser.getModuleById(server.arg("module"));
  if (m) {
      String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:sans-serif;padding:20px;}.q{margin-bottom:20px;background:#f9f9f9;padding:10px;}</style></head><body>";
      html += "<a href='/module?id=" + m->id + "'>&larr; Back</a><h1>" + m->name + " Quiz</h1>";
      html += contentParser.generateQuizHtml(*m);
      html += "</body></html>";
      server.send(200, "text/html", html);
  }
}

void setup() {
  Serial.begin(115200);
  if (contentParser.initialize()) {
    contentParser.loadModules();
    contentParser.printModuleInfo(0);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/module", handleModule);
  server.on("/lesson", handleLesson);
  server.on("/quiz", handleQuiz);
  server.onNotFound([](){
      server.sendHeader("Location", "/");
      server.send(302, "text/plain", "Redirect");
  });

  server.begin();
  Serial.println("Ready!");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}
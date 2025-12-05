#ifndef CONTENT_PARSER_H
#define CONTENT_PARSER_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include "md4c-html.h"

#define MAX_LESSONS 10
#define MAX_MODULES 10
#define MAX_QUIZ_QUESTIONS 50

struct Lesson {
  int id;
  String title;
  String content;
  bool isValid;
  Lesson() : id(0), title(""), content(""), isValid(false) {}
};

struct QuizQuestion {
  String question;
  String options[4];
  int optionCount;
  char correctAnswer;
  QuizQuestion() : question(""), optionCount(0), correctAnswer('a') {
    for(int i=0; i<4; i++) options[i] = "";
  }
};

struct Module {
  String id;
  String name;
  Lesson lessons[MAX_LESSONS];
  int lessonCount;
  QuizQuestion quizQuestions[MAX_QUIZ_QUESTIONS];
  int quizQuestionCount;
  bool hasQuiz;
  bool isValid;
  Module() : id(""), name(""), lessonCount(0), quizQuestionCount(0), hasQuiz(false), isValid(false) {}
};

class ContentParser {
private:
  Module modules[MAX_MODULES];
  int moduleCount;

  static void mdProcessOutput(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    String* output = (String*)userdata;
    for (MD_SIZE i = 0; i < size; i++) *output += text[i];
  }

  String markdownToHtml(const String& markdown) {
    String htmlOutput = "";
    md_html(markdown.c_str(), markdown.length(), mdProcessOutput, &htmlOutput, 0, 0);
    return htmlOutput;
  }

  String toTitleCase(const String& input) {
    String result = "";
    bool capitalizeNext = true;
    for (unsigned int i = 0; i < input.length(); i++) {
      char c = input.charAt(i);
      if (c == '-' || c == '_') { result += ' '; capitalizeNext = true; }
      else if (capitalizeNext) { result += (char)toupper(c); capitalizeNext = false; }
      else { result += (char)tolower(c); }
    }
    return result;
  }

  // Extracts ID from "1.intro.content" (returns 1)
  int extractLessonId(const String& filename) {
    int endIdx = filename.indexOf('.');
    if (endIdx > 0 && isDigit(filename.charAt(0))) {
      return filename.substring(0, endIdx).toInt();
    }
    return 0; 
  }

  String extractTitle(const String& markdown) {
    int startIdx = markdown.indexOf("# ");
    if (startIdx == -1) return "Untitled";
    startIdx += 2;
    int endIdx = markdown.indexOf("\n", startIdx);
    if (endIdx == -1) endIdx = markdown.length();
    return markdown.substring(startIdx, endIdx);
  }

  void parseQuizFile(const String& quizContent, Module& module) {
    int questionCount = 0;
    int pos = 0;
    while (pos < quizContent.length() && questionCount < MAX_QUIZ_QUESTIONS) {
      int qStart = quizContent.indexOf("###", pos);
      if (qStart == -1) break;
      int qEnd = quizContent.indexOf("\n", qStart);
      if (qEnd == -1) break;
      
      int nextLine = qEnd + 1;
      int qTextEnd = quizContent.indexOf("\n\n", nextLine);
      if (qTextEnd == -1) qTextEnd = quizContent.indexOf("\na)", nextLine);
      
      if (qTextEnd > nextLine) {
        String qText = quizContent.substring(nextLine, qTextEnd);
        qText.trim();
        if (qText.length() > 0 && !qText.startsWith("**")) {
          module.quizQuestions[questionCount].question = qText;
          int optCount = 0;
          for (char opt = 'a'; opt <= 'd'; opt++) {
            String pattern = String(opt) + ")";
            int optStart = quizContent.indexOf(pattern, qTextEnd);
            if (optStart == -1 || optStart > qTextEnd + 500) break;
            int optEnd = quizContent.indexOf("\n", optStart);
            if (optEnd == -1) optEnd = quizContent.length();
            String optStr = quizContent.substring(optStart + 2, optEnd);
            optStr.trim();
            module.quizQuestions[questionCount].options[optCount++] = optStr;
          }
          module.quizQuestions[questionCount].optionCount = optCount;
          
          int ansPos = quizContent.indexOf("**Answer:", qTextEnd);
          if (ansPos == -1) ansPos = quizContent.indexOf("**Sagot:", qTextEnd);
          if (ansPos != -1 && ansPos < qTextEnd + 500) {
             int ansStart = quizContent.indexOf(")", ansPos) - 1;
             if (ansStart > ansPos) module.quizQuestions[questionCount].correctAnswer = quizContent.charAt(ansStart);
          }
          questionCount++;
        }
      }
      pos = qEnd + 1;
    }
    module.quizQuestionCount = questionCount;
    module.hasQuiz = (questionCount > 0);
  }

public:
  ContentParser() : moduleCount(0) {}

  bool initialize() {
    if (!SPIFFS.begin(true)) return false;
    Serial.println("SPIFFS mounted.");
    return true;
  }

  // ⭐️ LOGIC CHANGE: Group by Filename Prefix
  void loadModules() {
    File root = SPIFFS.open("/");
    if (!root) return;

    moduleCount = 0;
    Serial.println("Scanning files using Prefix Grouping...");

    while (true) {
      File file = root.openNextFile();
      if (!file) break;

      String fullName = String(file.name());
      if (fullName.startsWith("/")) fullName = fullName.substring(1); // Remove leading slash

      // Skip system files
      if (fullName.startsWith(".") || file.isDirectory()) {
        file.close();
        continue;
      }

      String moduleID = "general";     // Default bucket
      String realFileName = fullName;  // Default filename

      // Check for Prefix (e.g., "math_1.intro.content")
      int underscoreIndex = fullName.indexOf('_');
      if (underscoreIndex > 0) {
        moduleID = fullName.substring(0, underscoreIndex); // "math"
        realFileName = fullName.substring(underscoreIndex + 1); // "1.intro.content"
      }

      // Find or Create Module
      int modIdx = -1;
      for (int i = 0; i < moduleCount; i++) {
        if (modules[i].id == moduleID) {
          modIdx = i;
          break;
        }
      }

      // Create new module if not found
      if (modIdx == -1) {
        if (moduleCount < MAX_MODULES) {
          modIdx = moduleCount++;
          modules[modIdx].id = moduleID;
          modules[modIdx].name = toTitleCase(moduleID);
          modules[modIdx].isValid = true;
          Serial.println("Created Module: " + modules[modIdx].name);
        } else {
          Serial.println("Max modules reached. Skipping: " + fullName);
          file.close();
          continue;
        }
      }

      // --- Parse File into the Module ---
      // Read content
      String content = "";
      while(file.available()) content += (char)file.read();
      
      Module& targetMod = modules[modIdx];

      if (realFileName.endsWith(".content") || realFileName.endsWith(".md")) {
         if (targetMod.lessonCount < MAX_LESSONS) {
            Lesson& l = targetMod.lessons[targetMod.lessonCount];
            l.id = extractLessonId(realFileName); // logic works on "1.intro.content"
            l.title = extractTitle(content);
            l.content = markdownToHtml(content);
            l.isValid = true;
            targetMod.lessonCount++;
            Serial.println("  Added Lesson to " + moduleID + ": " + l.title);
         }
      } 
      else if (realFileName.endsWith(".quiz") || realFileName.endsWith(".txt")) {
         parseQuizFile(content, targetMod);
         Serial.println("  Added Quiz to " + moduleID);
      }

      file.close();
    }
    root.close();
    Serial.print("Total modules loaded: "); Serial.println(moduleCount);
  }

  // Helpers
  int getModuleCount() const { return moduleCount; }
  Module* getModule(int i) { return (i >= 0 && i < moduleCount) ? &modules[i] : nullptr; }
  Module* getModuleById(const String& id) {
    for (int i=0; i<moduleCount; i++) if (modules[i].id == id) return &modules[i];
    return nullptr;
  }
  
  String generateQuizHtml(const Module& module) {
    if (!module.hasQuiz) return "";
    String html = "<form id='quizForm'>";
    for(int i=0; i<module.quizQuestionCount; i++) {
        html += "<div class='q'><p>" + String(i+1) + ". " + module.quizQuestions[i].question + "</p>";
        for(int j=0; j<module.quizQuestions[i].optionCount; j++) {
            html += "<label><input type='radio' name='q" + String(i) + "' value='" + String((char)('a'+j)) + "'> " + module.quizQuestions[i].options[j] + "</label><br>";
        }
        html += "</div>";
    }
    html += "<br><button type='button' onclick='gradeQuiz()'>Submit</button></form><div id='result'></div>";
    html += "<script>function gradeQuiz(){var s=0;var t=" + String(module.quizQuestionCount) + ";var f=document.forms['quizForm'];";
    for(int i=0; i<module.quizQuestionCount; i++) {
        html += "if(f.elements['q" + String(i) + "'].value=='" + String(module.quizQuestions[i].correctAnswer) + "')s++;";
    }
    html += "document.getElementById('result').innerHTML='Score: '+s+'/'+t;}</script>";
    return html;
  }

  void printModuleInfo(int i) {
    if (i < 0 || i >= moduleCount) return;
    Serial.println("Module: " + modules[i].name + " (Lessons: " + String(modules[i].lessonCount) + ")");
  }
};

#endif
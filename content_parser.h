#ifndef CONTENT_PARSER_H
#define CONTENT_PARSER_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include "md4c-html.h"

// Maximum limits for data structures
#define MAX_LESSONS 10
#define MAX_MODULES 10
#define MAX_QUIZ_QUESTIONS 50

// Structure for a Lesson
struct Lesson {
  int id;                    // Lesson number (extracted from filename)
  String title;              // Lesson title (extracted from markdown)
  String content;            // HTML-converted content from markdown
  bool isValid;              // Flag to check if lesson is properly loaded
  
  Lesson() : id(0), title(""), content(""), isValid(false) {}
};

// Structure for Quiz Question
struct QuizQuestion {
  String question;           // The question text
  String options[4];         // Up to 4 options (a, b, c, d)
  int optionCount;           // Number of options
  char correctAnswer;        // Correct answer ('a', 'b', 'c', 'd')
  
  QuizQuestion() : question(""), optionCount(0), correctAnswer('a') {
    for(int i = 0; i < 4; i++) options[i] = "";
  }
};

// Structure for a Module
struct Module {
  String id;                 // Directory name (e.g., "basic-mathematics")
  String name;               // Title case name (e.g., "Basic Mathematics")
  Lesson lessons[MAX_LESSONS];
  int lessonCount;
  QuizQuestion quizQuestions[MAX_QUIZ_QUESTIONS];
  int quizQuestionCount;
  bool hasQuiz;
  bool isValid;
  
  Module() : id(""), name(""), lessonCount(0), quizQuestionCount(0), 
             hasQuiz(false), isValid(false) {}
};

// Content Parser Class
class ContentParser {
private:
  Module modules[MAX_MODULES];
  int moduleCount;
  
  // Helper function to convert markdown to HTML
  static void mdProcessOutput(const MD_CHAR* text, MD_SIZE size, void* userdata) {
    String* output = (String*)userdata;
    for (MD_SIZE i = 0; i < size; i++) {
      *output += text[i];
    }
  }
  
  // Convert markdown string to HTML
  String markdownToHtml(const String& markdown) {
    String htmlOutput = "";
    int result = md_html(
      markdown.c_str(),
      markdown.length(),
      mdProcessOutput,
      &htmlOutput,
      0,  // parser_flags
      0   // renderer_flags
    );
    
    if (result != 0) {
      Serial.println("Error converting markdown to HTML");
      return "";
    }
    return htmlOutput;
  }
  
  // Convert directory name to Title Case
  // e.g., "basic-mathematics" -> "Basic Mathematics"
  String toTitleCase(const String& input) {
    String result = "";
    bool capitalizeNext = true;
    
    for (unsigned int i = 0; i < input.length(); i++) {
      char c = input.charAt(i);
      
      if (c == '-' || c == '_') {
        result += ' ';
        capitalizeNext = true;
      } else if (capitalizeNext) {
        result += (char)toupper(c);
        capitalizeNext = false;
      } else {
        result += (char)tolower(c);
      }
    }
    return result;
  }
  
  // Extract lesson ID from filename (e.g., "lesson1.content" -> 1)
  int extractLessonId(const String& filename) {
    int startIdx = filename.indexOf("lesson") + 6;
    int endIdx = filename.indexOf(".");
    if (startIdx > 6 && endIdx > startIdx) {
      return filename.substring(startIdx, endIdx).toInt();
    }
    return 0;
  }
  
  // Extract title from markdown content (looks for first # heading)
  String extractTitle(const String& markdown) {
    int startIdx = markdown.indexOf("# ");
    if (startIdx == -1) return "Untitled";
    
    startIdx += 2; // Skip "# "
    int endIdx = markdown.indexOf("\n", startIdx);
    if (endIdx == -1) endIdx = markdown.length();
    
    return markdown.substring(startIdx, endIdx);
  }
  
  // Parse quiz file and extract questions
  void parseQuizFile(const String& quizContent, Module& module) {
    // Simple parser for the quiz format
    // Looks for questions with multiple choice options
    int questionCount = 0;
    int pos = 0;
    
    while (pos < quizContent.length() && questionCount < MAX_QUIZ_QUESTIONS) {
      // Find question pattern (looks for numbered questions or "###")
      int questionStart = quizContent.indexOf("###", pos);
      if (questionStart == -1) break;
      
      // Extract question text (between ### and next line)
      int questionEnd = quizContent.indexOf("\n", questionStart);
      if (questionEnd == -1) break;
      
      String questionLine = quizContent.substring(questionStart + 3, questionEnd);
      questionLine.trim();
      
      // Skip if this is "Question X" header, look for actual question
      int nextLine = questionEnd + 1;
      int questionTextEnd = quizContent.indexOf("\n\n", nextLine);
      if (questionTextEnd == -1) questionTextEnd = quizContent.indexOf("\na)", nextLine);
      
      if (questionTextEnd > nextLine) {
        String questionText = quizContent.substring(nextLine, questionTextEnd);
        questionText.trim();
        
        if (questionText.length() > 0 && !questionText.startsWith("**")) {
          module.quizQuestions[questionCount].question = questionText;
          
          // Find options (a), b), c), d))
          int optionCount = 0;
          for (char opt = 'a'; opt <= 'd'; opt++) {
            String optPattern = String(opt) + ")";
            int optStart = quizContent.indexOf(optPattern, questionTextEnd);
            if (optStart == -1 || optStart > questionTextEnd + 500) break;
            
            int optEnd = quizContent.indexOf("\n", optStart);
            if (optEnd == -1) optEnd = quizContent.length();
            
            String optText = quizContent.substring(optStart + 2, optEnd);
            optText.trim();
            module.quizQuestions[questionCount].options[optionCount] = optText;
            optionCount++;
          }
          
          module.quizQuestions[questionCount].optionCount = optionCount;
          
          // Find correct answer (looks for **Answer: X** or **Sagot: X**)
          int answerPos = quizContent.indexOf("**Answer:", questionTextEnd);
          if (answerPos == -1) answerPos = quizContent.indexOf("**Sagot:", questionTextEnd);
          
          if (answerPos != -1 && answerPos < questionTextEnd + 500) {
            int answerStart = quizContent.indexOf(")", answerPos) - 1;
            if (answerStart > answerPos) {
              module.quizQuestions[questionCount].correctAnswer = 
                quizContent.charAt(answerStart);
            }
          }
          
          questionCount++;
        }
      }
      
      pos = questionEnd + 1;
    }
    
    module.quizQuestionCount = questionCount;
    module.hasQuiz = (questionCount > 0);
  }

public:
  ContentParser() : moduleCount(0) {}
  
  // Initialize and scan the storage directory
  bool initialize() {
    if (!SPIFFS.begin(true)) {
      Serial.println("Failed to mount SPIFFS");
      return false;
    }
    
    Serial.println("SPIFFS mounted successfully");
    
    // List all files for debugging
    Serial.println("Listing all files in SPIFFS:");
    File root = SPIFFS.open("/");
    listDir(root, 0);
    
    return true;
  }
  
  // Helper function to list directory contents (for debugging)
  void listDir(File dir, int level) {
    while (true) {
      File entry = dir.openNextFile();
      if (!entry) break;
      
      for (int i = 0; i < level; i++) Serial.print("  ");
      Serial.print(entry.name());
      if (entry.isDirectory()) {
        Serial.println("/");
        listDir(entry, level + 1);
      } else {
        Serial.print(" - ");
        Serial.print(entry.size());
        Serial.println(" bytes");
      }
      entry.close();
    }
  }
  
  // Load all modules from /storage directory
  void loadModules() {
    Serial.println("Starting to load modules...");
    
    // Check if /storage exists
    if (!SPIFFS.exists("/storage")) {
      Serial.println("ERROR: /storage directory does not exist!");
      Serial.println("Trying to scan root for module directories...");
      scanRootForModules();
      return;
    }
    
    File root = SPIFFS.open("/storage");
    if (!root) {
      Serial.println("Failed to open /storage directory");
      return;
    }
    
    if (!root.isDirectory()) {
      Serial.println("/storage is not a directory!");
      return;
    }
    
    Serial.println("/storage opened successfully");
    moduleCount = 0;
    
    // Scan for subdirectories in /storage
    while (moduleCount < MAX_MODULES) {
      File moduleDir = root.openNextFile();
      if (!moduleDir) {
        Serial.println("No more files in /storage");
        break;
      }
      
      String fullPath = String(moduleDir.name());
      Serial.print("Found: ");
      Serial.print(fullPath);
      Serial.print(" - isDir: ");
      Serial.println(moduleDir.isDirectory() ? "YES" : "NO");
      
      if (moduleDir.isDirectory()) {
        String dirName = fullPath;
        // Remove "/storage/" prefix if present
        if (dirName.startsWith("/storage/")) {
          dirName = dirName.substring(9);
        }
        
        Serial.print("Loading module: ");
        Serial.println(dirName);
        
        Module& currentModule = modules[moduleCount];
        currentModule.id = dirName;
        currentModule.name = toTitleCase(dirName);
        currentModule.isValid = true;
        
        // Load lessons and quiz from this module
        loadModuleContent(dirName, currentModule);
        
        if (currentModule.lessonCount > 0 || currentModule.hasQuiz) {
          Serial.print("Module loaded successfully with ");
          Serial.print(currentModule.lessonCount);
          Serial.println(" lessons");
          moduleCount++;
        } else {
          Serial.println("Module had no content, skipping");
        }
      }
      moduleDir.close();
    }
    
    Serial.print("Total modules loaded: ");
    Serial.println(moduleCount);
  }
  
  // Alternative: Scan root directory for module folders
  void scanRootForModules() {
    Serial.println("Scanning root directory for modules...");
    File root = SPIFFS.open("/");
    
    moduleCount = 0;
    while (moduleCount < MAX_MODULES) {
      File entry = root.openNextFile();
      if (!entry) break;
      
      String name = String(entry.name());
      Serial.print("Checking: ");
      Serial.println(name);
      
      // Skip if it starts with /storage (we already checked that)
      if (name.startsWith("/storage")) {
        entry.close();
        continue;
      }
      
      // Look for directories that might be modules
      if (entry.isDirectory()) {
        String dirName = name;
        if (dirName.startsWith("/")) dirName = dirName.substring(1);
        
        // Check if this directory has .content or .quiz files
        Serial.print("Potential module: ");
        Serial.println(dirName);
        
        Module& currentModule = modules[moduleCount];
        currentModule.id = dirName;
        currentModule.name = toTitleCase(dirName);
        currentModule.isValid = true;
        
        loadModuleContent(dirName, currentModule);
        
        if (currentModule.lessonCount > 0 || currentModule.hasQuiz) {
          Serial.print("Found module with content: ");
          Serial.println(dirName);
          moduleCount++;
        }
      }
      entry.close();
    }
  }
  
  // Load content for a specific module
  void loadModuleContent(const String& moduleId, Module& module) {
    // Try multiple path variations
    String paths[] = {
      "/storage/" + moduleId,
      "/" + moduleId,
      moduleId
    };
    
    File moduleDir;
    String workingPath = "";
    
    for (int i = 0; i < 3; i++) {
      Serial.print("Trying path: ");
      Serial.println(paths[i]);
      
      if (SPIFFS.exists(paths[i])) {
        moduleDir = SPIFFS.open(paths[i]);
        if (moduleDir && moduleDir.isDirectory()) {
          workingPath = paths[i];
          Serial.print("Successfully opened: ");
          Serial.println(workingPath);
          break;
        }
        if (moduleDir) moduleDir.close();
      }
    }
    
    if (workingPath.length() == 0) {
      Serial.println("Module directory not found for: " + moduleId);
      return;
    }
    
    module.lessonCount = 0;
    
    while (true) {
      File file = moduleDir.openNextFile();
      if (!file) {
        Serial.println("  No more files in module directory");
        break;
      }
      
      String fullPath = String(file.name());
      String fileName = fullPath;
      
      // Get just the filename without path
      int lastSlash = fileName.lastIndexOf('/');
      if (lastSlash != -1) {
        fileName = fileName.substring(lastSlash + 1);
      }
      
      Serial.print("  Found file: ");
      Serial.print(fileName);
      Serial.print(" (full path: ");
      Serial.print(fullPath);
      Serial.print(", size: ");
      Serial.print(file.size());
      Serial.println(" bytes)");
      
      if (fileName.endsWith(".content")) {
        // This is a lesson file
        if (module.lessonCount < MAX_LESSONS) {
          Lesson& lesson = module.lessons[module.lessonCount];
          
          // Read file content
          String content = "";
          size_t fileSize = file.size();
          content.reserve(fileSize + 1);
          
          while (file.available()) {
            content += (char)file.read();
          }
          
          Serial.print("    Read ");
          Serial.print(content.length());
          Serial.println(" characters");
          
          lesson.id = extractLessonId(fileName);
          lesson.title = extractTitle(content);
          lesson.content = markdownToHtml(content);
          lesson.isValid = true;
          
          Serial.print("    Loaded lesson ");
          Serial.print(lesson.id);
          Serial.print(": ");
          Serial.println(lesson.title);
          
          module.lessonCount++;
        }
      } else if (fileName.endsWith(".quiz")) {
        // This is a quiz file
        String quizContent = "";
        size_t fileSize = file.size();
        quizContent.reserve(fileSize + 1);
        
        while (file.available()) {
          quizContent += (char)file.read();
        }
        
        Serial.print("    Read ");
        Serial.print(quizContent.length());
        Serial.println(" characters from quiz");
        
        parseQuizFile(quizContent, module);
        
        Serial.print("    Loaded quiz with ");
        Serial.print(module.quizQuestionCount);
        Serial.println(" questions");
      }
      
      file.close();
    }
    
    moduleDir.close();
  }
  
  // Getters
  int getModuleCount() const { return moduleCount; }
  
  Module* getModule(int index) {
    if (index >= 0 && index < moduleCount) {
      return &modules[index];
    }
    return nullptr;
  }
  
  Module* getModuleById(const String& id) {
    for (int i = 0; i < moduleCount; i++) {
      if (modules[i].id == id) {
        return &modules[i];
      }
    }
    return nullptr;
  }
  
  // Generate HTML for quiz
  String generateQuizHtml(const Module& module) {
    if (!module.hasQuiz) return "";
    
    String html = "<form id='quizForm'>\n";
    
    for (int i = 0; i < module.quizQuestionCount; i++) {
      const QuizQuestion& q = module.quizQuestions[i];
      
      html += "<div class='question'>\n";
      html += "<p><strong>" + String(i + 1) + ". " + q.question + "</strong></p>\n";
      
      for (int j = 0; j < q.optionCount; j++) {
        char optionLetter = 'a' + j;
        html += "<label><input type='radio' name='q" + String(i) + 
                "' value='" + String(optionLetter) + "'> " + 
                q.options[j] + "</label><br>\n";
      }
      
      html += "</div>\n";
    }
    
    html += "<button type='button' onclick='gradeQuiz()'>Submit Quiz</button>\n";
    html += "</form>\n";
    html += "<div id='result'></div>\n";
    
    // Add JavaScript for grading
    html += "<script>\nfunction gradeQuiz() {\n";
    html += "  var score = 0;\n";
    html += "  var total = " + String(module.quizQuestionCount) + ";\n";
    html += "  var form = document.forms['quizForm'];\n";
    
    for (int i = 0; i < module.quizQuestionCount; i++) {
      html += "  if(form.elements['q" + String(i) + "'].value == '" + 
              String(module.quizQuestions[i].correctAnswer) + "') score++;\n";
    }
    
    html += "  var resultDiv = document.getElementById('result');\n";
    html += "  resultDiv.innerHTML = 'You scored ' + score + ' out of ' + total;\n";
    html += "  if(score == total) {\n";
    html += "    resultDiv.style.color = 'green';\n";
    html += "    resultDiv.innerHTML += '<br>Excellent work!';\n";
    html += "  } else if(score >= total * 0.7) {\n";
    html += "    resultDiv.style.color = 'orange';\n";
    html += "    resultDiv.innerHTML += '<br>Good job! Keep practicing.';\n";
    html += "  } else {\n";
    html += "    resultDiv.style.color = 'red';\n";
    html += "    resultDiv.innerHTML += '<br>Keep studying!';\n";
    html += "  }\n";
    html += "}\n</script>\n";
    
    return html;
  }
  
  // Print module info for debugging
  void printModuleInfo(int index) {
    if (index < 0 || index >= moduleCount) return;
    
    Module& m = modules[index];
    Serial.println("\n=== Module Info ===");
    Serial.print("ID: "); Serial.println(m.id);
    Serial.print("Name: "); Serial.println(m.name);
    Serial.print("Lessons: "); Serial.println(m.lessonCount);
    Serial.print("Quiz Questions: "); Serial.println(m.quizQuestionCount);
    
    for (int i = 0; i < m.lessonCount; i++) {
      Serial.print("  Lesson ");
      Serial.print(m.lessons[i].id);
      Serial.print(": ");
      Serial.println(m.lessons[i].title);
    }
  }
};

#endif // CONTENT_PARSER_H

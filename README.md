# 🧑‍💻 EduBridge by WON 📚
## 📌 Project Goals & Setup

This whole project is based on **PlatformIO** in VS Code. We use a flat file system (SPIFFS) which is weird, so we have to stick to the naming rules below.

  * **Brain of the System:** `src/main.cpp` (This is where the website gets built).
  * **Our Content:** The **`data/`** folder (This is where all the lessons and quizzes go).

-----

## ✏️ Part 1: Adding New Lessons & Quizzes
We can't use regular folders (like `math/` or `filipino/`) because the ESP32 loses the folder names when uploading. Instead, we use the **filename** to group things.
### 1\. The Naming Formula (THE MOST IMPORTANT RULE\!)
Every file must follow this exact pattern, separated by an underscore (`_`):

`[module_id]_[id].[title].[extension]`

| If you want to add... | The Filename Should Be... | Why it works: |
| :--- | :--- | :--- |
| **Math Lesson 1** | `math_1.intro.content` | The code sees **`math`** and creates a Module. |
| **Filipino Quiz** | `filipino_module.quiz` | The code sees **`filipino`** and adds the quiz to that Module. |

### 2\. Where to Put the Files
  * Put **ALL** lesson and quiz files directly inside the **`data/`** folder. Don't put them in subfolders like `data/math/`.
  * 
### 3\. Writing Lesson Content
  * Use a `.content` or `.md` file.
  * You can use simple Markdown like `# Heading`, `**bold**`, and `*italics*`.

### 4\. Writing Quizzes
  * Use a `.quiz` or `.txt` file.
  * Make sure you use the exact format for the questions and the answer key:

<!-- end list -->

```text
### Question 1
What is the largest island in the Philippines?

a) Cebu
b) Luzon
c) Mindanao
d) Palawan

**Answer: b**
```

-----

## 🎨 Part 2: Changing the Look (CSS)
If you want to change colors or fonts, you have to edit the C++ code directly.
1.  Open **`src/main.cpp`**.
2.  Search for the three main handler functions: `handleRoot()`, `handleModule()`, and `handleQuiz()`.
3.  Inside those functions, find the line that contains `<style>`.

**Example:**

```cpp
// Search for this line in handleRoot:
html += "<style>body{font-family:Arial;padding:20px;background:#f4f4f9;}"; 

// Change the background color to, say, a light blue:
html += "<style>body{font-family:Arial;padding:20px;background:#E6F3FF;}";
```

-----

## 🚀 Part 3: How to Upload Your Changes
The most common reason for bugs is mixing up these two uploads. **You need to run them separately.**

| If you changed... | You must run this task... | VS Code Status Bar Icon |
| :--- | :--- | :--- |
| **Files/Content** (in `data/`) | **Upload Filesystem Image** | (Lightning Bolt → Platform) |
| **Code/CSS** (in `src/main.cpp`) | **Upload** | (Right Arrow →) |

### Upload Steps:
1.  **Clean Cache:** Click the **Trash Can / Broom icon** in the bottom status bar first (good habit\!).
2.  **Upload Data:** Run the **Upload Filesystem Image** task.
3.  **Upload Code:** Run the **Upload** task.
4.  **Check:** Open the Monitor (Plug icon) and look for **`Total modules loaded: X`** (it should be \> 0\!).

### 🛑 If It Fails:

  * **Error: "Resource temporarily unavailable"**: Another program is holding the USB port. **Close your Terminal and VS Code, then restart.**
  * **Error: "Total modules loaded: 0"**: You forgot to run **Upload Filesystem Image**, or you still have some old files in your local `data/` folder that aren't named with the `prefix_` convention.

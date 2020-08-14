#include <clocale>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <random>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include "qa-screens.h"
#include "qa-file.h"

QuestionSet question_set;
TestResult current;
size_t now_id;
std::deque<TestResult> history;
std::vector<std::string> answers;
std::chrono::time_point<std::chrono::steady_clock> start_time;
std::string history_path;
std::mt19937_64 rand_gen;

enum QAScreen {
  kTitle,
  kOpenQuestion,
  kHistory,
  kHowTo,
  kQuestionNum,
  kPrepare,
  kQuestion,
  kFinished,
  kReview,
  kExportTxt,
  kExit
};
const std::string kFileError =
    "Error: Empty question file or question file does not exist.";
const std::string kHistError =
    "Error: Failed to load result. Question file seems to be changed.";
const std::string kNumberError = "Error: Invalid number of questions.";
const std::string kExportError = "Error: Cannot open the file to export.";

inline void SetTitle(ScreenWithTitle* scr) {
  if (question_set.questions.empty()) {
    scr->SetTitle("Welcome to Q&A System!");
  } else {
    scr->SetTitle(question_set.title);
  }
}

QAScreen ShowTitleScreen() {
  if (question_set.questions.empty()) {
    QAScreen results[] = {kOpenQuestion, kHistory, kHowTo, kExit};
    MenuScreen scr({"Open question file", "View history",
                    "How to: Make a question file", "Exit"});
    SetTitle(&scr);
    doupdate();
    while (!scr.ProcessKey(getch())) doupdate();
    return results[scr.GetValue()];
  } else {
    QAScreen results[] = {kQuestionNum, kOpenQuestion, kHistory, kHowTo, kExit};
    MenuScreen scr({"Take the test", "Open another question file",
                    "View history", "How to: Make a question file", "Exit"});
    SetTitle(&scr);
    doupdate();
    while (!scr.ProcessKey(getch())) doupdate();
    return results[scr.GetValue()];
  }
}

QAScreen ShowOpenQuestionScreen() {
  PromptScreen scr("Enter the filename of the question file.\n"
                   "Leave it blank to go back to the main page.");
  SetTitle(&scr);
  doupdate();
  while (true) {
    while (!scr.ProcessKey(getch())) doupdate();
    std::string filename = scr.GetValue();
    if (filename.empty()) return kTitle;
    question_set = ReadCSV(filename);
    if (question_set.questions.size()) {
      current.file = std::filesystem::absolute(filename);
      return kTitle;
    }
    scr.SetMessage(kFileError);
    doupdate();
  }
}

QAScreen ShowHistoryScreen() {
  std::vector<std::string> hist;
  for (auto& i : history) hist.push_back(i.GetMenuText());
  MenuScreen scr(hist,
      "Select an entry to view detailed results or take the test again.\n"
      "Press <ESC> to leave.\n\n" +
      kHistoryHeader);
  SetTitle(&scr);
  doupdate();
  while (true) {
    while (!scr.ProcessKey(getch())) doupdate();
    int val = scr.GetValue();
    if (val == -1) {
      question_set.questions.clear();
      return kTitle;
    }
    auto& i = history[val];
    question_set = ReadCSV(i.file);
    if (question_set.questions.empty()) {
      scr.SetMessage(kFileError);
      doupdate();
      continue;
    }
    bool flag = false;
    for (auto& j : i.ord) {
      if (j >= question_set.questions.size()) {
        flag = true;
        break;
      }
    }
    for (auto& j : i.unsure) {
      if (j >= question_set.questions.size()) {
        flag = true;
        break;
      }
    }
    for (auto& j : i.wa) {
      if (j.id >= question_set.questions.size()) {
        flag = true;
        break;
      }
    }
    if (flag) {
      scr.SetMessage(kHistError);
      doupdate();
      question_set.questions.clear();
      continue;
    }
    current = i;
    return kFinished;
  }
}

QAScreen ShowHowToScreen() {
  ViewScreen scr(
      "HOW TO: Make a question file\n\n"
      "This application supports question files in csv (Comma-Separated Values) format.\n"
      "You can simply use Microsoft Excel to make one. The following are the rules:\n\n"
      "1. Each question contains three parts: question description, answer and hint.\n"
      "2. The answer should NOT be a blank, an \"*\" or an \"=\", or the question won\'t\n"
      "   able to be answered correctly.\n"
      "3. The A1 cell should contain the title of the test.\n"
      "   If the A2 cell is '1', the answers will be case-insensitive.\n"
      "   When processing the user's input and the answer, all characters in the A3 cell\n"
      "   will be ignored.\n"
      "4. In the following rows, each row represents a question. The first column is\n"
      "   the question description, the second is the answer, and the third is the\n"
      "   hint.\n"
      "5. While saving the file, remember to choose the csv (comma separated) format.\n");
  SetTitle(&scr);
  doupdate();
  while (!scr.ProcessKey(getch())) doupdate();
  return kTitle;
}

QAScreen ShowQuestionNumScreen() {
  int num = 1;
  if (question_set.questions.size() > 1) {
    PromptScreen scr("Input the number of questions you want to practice (1~" +
                     std::to_string(question_set.questions.size()) + "):");
    SetTitle(&scr);
    doupdate();
    while (true) {
      while (!scr.ProcessKey(getch())) doupdate();
      try {
        num = std::stoi(scr.GetValue());
        if (num >= 1 && num <= (int)question_set.questions.size()) break;
      } catch (...) {}
      scr.SetMessage(kNumberError);
      doupdate();
    }
  }
  // file: set, others: not yet
  auto& ord = current.ord;
  ord.clear();
  for (size_t i = 0; i < question_set.questions.size(); i++) ord.push_back(i);
  std::shuffle(ord.begin(), ord.end(), rand_gen);
  ord.resize(num);
  return kPrepare;
}

QAScreen ShowPrepareScreen() {
  clear();
  ScreenWithTitle scr;
  SetTitle(&scr);
  mvaddstr(2, 1, "If you're ready for the test, press any key to continue...");
  refresh();
  getch();

  for (const char* i : {"THREE", "TWO", "ONE"}) {
    clear();
    SetTitle(&scr);
    mvaddstr(2, 1, i);
    refresh();
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(1s);
  }
  now_id = 0;
  answers.clear();
  current.unsure.clear();
  current.wa.clear();
  start_time = std::chrono::steady_clock::now();
  return kQuestion;
}

QAScreen ShowQuestionScreen() {
  size_t id = current.ord[now_id];
  QuestionScreen scr(question_set.questions[id].description,
                     (double)now_id / current.ord.size());
  SetTitle(&scr);
  doupdate();
  while (!scr.ProcessKey(getch())) doupdate();
  auto val = scr.GetValue();
  switch (val.first) {
    case QuestionScreen::kGiveUp:
      // answer can't be blank, so blank means giving up
      answers.emplace_back();
      break;
    case QuestionScreen::kUnsure:
      current.unsure.insert(id);
      [[fallthrough]];
    case QuestionScreen::kAnswer:
      answers.push_back(std::move(val.second));
      break;
    case QuestionScreen::kExit: return kTitle;
  }
  if (++now_id == current.ord.size()) {
    // scoring the whole test
    std::chrono::duration<double> elapsed =
        std::chrono::steady_clock::now() - start_time;
    current.elapsed = elapsed.count();
    current.finish = time(nullptr);
    current.score = 0;
    current.fullmark = 0;
    for (size_t i = 0; i < current.ord.size(); i++) {
      size_t id = current.ord[i];
      int score = Score(question_set.questions[id], answers[i],
                        question_set.ignore_chars);
      if (score < 1) current.wa.push_back({id, answers[i]});
      current.score += score;
      current.fullmark += 1;
    }
    answers.clear();
    history.push_front(current);
    ExportHistory(history_path, history);
    return kFinished;
  }
  return kQuestion;
}

QAScreen ShowFinishedScreen() {
  std::vector<QAScreen> results = {kQuestionNum, kPrepare, kReview, kExportTxt,
                                   kTitle, kExit};
  std::vector<std::string> choices = {
      "Take the test again with different number of questions",
      "Take the test on the questions you're unsure or answered wrong",
      "Review the questions you're unsure or answered wrong",
      "Export the result as a text file",
      "Go back to the main page",
      "Exit"};
  if (current.unsure.empty() && current.wa.empty()) {
    results.erase(results.begin() + 1);
    choices.erase(choices.begin() + 1);
  }
  MenuScreen scr(choices, current.GetSummary(false));
  SetTitle(&scr);
  doupdate();
  while (!scr.ProcessKey(getch())) doupdate();
  QAScreen ret = results[scr.GetValue()];
  if (ret == kPrepare) { // take the test on those unsure or wrong
    auto& ord = current.ord;
    ord.clear();
    for (auto& i : current.unsure) ord.push_back(i);
    for (auto& i : current.wa) ord.push_back(i.id);
    std::sort(ord.begin(), ord.end());
    ord.resize(std::unique(ord.begin(), ord.end()) - ord.begin());
    std::shuffle(ord.begin(), ord.end(), rand_gen);
  }
  return ret;
}

QAScreen ShowReviewScreen() {
  ViewScreen scr(current.GetReview(question_set, false));
  SetTitle(&scr);
  doupdate();
  while (!scr.ProcessKey(getch())) doupdate();
  return kFinished;
}

QAScreen ShowExportTxtScreen() {
  PromptScreen scr("Enter the path of the file to export.\n"
                   "Leave it blank to go back to the previous page.");
  SetTitle(&scr);
  doupdate();
  while (true) {
    while (!scr.ProcessKey(getch())) doupdate();
    std::string filename = scr.GetValue();
    if (filename.empty()) return kFinished;
    std::ofstream fout(filename);
    if (!fout.is_open()) {
      scr.SetMessage(kExportError);
      doupdate();
      continue;
    }
    fout << current.GetReview(question_set, true);
    return kFinished;
  }
}

void MainLoop() {
  QAScreen scr = kTitle;
  while (true) {
    switch (scr) {
      case kTitle: scr = ShowTitleScreen(); break;
      case kOpenQuestion: scr = ShowOpenQuestionScreen(); break;
      case kHistory: scr = ShowHistoryScreen(); break;
      case kHowTo: scr = ShowHowToScreen(); break;
      case kQuestionNum: scr = ShowQuestionNumScreen(); break;
      case kPrepare: scr = ShowPrepareScreen(); break;
      case kQuestion: scr = ShowQuestionScreen(); break;
      case kFinished: scr = ShowFinishedScreen(); break;
      case kReview: scr = ShowReviewScreen(); break;
      case kExportTxt: scr = ShowExportTxtScreen(); break;
      case kExit: return;
    }
  }
}

const int kTitleColorPair = 1;

int main() {
  rand_gen.seed(std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count());
  char* home = getenv("HOME");
  history_path = home ? (std::string)home + "/.qa_system.hist" : ".qa_system.hist";
  try {
    history = ReadHistory(history_path);
  } catch (...) {
    std::cerr << "Failed reading history file " << history_path
        << ".\nFix the history file or delete it." << std::endl;
    return 1;
  }

  std::setlocale(LC_ALL, "");
  std::setlocale(LC_CTYPE, "");
  initscr();
  keypad(stdscr, true);
  noecho();
  start_color();
  wnoutrefresh(stdscr);
  init_pair(kTitleColorPair, COLOR_BLACK, COLOR_GREEN);

  MainLoop();

  endwin();
}

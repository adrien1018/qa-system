#ifndef QA_SCREENS_H_
#define QA_SCREENS_H_

#include <optional>
#include "ncurses-widget.h"

// These classes handle screen resizing and termination.
// ProcessKey returns true if the screen is considered terminated.

// TODO: Add message box

extern const int kTitleColorPair;

class MessageBox { // Message boxes that appear at the center of the screen
  std::vector<Button> button_pos_;
  WINDOW* win_;
  ButtonGroup buttons_;
  std::vector<std::string> title_;
 public:
  // Each string is one line
  MessageBox(const std::vector<std::string>& title);
  ~MessageBox();
  MessageBox(const MessageBox&) = delete;
  MessageBox& operator=(const MessageBox&) = delete;
  void Refresh();
  int GetValue() const;
  bool ProcessKey(int);
};

class ScreenWithTitle {
  std::string title_;
 protected:
  void RefreshTitle_();
 public:
  virtual void SetCursor() {}
  void SetTitle(const std::string& str);
};

class MenuScreen : public ScreenWithTitle {
  Menu menu_;
  std::string header_;
  std::string message_;
  bool leave_;
  void Resize_();
 public:
  MenuScreen(const std::vector<std::string>&, const std::string& header = "");
  void SetMessage(const std::string&);
  int GetValue() const;
  bool ProcessKey(int);
};

// TODO: Add input buffer for UTF-8
class PromptScreen : public ScreenWithTitle {
  Textbox text_;
  std::string header_;
  std::string message_;
  void Resize_();
 public:
  PromptScreen(const std::string& header = "");
  void SetMessage(const std::string&);
  void SetCursor();
  std::string GetValue() const;
  bool ProcessKey(int);
};

class QuestionScreen : public ScreenWithTitle { // Answer a question
  Textbox question_, answer_;
  CheckBox unsure_, giveup_;
  std::optional<MessageBox> msg_;
  std::string errmsg_;
  double progress_;
  int state_;
  void RefreshErrMsg_();
  void Resize_();
 public:
  enum Result { kAnswer, kUnsure, kGiveUp, kExit };
  QuestionScreen(const std::string& question, double progress);
  void SetCursor();
  std::pair<Result, std::string> GetValue() const;
  bool ProcessKey(int);
};

class ViewScreen : public ScreenWithTitle { // View a textbox
  Textbox text_;
  std::string header_;
  void Resize_();
 public:
  ViewScreen(const std::string& content, const std::string& header = "");
  bool ProcessKey(int);
};

#endif // QA_SCREENS_H_

#ifndef QA_SCREENS_H_
#define QA_SCREENS_H_

#include <optional>
#include <functional>
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
  struct DefaultCallback {
    void operator()(std::string& /*header*/, std::vector<std::string>& /*choices*/,
                    int /*height*/, int /*width*/) {}
  };
  Menu menu_;
  std::string header_;
  std::string message_;
  bool leave_;
  template <class Func = DefaultCallback>
  void Resize_(Func&& callback = DefaultCallback()) {
    clear();
    mvaddstr(LINES - 2, 1, message_.c_str());
    RefreshTitle_();
    menu_.ResizeWindow(LINES - 4, COLS - 2);
    WINDOW* win = menu_.GetWin();
    wclear(win);
    mvwaddstr(win, 0, 0, header_.c_str()); // just for getting height
    int y, __attribute__((unused)) x;
    getyx(win, y, x);
    using namespace std::placeholders;
    menu_.ResizeWindowCallback(std::bind(callback, std::ref(header_), _1, _2, _3),
                               LINES - 4, COLS - 2, y + 1);
    win = menu_.GetWin();
    mvwaddstr(win, 0, 0, header_.c_str());
    wnoutrefresh(win);
  }
 public:
  MenuScreen(const std::vector<std::string>&, const std::string& header = "");
  void SetMessage(const std::string&);
  int GetValue() const;
  template <class Func = DefaultCallback>
  bool ProcessKey(int ch, Func&& callback = DefaultCallback()) {
    leave_ = false;
    if (ch == '\n') return true;
    if (ch == 27) {
      leave_ = true;
      return true;
    }
    if (ch == KEY_RESIZE) {
      Resize_(callback);
    } else {
      ch = menu_.ProcessKey(ch);
    }
    return false;
  }
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

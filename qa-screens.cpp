#include "qa-screens.h"

#include "ncurses-utils.h"

MessageBox::MessageBox(const std::vector<std::string>& title)
    : button_pos_{{"Cancel", 0, 0, 1, -1, -1, -1}, {"OK", 0, 0, -1, 0, -1, -1}},
      win_(nullptr),
      buttons_(button_pos_),
      title_(title) {
  curs_set(0);
  Refresh();
}

MessageBox::~MessageBox() {
  if (win_) delwin(win_);
}

void MessageBox::Refresh() {
  // "[OK]   [Cancel]" -> 15 wide
  int width = 15 + 2; // leave a border
  int height = std::min(LINES, (int)title_.size() + 3); // button + border = 3
  for (auto& i : title_) width = std::max(width, (int)StringWidth(i));
  width = std::min(width + 2, COLS); // border = 2
  if (win_) delwin(win_);
  win_ = newwin(height, width, CenterStart(0, LINES, height),
                CenterStart(0, COLS, width));
  for (size_t i = 0; i < title_.size(); i++) {
    PrintCenter(win_, title_[i], i + 1, 1, width - 1);
  }
  button_pos_[0].y = height - 2;
  button_pos_[0].x = CenterStart(1, width - 1, 15) + 7;
  button_pos_[1].y = height - 2;
  button_pos_[1].x = CenterStart(1, width - 1, 15);
  buttons_.SetWindow(win_);
  buttons_.ResetButtons(button_pos_);
  box(win_, 0, 0);
  wnoutrefresh(win_);
}

int MessageBox::GetValue() const {
  return buttons_.GetValue();
}

bool MessageBox::ProcessKey(int ch) {
  if (ch == '\n') return true;
  ch = buttons_.ProcessKey(ch);
  if (ch == KEY_RESIZE) Refresh();
  return false;
}

void ScreenWithTitle::SetTitle(const std::string& str) {
  title_ = str;
  RefreshTitle_();
}

void ScreenWithTitle::RefreshTitle_() {
  attron(COLOR_PAIR(kTitleColorPair));
  mvprintw(0, 0, " %-*s", COLS - 1, title_.c_str());
  attroff(COLOR_PAIR(kTitleColorPair));
  wnoutrefresh(stdscr);
  SetCursor();
}

void MenuScreen::Resize_() {
  clear();
  mvaddstr(LINES - 2, 1, message_.c_str());
  RefreshTitle_();
  menu_.ResizeWindow(LINES - 4, COLS - 2);
  WINDOW* win = menu_.GetWin();
  wclear(win);
  mvwaddstr(win, 0, 0, header_.c_str()); // just for getting height
  int y, x; // x not used
  getyx(win, y, x);
  menu_.ResizeWindow(LINES - 4, COLS - 2, y + 1);
  win = menu_.GetWin();
  mvwaddstr(win, 0, 0, header_.c_str());
  wnoutrefresh(win);
}

MenuScreen::MenuScreen(const std::vector<std::string>& choices,
                       const std::string& header)
    : menu_(choices, 2, 1, LINES - 4, COLS - 2),
      header_(header),
      leave_(false) {
  curs_set(0);
  Resize_();
}

void MenuScreen::SetMessage(const std::string& message) {
  message_ = message;
  Resize_();
}

int MenuScreen::GetValue() const {
  if (leave_) return -1;
  return menu_.GetValue();
}

bool MenuScreen::ProcessKey(int ch) {
  leave_ = false;
  if (ch == '\n') return true;
  if (ch == 27) {
    leave_ = true;
    return true;
  }
  if (ch == KEY_RESIZE) {
    Resize_();
  } else {
    ch = menu_.ProcessKey(ch);
  }
  return false;
}

void PromptScreen::Resize_() {
  clear();
  RefreshTitle_();
  WINDOW* win = derwin(stdscr, LINES - 3, COLS - 2, 2, 1);
  mvwaddstr(win, 0, 0, header_.c_str());
  int y, x;
  getyx(win, y, x);
  y += 2;
  delwin(win);
  mvaddstr(y + 4, 1, message_.c_str());
  win = derwin(stdscr, 3, COLS, y + 1, 0);
  box(win, 0, 0);
  delwin(win);
  wnoutrefresh(stdscr);
  text_.MoveWindow(y + 2, 1);
  text_.ResizeWindow(1, COLS - 2);
}

PromptScreen::PromptScreen(const std::string& header)
    : text_(4, 1, 1, COLS - 2, true, false, 1, 2048), header_(header) {
  curs_set(1);
  Resize_();
}

void PromptScreen::SetMessage(const std::string& message) {
  message_ = message;
  int y, x;
  getbegyx(text_.GetWin(), y, x);
  WINDOW* win = derwin(stdscr, 1, COLS - 2, y + 2, 1);
  wclear(win);
  waddstr(win, message_.c_str());
  delwin(win);
  wnoutrefresh(stdscr);
  text_.Refresh();
}

void PromptScreen::SetCursor() {
  text_.Refresh();
}

std::string PromptScreen::GetValue() const {
  return text_.GetValue();
}

bool PromptScreen::ProcessKey(int ch) {
  if (ch == '\n') return true;
  if (ch == KEY_RESIZE) {
    Resize_();
  } else {
    text_.ProcessKey(ch);
  }
  return false;
}

void QuestionScreen::RefreshErrMsg_() {
  int A = std::max(5, LINES - 16);
  WINDOW* win = derwin(stdscr, 1, COLS - 2, A + 5, 1);
  wclear(win);
  waddstr(win, errmsg_.c_str());
  delwin(win);
  wnoutrefresh(stdscr);
  question_.Refresh();
  answer_.Refresh();
}

void QuestionScreen::Resize_() {
  // +-----------------------------+
  // | Title                       | 0
  // +-----------------------------+ 1
  // | Question:                   | 2
  // |+---------(Textbox)---------+| 3
  // ||(Question)                 ||
  // |+---------------------------+| A=5~LINES-16
  // | Input your answer:          | A+1
  // |+---------(Textbox)---------+| A+2
  // ||                           || A+3
  // |+---------------------------+| A+4
  // | (Messages)                  | A+5
  // | [ ] Unsure (F5 to toggle)   | A+6
  // | [ ] Give up (F6 to toggle)  | A+7
  // | (Instructions)              | A+8
  // |                             |
  // | Progress:                   | A+10
  // | =========== (percentage)%   | A+11
  // | |         |         |       | A+12
  // | 0         50       100      | A+13
  // |                             |
  // |                             | LINES-1
  // +-----------------------------+
  //   ^         ^         ^
  //   1     (1+B)/2  B=(COLS-10)/2*2+1
  int A = std::max(5, LINES - 16);
  int B = (COLS - 10) / 2 * 2 + 1;
  clear();
  RefreshTitle_();
  WINDOW* win = derwin(stdscr, A - 3 + 1, COLS, 3, 0);
  box(win, 0, 0); // box of question
  delwin(win);
  win = derwin(stdscr, 3, COLS, A + 2, 0);
  box(win, 0, 0); // box of answer
  delwin(win);
  mvaddstr(2, 1, "Question:");
  mvaddstr(A + 1, 1, "Input your answer:");
  mvaddstr(A + 5, 1, errmsg_.c_str());
  mvaddstr(A + 6, 5, "Mark this problem as unsure (<F5> to toggle)");
  mvaddstr(A + 7, 5, "Give up this problem (<F6> to toggle)");
  win = derwin(stdscr, 2, COLS - 2, A + 8, 1);
  waddstr(win, "Press <UP><DOWN> to view the whole problem; <ESC> to abort the test.");
  delwin(win);
  mvaddstr(A + 10, 1, "Progress:");
  int num = B * progress_;
  mvaddstr(A + 11, 1, std::string(num, '=').c_str());
  mvprintw(A + 11, num + 2, "%.1f%%", progress_ * 100);
  mvaddch(A + 12, 1, '|');
  mvaddch(A + 12, (1 + B) / 2, '|');
  mvaddch(A + 12, B, '|');
  mvaddstr(A + 13, 1, "0");
  mvaddstr(A + 13, (1 + B) / 2, "50");
  mvaddstr(A + 13, B - 1, "100%");
  unsure_.MoveWindow(A + 6, 1);
  giveup_.MoveWindow(A + 7, 1);
  question_.ResizeWindow(A - 3 - 1, COLS - 2); // question textbox won't move
  question_.ResizeBuffer(1024, COLS - 2);
  answer_.MoveWindow(A + 3, 1);
  answer_.ResizeWindow(1, COLS - 2);
  if (msg_) msg_->Refresh();
}

QuestionScreen::QuestionScreen(const std::string& question, double progress)
    : question_(4, 1, 1, COLS - 2, false),
      answer_(std::max(5, LINES - 16) + 3, 1, 1, COLS - 2, true, false, 1,
              4096),
      unsure_(0, 0, KEY_F(5)),
      giveup_(0, 0, KEY_F(6)),
      progress_(progress),
      state_(0) {
  curs_set(1);
  unsure_.SetWindow(stdscr);
  giveup_.SetWindow(stdscr);
  question_.SetText(question);
  Resize_();
}

void QuestionScreen::SetCursor() {
  if (msg_) return;
  answer_.Refresh();
}

std::pair<QuestionScreen::Result, std::string> QuestionScreen::GetValue()
    const {
  Result res;
  if (msg_ && msg_->GetValue()) {
    res = kExit;
  } else if (unsure_.GetValue()) {
    res = kUnsure;
  } else if (giveup_.GetValue()) {
    res = kGiveUp;
  } else {
    res = kAnswer;
  }
  return {res, answer_.GetValue()};
}

bool QuestionScreen::ProcessKey(int ch) {
  if (ch == KEY_RESIZE) {
    Resize_();
  } else if (msg_) {
    if (msg_->ProcessKey(ch)) {
      if (msg_->GetValue()) return true;
      msg_.reset();
      curs_set(1);
      Resize_(); // repaint the whole screen when the message box exits
    }
  } else if (ch == 27) {
    msg_.emplace(std::vector<std::string>{"Are you sure to abort the test?",
                                          "The result won't be saved.", ""});
  } else if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_NPAGE || ch == KEY_PPAGE) {
    question_.ProcessKey(ch);
  } else if (ch == '\n') {
    if (giveup_.GetValue()) {
      if (state_ == 1 || answer_.size() == 0) return true;
      state_ = 1;
      errmsg_ = "Giving up with non-empty input. Press <ENTER> again to confirm.";
      RefreshErrMsg_();
    } else if (answer_.size() == 0) {
      state_ = 0;
      errmsg_ = "Error: Empty input.";
      RefreshErrMsg_();
    } else {
      return true;
    }
  } else {
    ch = answer_.ProcessKey(ch);
    if (ch) {
      ch = unsure_.ProcessKey(ch);
      if (!ch && unsure_.GetValue() && giveup_.GetValue()) giveup_.Toggle();
      ch = giveup_.ProcessKey(ch);
      if (!ch && unsure_.GetValue() && giveup_.GetValue()) unsure_.Toggle();
    }
  }
  if (!msg_) answer_.Refresh();
  return false;
}

void ViewScreen::Resize_() {
  clear();
  RefreshTitle_();
  WINDOW* win = derwin(stdscr, LINES - 3, COLS - 2, 2, 1);
  mvwaddstr(win, 0, 0, header_.c_str());
  int y, x;
  getyx(win, y, x);
  y += 2;
  delwin(win);
  int lines = LINES - (y + 2) - 3;
  win = derwin(stdscr, lines + 2, COLS, y + 1, 0);
  box(win, 0, 0);
  delwin(win);
  mvaddstr(LINES - 2, 1, "Press <ENTER> to continue.");
  wnoutrefresh(stdscr);
  text_.MoveWindow(y + 2, 1);
  text_.ResizeWindow(lines, COLS - 2);
  text_.ResizeBuffer(1024, COLS - 2);
}

ViewScreen::ViewScreen(const std::string& content, const std::string& header)
    : text_(4, 1, 1, COLS - 2, false, true, 1024), header_(header) {
  curs_set(0);
  text_.SetText(content);
  Resize_();
}

bool ViewScreen::ProcessKey(int ch) {
  if (ch == '\n') return true;
  if (ch == KEY_RESIZE) {
    Resize_();
  } else {
    text_.ProcessKey(ch);
  }
  return false;
}

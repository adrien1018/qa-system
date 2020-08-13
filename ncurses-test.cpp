#include <iostream>

#include <ncurses.h>
#include <list>
#include <string>
#include <variant>

class Buffer {
  struct Byte_ {
    unsigned char ch;
    bool move;
    std::pair<int, int> pos;
    Byte_() {}
    Byte_(unsigned char c) : ch(c), move(), pos() {}
  };
  std::list<Byte_> buf_;
  std::list<Byte_>::iterator cur_;
  struct Event_ {
    // To undo: erase [start, cur_), insert bytes to end, place cur_ to cur
    // (cur points into bytes, while start, end points to buf_)
    std::list<Byte_>::iterator start, cur;
    std::list<Byte_> bytes;
  } prev_; // leave it uninitialized (guarded by dirty)
  int maxheight_;
  int maxcol_;
  bool dirty_;
  std::list<Byte_>::iterator PrintBuffer_(WINDOW*);
 public:
  Buffer(int maxheight);
  std::pair<int, int> CurYX() const;
  int Lines() const;
  int Columns() const;
  bool IsDirty() const;
  std::string ToString() const;
  void MoveUp(int = 1);
  void MoveDown(int = 1);
  void MoveLeft();
  void MoveRight();
  void MoveLineStart();
  void MoveLineEnd();
  // Calling input functions will make the buffer into "dirty" state, in which
  // no futher cursor movements can be made (since the position of the
  // characters hasn't updated), and the `CurYX`, `Lines` functions will return
  // zero(s).
  // `Backspace` and `Delete` remove bytes based on UTF-8 rules. That is, they
  // find the previous / next byte in the 0x00-0x7f,0xc0-0xff range and remove
  // all bytes between them.
  void Backspace();
  void Delete();
  // `Insert` inserts one byte at a time.
  void Insert(unsigned char);
  // Automatically calls PrintBufferTruncate
  void SetMaxHeight(int, WINDOW*);
  // Printing the buffer, undoing or clearing will clear the "dirty" state.
  void Undo();
  void Clear();
  // The window must be at least one larger than maxheight,
  // otherwise the overflow detection will not work!
  void PrintBuffer(WINDOW*);
  void PrintBufferTruncate(WINDOW*);
  void SetCursor(WINDOW*) const;
};

Buffer::Buffer(int maxheight)
    : buf_(),
      cur_(buf_.begin()),
      maxheight_(maxheight),
      maxcol_(0),
      dirty_(false) {}

std::pair<int, int> Buffer::CurYX() const {
  if (dirty_ || cur_ == buf_.begin()) return {0, 0};
  return std::prev(cur_)->pos;
}

int Buffer::Lines() const {
  if (dirty_ || buf_.empty()) return 0;
  return buf_.rbegin()->pos.first + 1;
}

int Buffer::Columns() const {
  if (dirty_ || buf_.empty()) return 0;
  return maxcol_ + 1;
}

bool Buffer::IsDirty() const {
  return dirty_;
}

std::string Buffer::ToString() const {
  std::string str;
  for (auto& i : buf_) str.push_back(i.ch);
  return str;
}

void Buffer::MoveUp(int num) {
  if (dirty_) return;
  std::pair<int, int> now_pos = CurYX();
  if (now_pos.first < num) { // already on the first line
    cur_ = buf_.begin();
    return;
  }
  auto prv = cur_;
  for (--prv; prv->pos.first > now_pos.first - num;) cur_ = prv--;
  while (prv != buf_.begin() && prv->pos.first == now_pos.first - num &&
         (!prv->move || prv->pos.second > now_pos.second)) {
    cur_ = prv--;
  }
  if (!prv->move) cur_ = buf_.begin();
}

void Buffer::MoveDown(int num) {
  if (dirty_) return;
  std::pair<int, int> now_pos = CurYX();
  if (now_pos.first >= Lines() - num) { // already on the last line
    cur_ = buf_.end();
    return;
  }
  while (cur_->pos.first < now_pos.first + num) ++cur_;
  while (cur_->pos.first == now_pos.first + num &&
         cur_->pos.second < now_pos.second) {
    ++cur_;
  }
  if (cur_->pos.first == now_pos.first + num) ++cur_;
}

void Buffer::MoveLeft() {
  if (dirty_) return;
  if (cur_ == buf_.begin()) return;
  if (--cur_ == buf_.begin()) return;
  auto prv = std::prev(cur_);
  while (prv != buf_.begin() && !prv->move) cur_ = prv--;
  if (!prv->move) cur_ = buf_.begin();
}

void Buffer::MoveRight() {
  if (dirty_) return;
  if (cur_ == buf_.end()) return;
  while (!cur_->move) ++cur_;
  ++cur_;
}

void Buffer::MoveLineStart() {
  if (dirty_) return;
  if (cur_ == buf_.begin()) return;
  auto prv = std::prev(cur_);
  while (prv != buf_.begin() && prv->ch != '\n') cur_ = prv--;
  if (prv->ch != '\n') cur_ = buf_.begin();
}

void Buffer::MoveLineEnd() {
  if (dirty_) return;
  while (cur_ != buf_.end() && cur_->ch != '\n') ++cur_;
}

void Buffer::Backspace() {
  if (cur_ == buf_.begin()) return;
  auto start = std::prev(cur_);
  if (dirty_) {
    bool flag = false;
    if (start == prev_.start || cur_ == prev_.start) flag = true;
    while (start != buf_.begin() && 0x80 <= start->ch && start->ch < 0xc0) {
      --start;
      if (start == prev_.start) flag = true;
    }
    if (flag) {
      // start <= prev_.start <= cur_
      prev_.bytes.splice(prev_.start, buf_, start, prev_.start);
      buf_.erase(prev_.start, cur_);
      prev_.start = cur_;
    } else {
      // prev_.start < start < cur_
      buf_.erase(start, cur_);
    }
  } else {
    while (start != buf_.begin() && 0x80 <= start->ch && start->ch < 0xc0) {
      --start;
    }
    prev_.bytes.splice(prev_.bytes.begin(), buf_, start, cur_);
    prev_.start = cur_;
    prev_.cur = prev_.bytes.end();
    dirty_ = true;
  }
}

void Buffer::Delete() {
  if (cur_ == buf_.end()) return;
  auto end = std::next(cur_);
  while (end != buf_.end() && 0x80 <= end->ch && end->ch < 0xc0) ++end;
  prev_.bytes.splice(prev_.bytes.end(), buf_, cur_, end);
  if (!dirty_) {
    prev_.start = cur_;
    prev_.cur = prev_.bytes.begin();
    dirty_ = true;
  }
  cur_ = end;
}

void Buffer::Insert(unsigned char ch) {
  if (dirty_) {
    buf_.emplace(cur_, ch);
  } else {
    prev_.start = buf_.emplace(cur_, ch);
    prev_.cur = cur_;
    dirty_ = true;
  }
}

void Buffer::SetMaxHeight(int maxheight, WINDOW* win) {
  maxheight_ = maxheight;
  PrintBufferTruncate(win);
}

void Buffer::Undo() {
  if (!dirty_) return;
  dirty_ = false;
  buf_.erase(prev_.start, cur_);
  bool flag = prev_.bytes.empty();
  buf_.splice(cur_, prev_.bytes);
  if (!flag) cur_ = prev_.cur;
}

void Buffer::Clear() {
  buf_.clear();
  cur_ = buf_.begin();
  prev_.bytes.clear();
  dirty_ = false;
}

std::list<Buffer::Byte_>::iterator Buffer::PrintBuffer_(WINDOW* win) {
  maxcol_ = 0;
  wclear(win);
  wmove(win, 0, 0);
  std::pair<int, int> prev = {0, 0};
  for (auto it = buf_.begin(); it != buf_.end(); ++it) {
    waddch(win, it->ch);
    getyx(win, it->pos.first, it->pos.second);
    if (it->pos.first >= maxheight_) return it;
    if (it->pos.second > maxcol_) maxcol_ = it->pos.second;
    it->move = it->pos != prev;
    prev = it->pos;
  }
  SetCursor(win);
  return buf_.end();
}

void Buffer::PrintBuffer(WINDOW* win) {
  if (PrintBuffer_(win) != buf_.end()) { // rollback and reprint if overflow
    Undo();
    PrintBuffer_(win);
  }
  prev_.bytes.clear();
  dirty_ = false;
}

void Buffer::PrintBufferTruncate(WINDOW* win) {
  auto it = PrintBuffer_(win);
  if (it != buf_.end()) { // truncate if overflow
    // Truncate to whole UTF-8 character
    while (it != buf_.begin() && 0x80 <= it->ch && it->ch < 0xc0) --it;
    bool flag = cur_ == buf_.end();
    for (auto i = it; i != buf_.end() && !flag; i++) {
      if (cur_ == i) flag = true;
    }
    buf_.erase(it, buf_.end());
    if (flag) cur_ = buf_.end();
    PrintBuffer_(win);
  }
  prev_.bytes.clear();
  dirty_ = false;
}

void Buffer::SetCursor(WINDOW* win) const {
  std::pair<int, int> pos = CurYX();
  wmove(win, pos.first, pos.second);
}

static inline int Scroll(int old, int cur, int tot, int sz) {
  if (sz == 1) return cur;
  if (sz == 2) {
    if (cur >= old && cur < old + sz) return old;
    if (cur >= old + sz) return cur - 1;
    return cur;
  }
  if (cur > old && cur < old + sz - 1) return old;
  if (cur == old && cur == 0) return old;
  if (cur == old + sz - 1 && cur == tot - 1) return old;
  if (cur <= old) return std::max(0, cur - 1);
  return std::min(tot - sz, cur - sz + 2);
}

static inline int Move(int cur, int tot, int sz) {
  if (cur < 0 || tot <= sz) return 0;
  return std::min(std::max(tot - sz, 0), cur);
}

class Textbox {
  WINDOW* pad_;
  Buffer buf_;
  int posy_, posx_;
  int height_, width_;
  int maxheight_, maxwidth_;
  // multiline only affects key processing
  bool writable_, multiline_;
  int currow_, curcol_;
  void Redraw_(bool clr = true, bool truncate = false);
  void Refresh_(bool redraw = false);
 public:
  Textbox(int posy, int posx, int height, int width, bool writable = true,
          bool multiline = true, int maxheight = 2000, int maxwidth = -1);
  Textbox& operator=(const Textbox&) = delete;
  ~Textbox();
  std::string GetText() const;
  void Redraw() { Redraw_(); }
  void Refresh(bool redraw = true);
  void SetWritable(bool);
  void SetMultiline(bool);
  // To move / resize the window correctly, one may need to clear / refresh the
  // background screens (such as stdscr) beforehand
  void MoveWindow(int y, int x);
  void ResizeWindow(int height, int width);
  // Shrinking the buffer may truncate the content; if the size is smaller than
  // the window size, the window size will be shrinked to fit
  void ResizeBuffer(int maxheight, int maxwidth);
  void Clear();
  void SetText(const std::string&);
  // To make non-ASCII code overflow detection work, one need to call ProcessKey
  // with input_redraw = false on all but the last byte
  int ProcessKey(int, bool input_redraw = true);
};

Textbox::Textbox(int posy, int posx, int height, int width, bool writable,
                 bool multiline, int maxheight, int maxwidth)
    : buf_(maxheight < height ? height : maxheight),
      posy_(posy),
      posx_(posx),
      height_(height),
      width_(width),
      maxheight_(maxheight < height ? height : maxheight),
      maxwidth_(maxwidth < width ? width : maxwidth),
      writable_(writable),
      multiline_(multiline),
      currow_(0),
      curcol_(0) {
  pad_ = newpad(maxheight_ + 1, maxwidth_);
  Refresh();
}

Textbox::~Textbox() {
  delwin(pad_);
}

std::string Textbox::GetText() const {
  return buf_.ToString();
}

void Textbox::Redraw_(bool clr, bool truncate) {
  if (clr) wclear(pad_);
  if (truncate) {
    buf_.PrintBuffer(pad_);
  } else {
    buf_.PrintBufferTruncate(pad_);
  }
  Refresh_();
}

void Textbox::Refresh_(bool redraw) {
  if (writable_) {
    buf_.SetCursor(pad_);
    std::pair<int, int> pos = buf_.CurYX();
    int pr = currow_, pc = curcol_;
    currow_ = Scroll(currow_, pos.first, buf_.Lines(), height_);
    curcol_ = Scroll(curcol_, pos.second, buf_.Columns(), width_);
    redraw |= pc != curcol_ || pr != currow_;
  }
  Refresh(redraw);
}

void Textbox::Refresh(bool redraw) {
  pnoutrefresh(pad_, currow_, curcol_,
               posy_, posx_, posy_ + height_ - 1, posx_ + width_ - 1);
  if (redraw) redrawwin(pad_);
}

void Textbox::SetWritable(bool t) {
  writable_ = t;
}

void Textbox::SetMultiline(bool t) {
  multiline_ = t;
}

void Textbox::MoveWindow(int y, int x) {
  posy_ = y;
  posx_ = x;
  Refresh();
}

void Textbox::ResizeWindow(int height, int width) {
  height_ = height;
  width_ = width;
  Refresh_(true);
}

void Textbox::ResizeBuffer(int maxheight, int maxwidth) {
  if (maxheight < height_) height_ = maxheight;
  if (maxwidth < width_) width_ = maxwidth;
  maxheight_ = maxheight;
  maxwidth_ = maxwidth;
  delwin(pad_);
  pad_ = newpad(maxheight_ + 1, maxwidth_);
  buf_.SetMaxHeight(maxheight_, pad_);
  Refresh_(true);
}

void Textbox::Clear() {
  buf_.Clear();
  wclear(pad_);
  Refresh_();
}

void Textbox::SetText(const std::string& str) {
  buf_.Clear();
  for (auto i : str) buf_.Insert(i);
  Redraw_(true, true);
}

int Textbox::ProcessKey(int ch, bool input_redraw) {
  if (writable_) {
    switch (ch) {
      case KEY_UP: buf_.MoveUp(); Refresh_(); break;
      case KEY_DOWN: buf_.MoveDown(); Refresh_(); break;
      case KEY_LEFT: buf_.MoveLeft(); Refresh_(); break;
      case KEY_RIGHT: buf_.MoveRight(); Refresh_(); break;
      case KEY_PPAGE: buf_.MoveUp(std::max(1, height_ - 1)); Refresh_(); break;
      case KEY_NPAGE: buf_.MoveDown(std::max(1, height_ - 1)); Refresh_(); break;
      case KEY_HOME: buf_.MoveLineStart(); Refresh_(); break;
      case KEY_END: buf_.MoveLineEnd(); Refresh_(); break;
      case KEY_BACKSPACE: buf_.Backspace(); Redraw_(); break;
      case KEY_DC: buf_.Delete(); Redraw_(); break;
      default: {
        if ((multiline_ && ch == '\n') || (30 <= ch && ch < 256)) {
          buf_.Insert(ch);
          if (input_redraw) Redraw_();
        } else {
          return ch;
        }
      }
    }
  } else {
    int pr = currow_, pc = curcol_;
    switch (ch) {
      case KEY_UP:
        currow_ = std::max(0, currow_ - 1);
        Refresh(pr != currow_);
        break;
      case KEY_DOWN:
        currow_ = std::min(std::max(0, buf_.Lines() - height_), currow_ + 1);
        Refresh(pr != currow_);
        break;
      case KEY_LEFT:
        curcol_ = std::max(0, curcol_ - 1);
        Refresh(pc != curcol_);
        break;
      case KEY_RIGHT:
        curcol_ = std::min(std::max(0, buf_.Columns() - width_), curcol_ + 1);
        Refresh(pc != curcol_);
        break;
      case KEY_PPAGE:
        currow_ = std::max(0, currow_ - (height_ - 1));
        Refresh(pr != currow_);
        break;
      case KEY_NPAGE:
        currow_ = std::min(buf_.Lines() - height_, currow_ + (height_ - 1));
        Refresh(pr != currow_);
        break;
      case KEY_HOME:
        curcol_ = 0;
        Refresh(pc != curcol_);
        break;
      case KEY_END:
        curcol_ = std::max(0, buf_.Columns() - width_);
        Refresh(pc != curcol_);
        break;
      default: return ch;
    }
  }
  return 0;
}

#include <iostream>
#include <unistd.h>

int main() {
  setlocale(LC_CTYPE, "");
  initscr();
  keypad(stdscr, true);
  wnoutrefresh(stdscr);
  noecho();
  Textbox a(2, 2, 3, 60, true, true, 3, 60);
  a.SetText("012345678901234567890123456789012345678901234567890123456789"
            "012345678901234567890123456789012345678901234567890123456789"
            "01234567890123456789012345678901234567890123456789");
  doupdate();
  sleep(1);
  erase(); wnoutrefresh(stdscr);
  a.MoveWindow(3, 3);
  doupdate(); sleep(1); erase(); wnoutrefresh(stdscr);
  a.ResizeWindow(2, 59);
  doupdate(); sleep(1); erase(); wnoutrefresh(stdscr);
  a.ResizeBuffer(4, 59);
  a.ResizeWindow(4, 59);
  doupdate(); sleep(1); erase(); wnoutrefresh(stdscr);
  for (int i = 0; i < 200; i++) {
    a.ProcessKey(getch(), true);
    doupdate();
  }
  endwin();
  std::cout << a.GetText();
}

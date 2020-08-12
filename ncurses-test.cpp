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
    std::list<Byte_>::iterator pos;
    std::list<Byte_> bytes;
    bool del;
  } prev_; // leave it uninitialized (guarded by dirty)
  int maxheight_;
  int maxcol_;
  bool dirty_;
  bool PrintBuffer_(WINDOW*);
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
  // Printing the buffer, undoing or clearing will clear the "dirty" state.
  void Undo();
  void Clear();
  // The window must be at least one larger than maxheight,
  // otherwise the overflow detection will not work!
  void PrintBuffer(WINDOW*);
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

void Buffer::Backspace() {
  if (cur_ == buf_.begin()) return;
  dirty_ = true;
  auto start = std::prev(cur_);
  while (start != buf_.begin() && 0x80 <= start->ch && start->ch < 0xc0) {
    --start;
  }
  prev_.bytes.clear();
  prev_.bytes.splice(prev_.bytes.begin(), buf_, start, cur_);
  prev_.pos = cur_;
  prev_.del = false;
}

void Buffer::Delete() {
  if (cur_ == buf_.end()) return;
  dirty_ = true;
  auto end = std::next(cur_);
  while (end != buf_.end() && 0x80 <= end->ch && end->ch < 0xc0) ++end;
  prev_.bytes.clear();
  prev_.bytes.splice(prev_.bytes.begin(), buf_, cur_, end);
  prev_.pos = cur_ = end;
  prev_.del = true;
}

void Buffer::Insert(unsigned char ch) {
  dirty_ = true;
  prev_.pos = buf_.emplace(cur_, ch);
  prev_.bytes.clear();
}

void Buffer::Undo() {
  if (!dirty_) return;
  dirty_ = false;
  if (prev_.bytes.empty()) { // insert operation; delete the byte
    buf_.erase(prev_.pos);
  } else { // delete operation; insert the bytes
    auto tmp = prev_.bytes.begin();
    buf_.splice(prev_.pos, prev_.bytes);
    if (prev_.del) cur_ = tmp; // tmp now points to buf_
  }
}

void Buffer::Clear() {
  buf_.clear();
  cur_ = buf_.begin();
  dirty_ = false;
}

bool Buffer::PrintBuffer_(WINDOW* win) { // return false if overflows
  maxcol_ = 0;
  wclear(win);
  wmove(win, 0, 0);
  std::pair<int, int> prev = {0, 0};
  for (auto& i : buf_) {
    waddch(win, i.ch);
    getyx(win, i.pos.first, i.pos.second);
    if (i.pos.first >= maxheight_) return false;
    if (i.pos.second > maxcol_) maxcol_ = i.pos.second;
    i.move = i.pos != prev;
    prev = i.pos;
  }
  SetCursor(win);
  return true;
}

void Buffer::PrintBuffer(WINDOW* win) {
  if (!PrintBuffer_(win)) { // rollback and reprint if overflow
    Undo();
    PrintBuffer_(win);
  }
  dirty_ = false;
}

void Buffer::SetCursor(WINDOW* win) const {
  std::pair<int, int> pos = CurYX();
  wmove(win, pos.first, pos.second);
}

static inline int Scroll(int old, int cur, int tot, int sz) {
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
  void Redraw_(bool clr = true);
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
  void MoveWindow(int y, int x);
  void ResizeWindow(int height, int width);
  // Shrinking the buffer may truncate the content
  void ResizeBuffer(int maxheight, int maxwidth);
  void Clear();
  void SetText(const std::string&);
  int ProcessKey(int, bool = true);
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

void Textbox::Redraw_(bool clr) {
  if (clr) wclear(pad_);
  buf_.PrintBuffer(pad_);
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
  Refresh_();
}

void Textbox::ResizeBuffer(int maxheight, int maxwidth) {
  if (maxheight < height_) maxheight = height_;
  if (maxwidth < width_) maxwidth = width_;
  maxheight_ = maxheight;
  maxwidth_ = maxwidth;
  std::string str = GetText();
  buf_ = Buffer(maxheight);
  for (auto i : str) buf_.Insert(i);
  delwin(pad_);
  pad_ = newpad(maxheight_ + 1, maxwidth_);
  Redraw_(false);
}

void Textbox::Clear() {
  buf_.Clear();
  wclear(pad_);
  Refresh_();
}

void Textbox::SetText(const std::string& str) {
  buf_.Clear();
  for (auto i : str) buf_.Insert(i);
  Redraw_();
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
  a.MoveWindow(3, 3);
  redrawwin(stdscr);
  doupdate();
  sleep(1);
  a.ResizeWindow(2, 59);
  redrawwin(stdscr);
  doupdate();
  sleep(1);
  a.ResizeBuffer(4, 59);
  a.ResizeWindow(4, 59);
  redrawwin(stdscr);
  doupdate();
  for (int i = 0; i < 200; i++) {
    a.ProcessKey(getch(), true);
    doupdate();
  }
  endwin();
  std::cout << a.GetText();
}

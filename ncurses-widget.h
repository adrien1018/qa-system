#ifndef NCURSES_WIDGET_H_
#define NCURSES_WIDGET_H_

#include <list>
#include <string>
#include <vector>
#include <menu.h>
#include <ncurses.h>

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
  size_t size() const { return buf_.size(); }
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
  ~Textbox();
  Textbox(const Textbox&) = delete;
  Textbox& operator=(const Textbox&) = delete;
  WINDOW* GetWin();
  std::string GetValue() const;
  size_t size() const { return buf_.size(); }
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

class Menu {
  WINDOW *win_, *sub_;
  MENU* menu_;
  std::vector<ITEM*> items_;
  std::vector<std::string> choices_;
  int posy_, posx_;
  int height_, width_;
  int suby_, subx_;
  int subheight_, subwidth_;
  void Build_();
  void Destroy_();
 public:
  Menu(const std::vector<std::string>&, int posy, int posx, int height,
       int width, int suby = 0, int subx = 0, int subheight = -1,
       int subwidth = -1);
  ~Menu();
  Menu(const Menu&) = delete;
  Menu& operator=(const Menu&) = delete;
  WINDOW* GetWin();
  int GetValue() const;
  void Refresh();
  void MoveWindow(int y, int x);
  void ResizeWindow(int height, int width, int suby = -1, int subx = -1,
                    int subheight = -1, int subwidth = -1);
  template <class Func>
  void ResizeWindowCallback(Func&& callback, int height, int width,
                            int suby = -1, int subx = -1, int subheight = -1,
                            int subwidth = -1) {
    int item = GetValue();
    Destroy_();
    height_ = height;
    width_ = width;
    if (suby >= 0) suby_ = suby;
    if (subx >= 0) subx_ = subx;
    subheight_ = subheight <= 0 ? height_ - suby_ : subheight;
    subwidth_ = subwidth <= 0 ? width_ - subx_ : subwidth;
    callback(choices_, subheight_, subwidth_);
    Build_();
    item = std::min(item, (int)choices_.size() - 1);
    if (choices_.size()) set_current_item(menu_, items_[item]);
    Refresh();
  }
  int ProcessKey(int);
};

class CheckBox {
  WINDOW* win_;
  int toggle_, posy_, posx_;
  char on_, off_;
  bool selected_;
 public:
  // Print directly on window; if nullptr, nothing is displayed
  CheckBox(int posy, int posx, int toggle = '\n', WINDOW* = nullptr,
           char on = 'v', char off = ' ');
  // Note: this function does not refresh
  void SetWindow(WINDOW*);
  bool GetValue() const;
  void Refresh();
  void Toggle();
  void MoveWindow(int y, int x);
  void SetChar(char on, char off = ' ');
  void SetToggle(int);
  int ProcessKey(int);
};

struct Button {
  std::string str;
  int y, x;
  int left, right, up, down;
};

class ButtonGroup {
  WINDOW* win_;
  std::vector<Button> buttons_;
  int selected_;
 public:
  // Print directly on window; if nullptr, nothing is displayed
  ButtonGroup(const std::vector<Button>&, WINDOW* = nullptr);
  // Note: this function does not refresh
  void SetWindow(WINDOW*);
  void Refresh();
  // Must clear the old buttons manually
  void ResetButtons(const std::vector<Button>&);
  void Unselect();
  void Select(int);
  int GetValue() const;
  int ProcessKey(int);
};

#endif // NCURSES_WIDGET_H_

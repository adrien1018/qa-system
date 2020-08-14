#include "ncurses-utils.h"

size_t StringWidth(const std::string& str, size_t max) {
  WINDOW* win = newwin(2, max, 0, 0); // not refreshed so not displayed
  waddstr(win, str.c_str());
  int y, x;
  getyx(win, y, x);
  size_t ret = y == 1 ? max : x;
  delwin(win);
  return ret;
}

size_t PrefixFit(const std::string& str, size_t cols) {
  WINDOW* win = newwin(2, cols, 0, 0); // not refreshed so not displayed
  for (size_t i = 0; i < str.size(); i++) {
    int y, x;
    waddch(win, str[i]);
    getyx(win, y, x);
    if (y == 1 && x > 0) return i;
  }
  return str.size();
}

void PrintCenter(WINDOW* win, const std::string& str, int y, int left,
                 int right) {
  int width = StringWidth(str, right - left);
  mvwaddstr(win, y, CenterStart(left, right, width), str.c_str());
}

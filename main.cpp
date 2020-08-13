#include <iostream>
#include <unistd.h>
#include "ncurses-widget.h"

int main() {
  setlocale(LC_CTYPE, "");
  initscr();
  keypad(stdscr, true);
  wnoutrefresh(stdscr);
  noecho();
  CheckBox a(4, 4, ' ');
  doupdate();
  for (int i = 0; i < 10; i++) {
    a.ProcessKey(getch());
    doupdate();
  }
  sleep(1);
  int res = a.GetValue();
  endwin();
  std::cout << res << std::endl;
}

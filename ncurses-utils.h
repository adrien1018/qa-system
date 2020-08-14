#ifndef NCURSES_UTILS_H_
#define NCURSES_UTILS_H_

#include <string>
#include <ncurses.h>

size_t StringWidth(const std::string& str, size_t max = 4096);
size_t PrefixFit(const std::string& str, size_t cols);

inline constexpr int CenterStart(int left, int right, int width) {
  return left + (right - left - width) / 2;
}

// Single-line strings only
void PrintCenter(WINDOW*, const std::string& str, int y, int left, int right);

#endif

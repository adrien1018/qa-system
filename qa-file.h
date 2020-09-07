#ifndef QA_FILE_H_
#define QA_FILE_H_

#include <deque>
#include <string>
#include <vector>
#include <unordered_set>

struct Question {
  size_t id;
  std::string description, answer;
  bool case_sensitive;
};

int Score(const Question&, const std::string& user_ans,
          const std::unordered_set<wchar_t>& ignore_chars);

struct QuestionSet {
  std::string title;
  std::unordered_set<wchar_t> ignore_chars;
  std::vector<Question> questions;
};

QuestionSet ReadCSV(const std::string& filename);

extern const std::string kHistoryHeader;

class TestResult {
 public:
  std::string file;
  struct WrongAnswer {
    size_t id;
    std::string ans; // empty string: give up
  };
  std::vector<size_t> ord;
  std::unordered_set<size_t> unsure;
  std::vector<WrongAnswer> wa;
  time_t finish;
  double elapsed;
  int score, fullmark;
  std::string GetMenuText(int width) const;
  std::string GetSummary(bool full) const;
  std::string GetReview(const QuestionSet&, bool full) const;
};

bool ExportHistory(const std::string& filename, const std::deque<TestResult>&);
std::deque<TestResult> ReadHistory(const std::string& filename);

#endif // QA_FILE_H_

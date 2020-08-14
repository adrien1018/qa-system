#include "qa-file.h"

#include <cwctype>
#include <locale>
#include <codecvt>
#include <fstream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "ncurses-utils.h"

static inline std::wstring FromUTF8(const std::string& str) {
  static std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
  return conv.from_bytes(str);
}

int Score(const Question& q, const std::string& user_ans,
          const std::unordered_set<wchar_t>& ignore_chars) {
  if (user_ans.empty()) return 0; // give up
  if (q.case_sensitive || ignore_chars.size()) {
    std::wstring ans = FromUTF8(user_ans);
    std::wstring user = FromUTF8(user_ans);
    if (q.case_sensitive) {
      for (auto& i : ans) i = std::towupper(i);
      for (auto& i : user) i = std::towupper(i);
    }
    if (ignore_chars.size()) {
      auto FilterRule = [&ignore_chars](wchar_t i) { return !ignore_chars.count(i); };
      ans.resize(std::stable_partition(ans.begin(), ans.end(), FilterRule) -
                 ans.begin());
      user.resize(std::stable_partition(user.begin(), user.end(), FilterRule) -
                  user.begin());
    }
    return ans == user;
  } else {
    return q.answer == user_ans;
  }
}

static std::pair<bool, std::vector<std::string>> CSVLineToVector(std::ifstream& fin) {
  std::vector<std::string> ans;
  std::string current;
  int in_quote = 0;
  char ch;
  while (fin.get(ch)) {
    if (in_quote == 2) {
      if (ch == ',') {
        ans.emplace_back(std::move(current));
        current.clear();
      } else if (ch == '\"') {
        in_quote = 1;
        current.push_back(ch);
      } else { // error
        in_quote = 0;
        current.push_back(ch);
      }
    } else if (in_quote == 1) {
      if (ch == '\"') {
        in_quote = 2;
      } else {
        current.push_back(ch);
      }
    } else {
      switch (ch) {
        case '\"': in_quote = 1; break;
        case ',':
          ans.emplace_back(std::move(current));
          current.clear();
          break;
        case '\r': fin.get(); [[fallthrough]]; // should be '\n'
        case '\n': ans.emplace_back(std::move(current)); return {true, ans};
        case (char)0xfe: case (char)0xff: break; // Invalid UTF-8; ignore because of BOM
        default: current.push_back(ch);
      }
    }
  }
  // File ends before EOL; technically invalid CSV
  if (current.size()) ans.emplace_back(std::move(current));
  if (ans.size()) return {true, ans};
  return {false, ans}; // Valid: after last '\n'
}

QuestionSet ReadCSV(const std::string& filename) {
  std::ifstream fin(filename);
  if (!fin.is_open()) return {};
  auto line = CSVLineToVector(fin);
  if (!line.first) return {};
  QuestionSet ret;
  if (line.second.size() > 0) ret.title = line.second[0];
  bool default_case_sensitive = line.second.size() > 1 && line.second[1] == "1";
  if (line.second.size() > 2) {
    std::wstring str = FromUTF8(line.second[2]);
    for (auto& i : str) ret.ignore_chars.insert(i);
  }
  for (size_t i = 0; line = CSVLineToVector(fin), line.first; i++) {
    ret.questions.push_back({i, line.second.size() > 0 ? line.second[0] : "",
                             line.second.size() > 1 ? line.second[1] : "",
                             line.second.size() > 2 && line.second[2].size()
                                 ? line.second[2] == "1"
                                 : default_case_sensitive});
  }
  return ret;
}

const std::string kHistoryHeader =
    "Filename                    Score  Tot.Ques.  Elapsed(s)     Date/Time";
   //0    |    ^10  |    ^20  |    ^30  |    ^40  |    ^50  |    ^60  |   ^70
   ///home/a/Meow.csv               88         99       2.555 2020-08-14 01:02:03

std::string TestResult::GetMenuText() const {
  size_t num = PrefixFit(file, 28);
  if (num < file.size()) {
    while (num && 0x80 <= (unsigned char)file[num] &&
           (unsigned char)file[num] < 0xc0) {
      --num;
    }
  }
  std::string display_file = file.substr(0, num);
  size_t width = StringWidth(display_file);
  char buf[50], datebuf[22];
  strftime(datebuf, sizeof(datebuf), "%Y-%m-%d %H:%M:%S", localtime(&finish));
  snprintf(buf, sizeof(buf), "%5d%11d%12.3lf%20s", score, (int)ord.size(),
           elapsed, datebuf);
  return display_file + std::string(28 - width, ' ') + buf;
}

std::string TestResult::GetSummary(bool full) const {
  std::string ret;
  if (full) ret = "Question file: " + file + '\n';
  char buf[100];
  snprintf(buf, sizeof(buf), "Score: %d/%d\n", score, fullmark);
  ret += buf;
  snprintf(buf, sizeof(buf), "Elapsed time: %.3lf s\n", elapsed);
  ret += buf;
  if (full) {
    strftime(buf, sizeof(buf), "Finish time: %Y-%m-%d %H:%M:%S\n",
             localtime(&finish));
    ret += buf;
  }
  return ret;
}

std::string TestResult::GetReview(const QuestionSet& qs, bool full) const {
  std::string ret = GetSummary(full);
  ret += "\nReview:\n";
  std::unordered_set<size_t> wa_ids;
  for (auto& i : wa) {
    if (unsure.count(i.id)) {
      ret += "[incorrect, unsure] ";
    } else {
      ret += "[incorrect] ";
    }
    auto& q = qs.questions[i.id];
    ret += "Question: " + q.description + ", answer: " + q.answer;
    if (i.ans.empty()) {
      ret += ", you gave up this question (Q";
    } else {
      ret += ", your answer: " + i.ans + " (Q";
    }
    ret += std::to_string(i.id + 1) + ")\n";
    wa_ids.insert(i.id);
  }
  for (auto& id : unsure) {
    if (wa_ids.count(id)) continue;
    auto& q = qs.questions[id];
    ret += "[unsure] Question: " + q.description + ", answer: " + q.answer +
           " (Q" + std::to_string(id + 1) + ")\n";
  }
  if (!full) ret.pop_back();
  return ret;
}

bool ExportHistory(const std::string& filename,
                   const std::deque<TestResult>& hist) {
  std::ofstream fout(filename);
  if (!fout.is_open()) return false;
  using JSON = nlohmann::json;
  JSON ret;
  for (auto& i : hist) {
    JSON entry;
    entry["file"] = i.file;
    entry["order"] = i.ord;
    entry["unsure"] = i.unsure;
    for (auto& j : i.wa) entry["wa"].push_back(JSON{j.id, j.ans});
    entry["time"] = i.finish;
    entry["elapsed"] = i.elapsed;
    entry["score"] = i.score;
    entry["fullmark"] = i.fullmark;
    ret.push_back(std::move(entry));
  }
  fout << ret;
  return true;
}

std::deque<TestResult> ReadHistory(const std::string& filename) {
  std::ifstream fin(filename);
  if (!fin.is_open()) return {};
  using JSON = nlohmann::json;
  JSON json;
  fin >> json;
  std::deque<TestResult> ret;
  for (auto& i : json) {
    TestResult tr;
    tr.file = i["file"].get<std::string>();
    for (auto& j : i["order"]) tr.ord.push_back(j);
    for (auto& j : i["unsure"]) tr.unsure.insert(j.get<size_t>());
    tr.finish = i["time"];
    tr.elapsed = i["elapsed"];
    tr.score = i["score"];
    tr.fullmark = i["fullmark"];
    for (auto& j : i["wa"]) {
      tr.wa.push_back({j[0].get<size_t>(), j[1].get<std::string>()});
    }
    ret.push_back(std::move(tr));
  }
  return ret;
}

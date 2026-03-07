#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <set>
#include <mutex>
#include <queue>

#include "token.h"
using namespace std;

#define current_linenum __LINE__

template<typename... T>
string str(T... args) {
    ostringstream oss;
    ((oss << args), ...);
    string result = oss.str();
    return result;
}

template<typename... T>
string sen(T... args) {
    ostringstream oss;
    ((oss << args << " "), ...);
    string result = oss.str();
    if (!result.empty()) result.pop_back(); // remove trailing space
    return result;
}

enum ErrorType {
  USER,
  WARNING,
  GENERAL,
  SYNTAX,
  SEMANTIC,
  COMPTIME,
  TYPE,
  INTERNAL,
  RUNTIME
};

struct SageError;

class ErrorLogger {
private:
  vector<SageError*> errors;
  vector<SageError*> internal_errors;

  mutex error_mutex;
  queue<SageError*> pending_comptime_errors;

  set<size_t> error_hashes;
  int error_amount = 0;
  int warning_amount = 0;
  bool warnings_are_errors = false;
  string outfile_name = "./compiler_dev_log.log";
  bool errors_logged = false;

  ErrorLogger();
  ~ErrorLogger();

public:
  ErrorLogger(const ErrorLogger&) = delete;
  ErrorLogger &operator=(const ErrorLogger&) = delete;

  static ErrorLogger &get();

  // thread safe
  void log_error_safe(Token &token, string message, ErrorType type);
  void log_error_safe(string filename, int lineno, string message, ErrorType);
  void log_warning_safe(Token &token, string message, ErrorType type);

  // non thread safe
  void log_error_unsafe(Token &token, string message, ErrorType type);
  void log_error_unsafe(string filename, int lineno, string message, ErrorType);
  void log_warning_unsafe(Token &token, string message, ErrorType type);

  bool has_errors();
  void report_errors();
  void collect_pending_errors();
};

struct SageError {
  int line_number = 0;
  int column_number = 0;
  ErrorType error_type = ErrorType::GENERAL;
  string message = "";
  string filename = "";

  // ANSI color codes
  static constexpr const char *RED = "\033[31m";
  static constexpr const char *BLUE = "\033[34m";
  static constexpr const char *YELLOW = "\033[33m";
  static constexpr const char *RESET = "\033[0m";
  static constexpr const char *BOLD = "\033[1m";

  SageError();
  SageError(string msg, string filename, int line_number, int column_number, ErrorType type);
  string get_error_type_string();
  vector<string> read_file_lines();
  string print();
  void print_error();
  size_t hash() const;
};



#include "../include/error_logger.h"
#include <fstream>
#include <iostream>
#include <vector>
using namespace std;

SageError::SageError() {
}

SageError::SageError(
    string msg,
    string filename,
    int line_number,
    int column_number,
    ErrorType error_type)
    : line_number(line_number),
      column_number(column_number),
      error_type(error_type),
      message(msg),
      filename(filename) {
}

size_t SageError::hash() const {
    size_t h1 = std::hash<string>{}(filename);
    size_t h2 = std::hash<int>{}(line_number);
    size_t h3 = std::hash<int>{}(column_number);
    size_t h4 = std::hash<int>{}(static_cast<int>(error_type));
    size_t h5 = std::hash<string>{}(message);

    // Combine hashes using XOR and bit shifting
    return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
}

string SageError::get_error_type_string() {
    switch (error_type) {
        case ErrorType::SYNTAX: return "syntax error";
        case ErrorType::SEMANTIC: return "semantic error";
        case ErrorType::GENERAL: return "error";
        case ErrorType::WARNING: return "warning";
        case ErrorType::TYPE: return "type error";
        case ErrorType::INTERNAL: return "internal error";
        default: return "error";
    }
}

vector<string> SageError::read_file_lines() {
    vector<string> lines;
    ifstream file(filename);
    string line;

    if (!file.is_open()) {
        return lines; // Return empty vector if file can't be opened
    }

    while (std::getline(file, line)) {
        lines.push_back(line);
    }

    return lines;
}

string SageError::print() {
    std::ostringstream output;

    // Main error line: "error: message"
    output << RED << BOLD << get_error_type_string() << ": " << RESET
            << BOLD << message << RESET << "\n";

    // Location line: " --> filename:line:column"
    if (!filename.empty() && line_number > 0) {
        output << BLUE << " --> " << RESET << filename << ":" << line_number << ":" << column_number << "\n";

        // Try to read the file and show the problematic line
        auto lines = read_file_lines();
        if (!lines.empty() && line_number <= static_cast<int>(lines.size())) {
            // Calculate padding for line numbers
            int max_line_digits = to_string(line_number + 1).length();

            // Empty line before code
            output << BLUE;
            for (int i = 0; i < max_line_digits; ++i) output << " ";
            output << " |" << RESET << "\n";

            // Show the line with error
            output << BLUE << line_number;
            for (int i = to_string(line_number).length(); i < max_line_digits; ++i) {
                output << " ";
            }
            output << " | " << RESET << lines[line_number - 1] << "\n";

            // Show pointer to error location
            if (column_number > 0) {
                output << BLUE;
                for (int i = 0; i < max_line_digits; ++i) output << " ";
                output << " | " << RESET;

                // Add spaces up to the error column
                for (int i = 1; i < column_number; ++i) output << " ";
                output << RED << "^" << RESET << "\n";
            }
        }
    }

    return output.str();
}

void SageError::print_error() {
    cout << print();
}


ErrorLogger::ErrorLogger() {
    error_amount = 0;
    warning_amount = 0;
}

ErrorLogger::~ErrorLogger() {
    // if (internal_errors.size() > 0) {
    //     ofstream outfile(outfile_name, ios::app);
    //     string fulllog = "";
    //     for (int i = 0; i < internal_errors.size(); ++i) {
    //         fulllog = fulllog + internal_errors[i]->print();
    //     }
    //     outfile << fulllog;
    //     outfile.close();
    // }

    for (int i = 0; i < error_amount + warning_amount; ++i) {
        delete errors[i];
    }
}

void ErrorLogger::log_internal_error(
    string sourcefile,
    int lineno,
    string message) {
    SageError *error = new SageError(
        message,
        sourcefile,
        lineno,
        0,
        INTERNAL);

    if (error_hashes.find(error->hash()) != error_hashes.end()) {
        delete error;
        return;
    }

    errors.push_back(error);
    error_hashes.insert(error->hash());
}

void ErrorLogger::log_error(
    string filename,
    int lineno,
    string message,
    ErrorType type) {
    SageError *error = new SageError(
        message,
        filename,
        lineno,
        0,
        type);

    if (error_hashes.find(error->hash()) != error_hashes.end()) {
        delete error;
        return;
    }

    error_amount++;

    errors.push_back(error);
    error_hashes.insert(error->hash());
}

void ErrorLogger::log_error(
    Token &token,
    string message,
    ErrorType type) {
    SageError *error = new SageError(
        message,
        token.filename,
        token.linenum,
        token.linedepth,
        type);

    if (error_hashes.find(error->hash()) != error_hashes.end()) {
        delete error;
        return;
    }

    error_amount++;

    errors.push_back(error);
    error_hashes.insert(error->hash());
}

void ErrorLogger::log_warning(
    Token &token,
    string message,
    ErrorType type) {
    SageError *error = new SageError(
        message,
        token.filename,
        token.linenum,
        token.linedepth,
        type);

    if (error_hashes.find(error->hash()) != error_hashes.end()) {
        delete error;
        return;
    }

    warning_amount++;

    errors.push_back(error);
    error_hashes.insert(error->hash());
}

bool ErrorLogger::has_errors() {
    return (error_amount > 0 || (warnings_are_errors && warning_amount > 0));
}

void ErrorLogger::report_errors() {
    if (errors_logged) {
        return;
    }

    string output;
    for (auto error: errors) {
        output = error->print();
        printf("%s\n", output.c_str());
    }
    errors_logged = true;
}

ErrorLogger &ErrorLogger::get() {
    static ErrorLogger instance;
    return instance;
}

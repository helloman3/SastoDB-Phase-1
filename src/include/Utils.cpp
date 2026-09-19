#pragma once
#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace Utils {
    inline string trim(const string& text) {
        int start = 0;
        while (start < (int)text.length() && (text[start] == ' ' || text[start] == '\t')) {
            start++;
        }

        int end = (int)text.length() - 1;
        while (end >= start && (text[end] == ' ' || text[end] == '\t' || text[end] == '\r' || text[end] == '\n')) {
            end--;
        }

        if (start > end) return "";
        return text.substr(start, end - start + 1);
    }

    inline string toUpper(string text) {
        for (int i = 0; i < (int)text.length(); i++) {
            text[i] = toupper(text[i]);
        }
        return text;
    }

    inline string stripQuotes(string text) {
        text = trim(text);
        if (text.length() >= 2) {
            char first = text.front();
            char last = text.back();
            if ((first == '\'' && last == '\'') || (first == '"' && last == '"')) {
                return text.substr(1, text.length() - 2);
            }
        }
        return text;
    }

    inline bool isNumeric(string text) {
        text = trim(text);
        if (text.empty()) return false;

        int startIndex = 0;
        if (text[0] == '-' || text[0] == '+') {
            startIndex = 1;
        }
        if (startIndex == (int)text.length()) return false;

        for (int i = startIndex; i < (int)text.length(); i++) {
            if (!isdigit(text[i])) return false;
        }
        return true;
    }

    inline int stringToInt(const string& text) {
        try {
            return stoi(trim(text));
        } catch (...) {
            return 0;
        }
    }

    inline double stringToFloat(const string& text) {
        try {
            return stod(trim(text));
        } catch (...) {
            return 0.0;
        }
    }

    inline vector<string> split(const string& text, char delimiter) {
        vector<string> parts;
        string currentWord = "";
        bool insideQuotes = false;

        for (int i = 0; i < (int)text.length(); i++) {
            char c = text[i];

            if (c == '\'' || c == '"') {
                insideQuotes = !insideQuotes;
                currentWord += c;
            } else if (c == delimiter && !insideQuotes) {
                parts.push_back(trim(currentWord));
                currentWord = "";
            } else {
                currentWord += c;
            }
        }

        parts.push_back(trim(currentWord));
        return parts;
    }
}

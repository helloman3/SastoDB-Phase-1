#pragma once
#include <iostream>
#include <string>
#include <vector>

using namespace std;

class Row {
private:
    vector<string> values;

public:
    Row() {}

    Row(int size) {
        values.resize(size, "");
    }

    Row(const vector<string>& initialValues) {
        values = initialValues;
    }

    int getCount() const {
        return values.size();
    }

    string get(int index) const {
        if (index >= 0 && index < (int)values.size()) {
            return values[index];
        }
        return "";
    }

    void set(int index, const string& value) {
        if (index >= 0 && index < (int)values.size()) {
            values[index] = value;
        }
    }

    void add(const string& value) {
        values.push_back(value);
    }

    string& operator[](int index) {
        return values[index];
    }

    const string& operator[](int index) const {
        return values[index];
    }

    friend ostream& operator<<(ostream& os, const Row& row) {
        os << "| ";
        for (int i = 0; i < (int)row.values.size(); i++) {
            os << row.values[i];
            if (i + 1 < (int)row.values.size()) {
                os << " | ";
            } else {
                os << " |";
            }
        }
        return os;
    }
};

#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include "Column.cpp"
#include "Row.cpp"
#include "Utils.cpp"

using namespace std;

class Table {
private:
    string name;
    vector<Column> columns;
    vector<Row> rows;

    bool matchCondition(const string& rowVal, const string& colType, const string& op, const string& targetVal) const {
        string cleanTarget = Utils::stripQuotes(targetVal);

        if (colType == "INT" || colType == "INTEGER") {
            int val1 = Utils::stringToInt(rowVal);
            int val2 = Utils::stringToInt(cleanTarget);
            if (op == "=" || op == "==") return val1 == val2;
            if (op == "!=") return val1 != val2;
            if (op == "<") return val1 < val2;
            if (op == ">") return val1 > val2;
            if (op == "<=") return val1 <= val2;
            if (op == ">=") return val1 >= val2;
        } else if (colType == "FLOAT" || colType == "DOUBLE") {
            double val1 = Utils::stringToFloat(rowVal);
            double val2 = Utils::stringToFloat(cleanTarget);
            if (op == "=" || op == "==") return val1 == val2;
            if (op == "!=") return val1 != val2;
            if (op == "<") return val1 < val2;
            if (op == ">") return val1 > val2;
            if (op == "<=") return val1 <= val2;
            if (op == ">=") return val1 >= val2;
        } else {
            if (op == "=" || op == "==") return rowVal == cleanTarget;
            if (op == "!=") return rowVal != cleanTarget;
            if (op == "<") return rowVal < cleanTarget;
            if (op == ">") return rowVal > cleanTarget;
        }
        return false;
    }

    void printCell(const string& text, int width) const {
        cout << " " << left << setw(width) << text << " |";
    }

    void printBorder(const vector<int>& widths) const {
        cout << "+";
        for (int w : widths) {
            cout << string(w + 2, '-') << "+";
        }
        cout << "\n";
    }

public:
    Table() {
        name = "";
    }

    Table(string tableName) {
        name = tableName;
    }

    string getName() const { return name; }
    int getColumnCount() const { return columns.size(); }
    int getRowCount() const { return rows.size(); }

    void addColumn(const Column& col) {
        columns.push_back(col);
    }

    int getColumnIndex(const string& colName) const {
        for (int i = 0; i < (int)columns.size(); i++) {
            if (Utils::toUpper(columns[i].getName()) == Utils::toUpper(colName)) {
                return i;
            }
        }
        return -1;
    }

    bool insertRow(const Row& row, string& errorMsg) {
        if (row.getCount() != (int)columns.size()) {
            errorMsg = "Column count mismatch: Expected " + to_string(columns.size()) +
                       " values, received " + to_string(row.getCount());
            return false;
        }

        for (int i = 0; i < (int)columns.size(); i++) {
            string val = row.get(i);
            bool isNull = (val.empty() || Utils::toUpper(val) == "NULL");

            if (columns[i].getIsNotNull() && isNull) {
                errorMsg = "Column '" + columns[i].getName() + "' cannot be NULL.";
                return false;
            }

            if (!isNull && columns[i].isInt() && !Utils::isNumeric(val)) {
                errorMsg = "Invalid integer value '" + val + "' for column '" + columns[i].getName() + "'.";
                return false;
            }

            if ((columns[i].getIsPrimaryKey() || columns[i].getIsUnique()) && !isNull) {
                for (int r = 0; r < (int)rows.size(); r++) {
                    if (rows[r].get(i) == val) {
                        string constraint = columns[i].getIsPrimaryKey() ? "PRIMARY KEY" : "UNIQUE";
                        errorMsg = "Duplicate value '" + val + "' violates " + constraint + " constraint on column '" + columns[i].getName() + "'.";
                        return false;
                    }
                }
            }
        }

        rows.push_back(row);
        return true;
    }

    bool selectRows(const string& whereCol, const string& op, const string& whereVal, int& outCount) const {
        outCount = 0;
        int whereIdx = -1;

        if (!whereCol.empty()) {
            whereIdx = getColumnIndex(whereCol);
            if (whereIdx == -1) {
                cout << "Error: WHERE column '" << whereCol << "' not found.\n";
                return false;
            }
        }

        if (columns.empty()) {
            cout << "Table '" << name << "' has no columns.\n";
            return true;
        }

        vector<int> widths(columns.size());
        for (int i = 0; i < (int)columns.size(); i++) {
            widths[i] = columns[i].getName().length();
        }

        vector<int> matchedRowIndexes;
        for (int r = 0; r < (int)rows.size(); r++) {
            bool match = true;
            if (whereIdx >= 0) {
                match = matchCondition(rows[r].get(whereIdx), columns[whereIdx].getType(), op, whereVal);
            }
            if (match) {
                matchedRowIndexes.push_back(r);
                for (int i = 0; i < (int)columns.size(); i++) {
                    int len = rows[r].get(i).length();
                    if (len > widths[i]) widths[i] = len;
                }
            }
        }
        outCount = matchedRowIndexes.size();

        printBorder(widths);
        cout << "|";
        for (int i = 0; i < (int)columns.size(); i++) {
            printCell(columns[i].getName(), widths[i]);
        }
        cout << "\n";
        printBorder(widths);

        for (int rIdx : matchedRowIndexes) {
            cout << "|";
            for (int i = 0; i < (int)columns.size(); i++) {
                printCell(rows[rIdx].get(i), widths[i]);
            }
            cout << "\n";
        }
        printBorder(widths);

        return true;
    }

    bool updateRows(const string& targetCol, const string& newVal,
                    const string& whereCol, const string& op, const string& whereVal,
                    int& updatedCount, string& errorMsg) {
        updatedCount = 0;
        int targetIdx = getColumnIndex(targetCol);
        if (targetIdx == -1) {
            errorMsg = "Target column '" + targetCol + "' not found.";
            return false;
        }

        int whereIdx = -1;
        if (!whereCol.empty()) {
            whereIdx = getColumnIndex(whereCol);
            if (whereIdx == -1) {
                errorMsg = "WHERE column '" + whereCol + "' not found.";
                return false;
            }
        }

        string cleanVal = (columns[targetIdx].isText()) ? Utils::stripQuotes(newVal) : Utils::trim(newVal);
        bool isNull = (cleanVal.empty() || Utils::toUpper(cleanVal) == "NULL");

        if (columns[targetIdx].getIsNotNull() && isNull) {
            errorMsg = "Column '" + columns[targetIdx].getName() + "' cannot be NULL.";
            return false;
        }

        if (!isNull && columns[targetIdx].isInt() && !Utils::isNumeric(cleanVal)) {
            errorMsg = "Invalid integer value '" + cleanVal + "' for column '" + columns[targetIdx].getName() + "'.";
            return false;
        }

        int matchCount = 0;
        for (int r = 0; r < (int)rows.size(); r++) {
            bool match = (whereIdx >= 0) ? matchCondition(rows[r].get(whereIdx), columns[whereIdx].getType(), op, whereVal) : true;
            if (match) matchCount++;
        }

        if ((columns[targetIdx].getIsPrimaryKey() || columns[targetIdx].getIsUnique()) && !isNull) {
            if (matchCount > 1) {
                string constraint = columns[targetIdx].getIsPrimaryKey() ? "PRIMARY KEY" : "UNIQUE";
                errorMsg = "Cannot update multiple rows to identical value under " + constraint + " constraint.";
                return false;
            }
            for (int r = 0; r < (int)rows.size(); r++) {
                bool match = (whereIdx >= 0) ? matchCondition(rows[r].get(whereIdx), columns[whereIdx].getType(), op, whereVal) : true;
                if (!match && rows[r].get(targetIdx) == cleanVal) {
                    string constraint = columns[targetIdx].getIsPrimaryKey() ? "PRIMARY KEY" : "UNIQUE";
                    errorMsg = "Duplicate value '" + cleanVal + "' violates " + constraint + " constraint.";
                    return false;
                }
            }
        }

        for (int r = 0; r < (int)rows.size(); r++) {
            bool match = (whereIdx >= 0) ? matchCondition(rows[r].get(whereIdx), columns[whereIdx].getType(), op, whereVal) : true;
            if (match) {
                rows[r].set(targetIdx, cleanVal);
                updatedCount++;
            }
        }
        return true;
    }

    bool deleteRows(const string& whereCol, const string& op, const string& whereVal, int& deletedCount) {
        deletedCount = 0;
        int whereIdx = -1;

        if (!whereCol.empty()) {
            whereIdx = getColumnIndex(whereCol);
            if (whereIdx == -1) {
                cout << "Error: WHERE column '" << whereCol << "' not found.\n";
                return false;
            }
        }

        vector<Row> remainingRows;
        for (int r = 0; r < (int)rows.size(); r++) {
            bool match = (whereIdx >= 0) ? matchCondition(rows[r].get(whereIdx), columns[whereIdx].getType(), op, whereVal) : true;
            if (match) {
                deletedCount++;
            } else {
                remainingRows.push_back(rows[r]);
            }
        }

        rows = remainingRows;
        return true;
    }

    void displaySchema() const {
        cout << "Table: " << name << " (" << rows.size() << " rows)\n";
        vector<int> widths = {20, 10, 11, 6, 8};

        printBorder(widths);
        cout << "|";
        printCell("Column Name", widths[0]);
        printCell("Type", widths[1]);
        printCell("Primary Key", widths[2]);
        printCell("Unique", widths[3]);
        printCell("Not Null", widths[4]);
        cout << "\n";
        printBorder(widths);

        for (const Column& col : columns) {
            cout << "|";
            printCell(col.getName(), widths[0]);
            printCell(col.getType(), widths[1]);
            printCell(col.getIsPrimaryKey() ? "YES" : "NO", widths[2]);
            printCell(col.getIsUnique() ? "YES" : "NO", widths[3]);
            printCell(col.getIsNotNull() ? "YES" : "NO", widths[4]);
            cout << "\n";
        }
        printBorder(widths);
    }

    bool saveToFile(const string& filename) const {
        ofstream out(filename);
        if (!out.is_open()) return false;

        out << "TABLE:" << name << "\n";
        out << "COLUMNS:" << columns.size() << "\n";
        for (const Column& col : columns) {
            out << col.getName() << ":" << col.getType() << ":"
                << (col.getIsPrimaryKey() ? 1 : 0) << ":"
                << (col.getIsUnique() ? 1 : 0) << ":"
                << (col.getIsNotNull() ? 1 : 0) << "\n";
        }

        out << "ROWS:" << rows.size() << "\n";
        for (const Row& row : rows) {
            for (int c = 0; c < row.getCount(); c++) {
                out << row.get(c);
                if (c < row.getCount() - 1) out << "\t";
            }
            out << "\n";
        }
        return true;
    }

    bool loadFromFile(const string& filename, string& errorMsg) {
        ifstream in(filename);
        if (!in.is_open()) {
            errorMsg = "Cannot open " + filename;
            return false;
        }

        columns.clear();
        rows.clear();

        string line;
        if (!getline(in, line)) return false;
        if (line.rfind("TABLE:", 0) == 0) name = line.substr(6);

        if (!getline(in, line)) return false;
        if (line.rfind("COLUMNS:", 0) == 0) {
            int expectedCols = Utils::stringToInt(line.substr(8));
            for (int i = 0; i < expectedCols; i++) {
                if (!getline(in, line)) return false;
                vector<string> parts = Utils::split(line, ':');
                if (parts.size() >= 3) {
                    bool pk = (parts[2] == "1");
                    bool uq = (parts.size() >= 4 && parts[3] == "1");
                    bool nn = (parts.size() >= 5 && parts[4] == "1");
                    addColumn(Column(parts[0], parts[1], pk, uq, nn));
                }
            }
        }

        if (!getline(in, line)) return false;
        if (line.rfind("ROWS:", 0) == 0) {
            int expectedRows = Utils::stringToInt(line.substr(5));
            for (int r = 0; r < expectedRows; r++) {
                if (!getline(in, line)) break;
                vector<string> values = Utils::split(line, '\t');
                if ((int)values.size() == (int)columns.size()) {
                    rows.push_back(Row(values));
                }
            }
        }
        return true;
    }
};

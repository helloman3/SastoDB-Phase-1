#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cctype>
#include <cstdio>

using namespace std;

// ============================================================================
// 1. DATA STRUCTURES
// ============================================================================
struct Column {
    string name;
    string type = "TEXT";       // INT, FLOAT, or TEXT
    bool isPK = false;          // PRIMARY KEY implies Unique & Not Null
    bool isUnique = false;
    bool isNotNull = false;

    bool isInt() const   { return type == "INT" || type == "INTEGER"; }
    bool isFloat() const { return type == "FLOAT" || type == "DOUBLE"; }
    bool isText() const  { return type == "TEXT" || type == "VARCHAR" || type == "STRING"; }
};

using Row = vector<string>;

// ============================================================================
// 2. STRING & PARSING UTILITIES
// ============================================================================
namespace Utils {
    inline string trim(const string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    inline string toUpper(string s) {
        for (char& c : s) c = (char)toupper(c);
        return s;
    }

    inline string stripQuotes(string s) {
        s = trim(s);
        if (s.length() >= 2 && ((s.front() == '\'' && s.back() == '\'') || 
                               (s.front() == '"'  && s.back() == '"'))) {
            return s.substr(1, s.length() - 2);
        }
        return s;
    }

    inline bool isNumeric(string s) {
        s = trim(s);
        if (s.empty()) return false;
        size_t i = (s[0] == '-' || s[0] == '+') ? 1 : 0;
        if (i == s.length()) return false;
        for (; i < s.length(); i++) {
            if (!isdigit(s[i])) return false;
        }
        return true;
    }

    inline vector<string> split(const string& text, char delimiter) {
        vector<string> parts;
        string current = "";
        bool inQuotes = false;
        for (char c : text) {
            if (c == '\'' || c == '"') inQuotes = !inQuotes;
            if (c == delimiter && !inQuotes) {
                parts.push_back(trim(current));
                current.clear();
            } else {
                current += c;
            }
        }
        parts.push_back(trim(current));
        return parts;
    }

    inline bool getParenContent(const string& s, string& inside) {
        size_t openP = s.find('(');
        size_t closeP = s.rfind(')');
        if (openP == string::npos || closeP == string::npos || closeP <= openP) return false;
        inside = s.substr(openP + 1, closeP - openP - 1);
        return true;
    }
}

// ============================================================================
// 3. TABLE ENGINE
// ============================================================================
class Table {
private:
    string name;
    vector<Column> columns;
    vector<Row> rows;

    bool matchCondition(const string& val, const string& type, const string& op, const string& target) const {
        string cleanTarget = Utils::stripQuotes(target);
        if (type == "INT" || type == "INTEGER") {
            int a = 0, b = 0;
            try { a = stoi(Utils::trim(val)); b = stoi(cleanTarget); } catch (...) { return false; }
            if (op == "=" || op == "==") return a == b;
            if (op == "!=") return a != b;
            if (op == "<")  return a < b;
            if (op == ">")  return a > b;
            if (op == "<=") return a <= b;
            if (op == ">=") return a >= b;
        } else if (type == "FLOAT" || type == "DOUBLE") {
            double a = 0, b = 0;
            try { a = stod(Utils::trim(val)); b = stod(cleanTarget); } catch (...) { return false; }
            if (op == "=" || op == "==") return a == b;
            if (op == "!=") return a != b;
            if (op == "<")  return a < b;
            if (op == ">")  return a > b;
            if (op == "<=") return a <= b;
            if (op == ">=") return a >= b;
        } else {
            if (op == "=" || op == "==") return val == cleanTarget;
            if (op == "!=") return val != cleanTarget;
            if (op == "<")  return val < cleanTarget;
            if (op == ">")  return val > cleanTarget;
        }
        return false;
    }

    void printBorder(const vector<int>& w) const {
        cout << "+";
        for (int width : w) cout << string(width + 2, '-') << "+";
        cout << "\n";
    }

public:
    Table(string tName = "") : name(tName) {}

    string getName() const { return name; }
    int getColumnCount() const { return columns.size(); }
    int getRowCount() const { return rows.size(); }
    void addColumn(const Column& c) { columns.push_back(c); }

    int getColumnIndex(const string& colName) const {
        for (int i = 0; i < (int)columns.size(); i++) {
            if (Utils::toUpper(columns[i].name) == Utils::toUpper(colName)) return i;
        }
        return -1;
    }

    bool insertRow(const Row& row, string& err) {
        if (row.size() != columns.size()) {
            err = "Column count mismatch: Expected " + to_string(columns.size()) +
                  " values, received " + to_string(row.size());
            return false;
        }
        for (size_t i = 0; i < columns.size(); i++) {
            string val = row[i];
            bool isNull = (val.empty() || Utils::toUpper(val) == "NULL");

            if (columns[i].isNotNull && isNull) {
                err = "Column '" + columns[i].name + "' cannot be NULL.";
                return false;
            }
            if (!isNull && columns[i].isInt() && !Utils::isNumeric(val)) {
                err = "Invalid integer value '" + val + "' for column '" + columns[i].name + "'.";
                return false;
            }
            if (!isNull && (columns[i].isPK || columns[i].isUnique)) {
                for (const auto& r : rows) {
                    if (r[i] == val) {
                        string cType = columns[i].isPK ? "PRIMARY KEY" : "UNIQUE";
                        err = "Duplicate value '" + val + "' violates " + cType + " constraint on column '" + columns[i].name + "'.";
                        return false;
                    }
                }
            }
        }
        rows.push_back(row);
        return true;
    }

    bool selectRows(const string& whereCol, const string& op, const string& whereVal, int& count) const {
        int wIdx = -1;
        if (!whereCol.empty()) {
            wIdx = getColumnIndex(whereCol);
            if (wIdx == -1) { cout << "Error: WHERE column '" << whereCol << "' not found.\n"; return false; }
        }
        if (columns.empty()) { cout << "Table '" << name << "' has no columns.\n"; return true; }

        vector<int> widths(columns.size());
        for (size_t i = 0; i < columns.size(); i++) widths[i] = columns[i].name.length();

        vector<int> matched;
        for (size_t r = 0; r < rows.size(); r++) {
            if (wIdx < 0 || matchCondition(rows[r][wIdx], columns[wIdx].type, op, whereVal)) {
                matched.push_back(r);
                for (size_t i = 0; i < columns.size(); i++) {
                    widths[i] = max(widths[i], (int)rows[r][i].length());
                }
            }
        }
        count = matched.size();

        printBorder(widths);
        cout << "|";
        for (size_t i = 0; i < columns.size(); i++) cout << " " << left << setw(widths[i]) << columns[i].name << " |";
        cout << "\n";
        printBorder(widths);

        for (int rIdx : matched) {
            cout << "|";
            for (size_t i = 0; i < columns.size(); i++) cout << " " << left << setw(widths[i]) << rows[rIdx][i] << " |";
            cout << "\n";
        }
        printBorder(widths);
        return true;
    }

    bool updateRows(const string& targetCol, const string& newVal,
                    const string& whereCol, const string& op, const string& whereVal,
                    int& updated, string& err) {
        updated = 0;
        int targetIdx = getColumnIndex(targetCol);
        if (targetIdx == -1) { err = "Target column '" + targetCol + "' not found."; return false; }

        int wIdx = -1;
        if (!whereCol.empty()) {
            wIdx = getColumnIndex(whereCol);
            if (wIdx == -1) { err = "WHERE column '" + whereCol + "' not found."; return false; }
        }

        string cleanVal = columns[targetIdx].isText() ? Utils::stripQuotes(newVal) : Utils::trim(newVal);
        bool isNull = (cleanVal.empty() || Utils::toUpper(cleanVal) == "NULL");

        if (columns[targetIdx].isNotNull && isNull) { err = "Column '" + columns[targetIdx].name + "' cannot be NULL."; return false; }
        if (!isNull && columns[targetIdx].isInt() && !Utils::isNumeric(cleanVal)) {
            err = "Invalid integer value '" + cleanVal + "' for column '" + columns[targetIdx].name + "'.";
            return false;
        }

        vector<int> matched;
        for (size_t r = 0; r < rows.size(); r++) {
            if (wIdx < 0 || matchCondition(rows[r][wIdx], columns[wIdx].type, op, whereVal)) matched.push_back(r);
        }

        if (!isNull && (columns[targetIdx].isPK || columns[targetIdx].isUnique)) {
            string cType = columns[targetIdx].isPK ? "PRIMARY KEY" : "UNIQUE";
            if (matched.size() > 1) {
                err = "Cannot update multiple rows to identical value under " + cType + " constraint.";
                return false;
            }
            for (size_t r = 0; r < rows.size(); r++) {
                bool isMatch = false;
                for (int m : matched) if (m == (int)r) isMatch = true;
                if (!isMatch && rows[r][targetIdx] == cleanVal) {
                    err = "Duplicate value '" + cleanVal + "' violates " + cType + " constraint.";
                    return false;
                }
            }
        }

        for (int r : matched) {
            rows[r][targetIdx] = cleanVal;
            updated++;
        }
        return true;
    }

    bool deleteRows(const string& whereCol, const string& op, const string& whereVal, int& deleted) {
        deleted = 0;
        int wIdx = -1;
        if (!whereCol.empty()) {
            wIdx = getColumnIndex(whereCol);
            if (wIdx == -1) { cout << "Error: WHERE column '" << whereCol << "' not found.\n"; return false; }
        }
        vector<Row> kept;
        for (const auto& r : rows) {
            if (wIdx >= 0 && matchCondition(r[wIdx], columns[wIdx].type, op, whereVal)) {
                deleted++;
            } else if (wIdx < 0) {
                deleted++;
            } else {
                kept.push_back(r);
            }
        }
        rows = kept;
        return true;
    }

    void displaySchema() const {
        cout << "Table: " << name << " (" << rows.size() << " rows)\n";
        vector<int> w = {20, 10, 11, 6, 8};
        printBorder(w);
        cout << "| " << left << setw(w[0]) << "Column Name" << " | " << setw(w[1]) << "Type" 
             << " | " << setw(w[2]) << "Primary Key" << " | " << setw(w[3]) << "Unique" 
             << " | " << setw(w[4]) << "Not Null" << " |\n";
        printBorder(w);
        for (const auto& col : columns) {
            cout << "| " << left << setw(w[0]) << col.name 
                 << " | " << setw(w[1]) << col.type 
                 << " | " << setw(w[2]) << (col.isPK ? "YES" : "NO") 
                 << " | " << setw(w[3]) << (col.isUnique ? "YES" : "NO") 
                 << " | " << setw(w[4]) << (col.isNotNull ? "YES" : "NO") << " |\n";
        }
        printBorder(w);
    }

    bool saveToFile(const string& filename) const {
        ofstream out(filename);
        if (!out.is_open()) return false;
        out << "TABLE:" << name << "\nCOLUMNS:" << columns.size() << "\n";
        for (const auto& c : columns) {
            out << c.name << ":" << c.type << ":" << (c.isPK ? 1 : 0) << ":" 
                << (c.isUnique ? 1 : 0) << ":" << (c.isNotNull ? 1 : 0) << "\n";
        }
        out << "ROWS:" << rows.size() << "\n";
        for (const auto& r : rows) {
            for (size_t c = 0; c < r.size(); c++) {
                out << r[c] << (c + 1 < r.size() ? "\t" : "");
            }
            out << "\n";
        }
        return true;
    }

    bool loadFromFile(const string& filename) {
        ifstream in(filename);
        if (!in.is_open()) return false;
        columns.clear(); rows.clear();
        string line;
        if (!getline(in, line) || line.rfind("TABLE:", 0) != 0) return false;
        name = line.substr(6);
        if (!getline(in, line) || line.rfind("COLUMNS:", 0) != 0) return false;
        int numCols = stoi(line.substr(8));
        for (int i = 0; i < numCols; i++) {
            if (!getline(in, line)) return false;
            auto p = Utils::split(line, ':');
            if (p.size() >= 5) {
                addColumn({p[0], p[1], p[2] == "1", p[3] == "1", p[4] == "1"});
            }
        }
        if (!getline(in, line) || line.rfind("ROWS:", 0) != 0) return false;
        int numRows = stoi(line.substr(5));
        for (int r = 0; r < numRows; r++) {
            if (!getline(in, line)) break;
            auto vals = Utils::split(line, '\t');
            if (vals.size() == columns.size()) rows.push_back(vals);
        }
        return true;
    }
};

// ============================================================================
// 4. DATABASE CATALOG & PERSISTENCE
// ============================================================================
class Database {
private:
    string dbName;
    vector<Table> tables;
    string getFile(const string& tName) const { return tName + "_data.txt"; }

public:
    Database(string name = "student_db") : dbName(name) { loadAll(); }
    ~Database() { saveAll(); }

    Table* getTable(const string& name) {
        for (auto& t : tables) {
            if (Utils::toUpper(t.getName()) == Utils::toUpper(name)) return &t;
        }
        return nullptr;
    }

    bool createTable(string name, const vector<Column>& cols, string& err) {
        name = Utils::trim(name);
        if (name.empty()) { err = "Table name cannot be empty."; return false; }
        if (getTable(name)) { err = "Table '" + name + "' already exists."; return false; }
        Table newTable(name);
        for (const auto& c : cols) newTable.addColumn(c);
        tables.push_back(newTable);
        saveAll();
        return true;
    }

    bool dropTable(const string& name, string& err) {
        for (auto it = tables.begin(); it != tables.end(); ++it) {
            if (Utils::toUpper(it->getName()) == Utils::toUpper(name)) {
                remove(getFile(it->getName()).c_str());
                tables.erase(it);
                saveAll();
                return true;
            }
        }
        err = "Table '" + name + "' does not exist.";
        return false;
    }

    void listTables() const {
        if (tables.empty()) { cout << "No tables found in database '" << dbName << "'.\n"; return; }
        cout << "+----------------------+------------+------------+\n"
             << "| Table Name           | Columns    | Rows       |\n"
             << "+----------------------+------------+------------+\n";
        for (const auto& t : tables) {
            cout << "| " << left << setw(20) << t.getName() << " | " 
                 << setw(10) << t.getColumnCount() << " | " 
                 << setw(10) << t.getRowCount() << " |\n";
        }
        cout << "+----------------------+------------+------------+\n"
             << tables.size() << " table(s) in total.\n";
    }

    void saveAll() {
        ofstream cat("db_catalog.txt");
        if (!cat.is_open()) return;
        for (const auto& t : tables) {
            cat << t.getName() << "\n";
            t.saveToFile(getFile(t.getName()));
        }
    }

    void loadAll() {
        ifstream cat("db_catalog.txt");
        if (!cat.is_open()) return;
        tables.clear();
        string tName;
        while (getline(cat, tName)) {
            tName = Utils::trim(tName);
            if (tName.empty()) continue;
            Table t(tName);
            if (t.loadFromFile(getFile(tName))) tables.push_back(t);
        }
    }
};

// ============================================================================
// 5. QUERY PARSER & DIRECT DISPATCHER
// ============================================================================
bool parseWhere(string wherePart, string& col, string& op, string& val) {
    wherePart = Utils::trim(wherePart);
    const char* ops2[] = {"!=", "<=", ">=", "=="};
    for (const char* o : ops2) {
        size_t p = wherePart.find(o);
        if (p != string::npos) {
            col = Utils::trim(wherePart.substr(0, p));
            op = o;
            val = Utils::stripQuotes(Utils::trim(wherePart.substr(p + 2)));
            return !col.empty() && !val.empty();
        }
    }
    const char* ops1[] = {"=", "<", ">"};
    for (const char* o : ops1) {
        size_t p = wherePart.find(o);
        if (p != string::npos) {
            col = Utils::trim(wherePart.substr(0, p));
            op = o;
            val = Utils::stripQuotes(Utils::trim(wherePart.substr(p + 1)));
            return !col.empty() && !val.empty();
        }
    }
    return false;
}

void executeSQL(Database& db, string sql) {
    sql = Utils::trim(sql);
    if (sql.empty()) return;
    if (sql.back() == ';') sql = Utils::trim(sql.substr(0, sql.length() - 1));

    string upper = Utils::toUpper(sql);

    // HELP
    if (upper == ".HELP" || upper == "HELP") {
        cout << "Available SQL Statements:\n"
             << "  CREATE TABLE table_name (col1 TYPE [PRIMARY KEY | UNIQUE | NOT NULL], ...);\n"
             << "  DROP TABLE table_name;\n"
             << "  INSERT INTO table_name VALUES (val1, val2, ...);\n"
             << "  SELECT * FROM table_name [WHERE col = val];\n"
             << "  UPDATE table_name SET col = val [WHERE col = val];\n"
             << "  DELETE FROM table_name [WHERE col = val];\n"
             << "  VIEW TABLES; or VIEW table_name;\n"
             << "  .exit or exit;\n";
        return;
    }

    // VIEW / SHOW
    if (upper.rfind("VIEW", 0) == 0 || upper.rfind("SHOW", 0) == 0) {
        size_t sp = sql.find(' ');
        string target = (sp != string::npos) ? Utils::trim(sql.substr(sp + 1)) : "TABLES";
        if (target.empty() || Utils::toUpper(target) == "TABLES") {
            db.listTables();
        } else {
            Table* t = db.getTable(target);
            if (!t) cout << "Error: Table '" << target << "' not found.\n";
            else t->displaySchema();
        }
        return;
    }

    // CREATE TABLE
    if (upper.rfind("CREATE TABLE", 0) == 0) {
        size_t pPos = sql.find('(');
        if (pPos == string::npos) { cout << "Error: Missing '(' in CREATE TABLE.\n"; return; }
        string tableName = Utils::trim(sql.substr(12, pPos - 12));
        if (tableName.empty()) { cout << "Error: Table name cannot be empty in CREATE TABLE.\n"; return; }
        string inside;
        if (!Utils::getParenContent(sql, inside)) { cout << "Error: Invalid parentheses in CREATE TABLE.\n"; return; }

        auto colDefs = Utils::split(inside, ',');
        vector<Column> cols;
        int pkCount = 0;
        for (const auto& def : colDefs) {
            auto words = Utils::split(def, ' ');
            if (words.empty() || words[0].empty()) continue;
            Column col;
            col.name = words[0];
            col.type = (words.size() > 1) ? Utils::toUpper(words[1]) : "TEXT";
            for (size_t w = 1; w < words.size(); w++) {
                string uw = Utils::toUpper(words[w]);
                if (uw == "PRIMARY" && w + 1 < words.size() && Utils::toUpper(words[w + 1]) == "KEY") {
                    col.isPK = col.isUnique = col.isNotNull = true; pkCount++; w++;
                } else if (uw == "UNIQUE") {
                    col.isUnique = true;
                } else if (uw == "NOT" && w + 1 < words.size() && Utils::toUpper(words[w + 1]) == "NULL") {
                    col.isNotNull = true; w++;
                }
            }
            cols.push_back(col);
        }
        if (pkCount > 1) { cout << "Error: Table cannot have multiple PRIMARY KEY columns.\n"; return; }
        string err;
        if (db.createTable(tableName, cols, err)) cout << "Table '" << tableName << "' created successfully.\n";
        else cout << "Error: " << err << "\n";
        return;
    }

    // DROP TABLE
    if (upper.rfind("DROP TABLE", 0) == 0) {
        string tName = Utils::trim(sql.substr(10));
        string err;
        if (db.dropTable(tName, err)) cout << "Table '" << tName << "' dropped successfully.\n";
        else cout << "Error: " << err << "\n";
        return;
    }

    // INSERT INTO
    if (upper.rfind("INSERT INTO", 0) == 0) {
        size_t vPos = upper.find("VALUES");
        if (vPos == string::npos) { cout << "Error: Missing 'VALUES' in INSERT.\n"; return; }
        string tName = Utils::trim(sql.substr(11, vPos - 11));
        Table* t = db.getTable(tName);
        if (!t) { cout << "Error: Table '" << tName << "' not found.\n"; return; }
        string inside;
        if (!Utils::getParenContent(sql.substr(vPos), inside)) { cout << "Error: Missing '()' after VALUES.\n"; return; }

        auto tokens = Utils::split(inside, ',');
        Row r;
        for (const auto& tok : tokens) r.push_back(Utils::stripQuotes(tok));
        string err;
        if (t->insertRow(r, err)) {
            db.saveAll();
            cout << "1 row inserted successfully.\n";
        } else {
            cout << "Error: " << err << "\n";
        }
        return;
    }

    // SELECT
    if (upper.rfind("SELECT", 0) == 0) {
        size_t fPos = upper.find(" FROM ");
        if (fPos == string::npos) { cout << "Error: Missing 'FROM' in SELECT.\n"; return; }
        string rest = Utils::trim(sql.substr(fPos + 6));
        string uRest = Utils::toUpper(rest);
        size_t wPos = uRest.find(" WHERE ");

        string tName = rest, wCol = "", op = "", wVal = "";
        if (wPos != string::npos) {
            tName = Utils::trim(rest.substr(0, wPos));
            if (!parseWhere(rest.substr(wPos + 7), wCol, op, wVal)) {
                cout << "Error: Invalid WHERE clause syntax in SELECT.\n"; return;
            }
        }
        Table* t = db.getTable(tName);
        if (!t) { cout << "Error: Table '" << tName << "' not found.\n"; return; }
        int count = 0;
        t->selectRows(wCol, op, wVal, count);
        cout << count << " row(s) returned.\n";
        return;
    }

    // UPDATE
    if (upper.rfind("UPDATE", 0) == 0) {
        size_t sPos = upper.find(" SET ");
        if (sPos == string::npos) { cout << "Error: Missing 'SET' in UPDATE.\n"; return; }
        string tName = Utils::trim(sql.substr(6, sPos - 6));
        Table* t = db.getTable(tName);
        if (!t) { cout << "Error: Table '" << tName << "' not found.\n"; return; }

        string rest = Utils::trim(sql.substr(sPos + 5));
        size_t wPos = Utils::toUpper(rest).find(" WHERE ");
        string setPart = rest, wCol = "", op = "", wVal = "";
        if (wPos != string::npos) {
            setPart = Utils::trim(rest.substr(0, wPos));
            if (!parseWhere(rest.substr(wPos + 7), wCol, op, wVal)) {
                cout << "Error: Invalid WHERE clause syntax in UPDATE.\n"; return;
            }
        }
        size_t eqPos = setPart.find('=');
        if (eqPos == string::npos) { cout << "Error: Invalid SET expression: expected 'col = val'.\n"; return; }
        string setCol = Utils::trim(setPart.substr(0, eqPos));
        string setVal = Utils::trim(setPart.substr(eqPos + 1));

        int updated = 0;
        string err;
        if (t->updateRows(setCol, setVal, wCol, op, wVal, updated, err)) {
            db.saveAll();
            cout << updated << " row(s) updated successfully.\n";
        } else {
            cout << "Error: " << err << "\n";
        }
        return;
    }

    // DELETE FROM
    if (upper.rfind("DELETE FROM", 0) == 0) {
        string rest = Utils::trim(sql.substr(11));
        size_t wPos = Utils::toUpper(rest).find(" WHERE ");
        string tName = rest, wCol = "", op = "", wVal = "";
        if (wPos != string::npos) {
            tName = Utils::trim(rest.substr(0, wPos));
            if (!parseWhere(rest.substr(wPos + 7), wCol, op, wVal)) {
                cout << "Error: Invalid WHERE clause syntax in DELETE.\n"; return;
            }
        }
        Table* t = db.getTable(tName);
        if (!t) { cout << "Error: Table '" << tName << "' not found.\n"; return; }
        int deleted = 0;
        if (t->deleteRows(wCol, op, wVal, deleted)) {
            db.saveAll();
            cout << deleted << " row(s) deleted successfully.\n";
        }
        return;
    }

    cout << "Error: Unrecognized command. Type '.help' for help.\n";
}

// ============================================================================
// 6. MAIN REPL SHELL
// ============================================================================
int main() {
    Database db("student_db");

    cout << "  ###    #    ###  #####  ###    ####   ####  \n"
         << " #      # #  #       #   #   #   #   #  #   # \n"
         << "  ###  #####  ###    #   #   #   #   #  ####  \n"
         << "     # #   #     #   #   #   #   #   #  #   # \n"
         << " ###   #   #  ###    #    ###    ####   ####  \n"
         << "--------------------------------------------------\n"
         << "       Embedded SQL RDBMS | Single-File Engine    \n"
         << "              Developed by Group H                \n"
         << "--------------------------------------------------\n"
         << "Type '.help;' or .help for help. End SQL statements with ';'.\n"
         << "Type '.exit' or 'exit;' to quit.\n\n";

    string fullQuery = "";
    while (true) {
        cout << (fullQuery.empty() ? "db> " : "...> ");
        string line;
        if (!getline(cin, line)) break;

        // Strip comments and trim
        size_t cPos = line.find("--");
        if (cPos != string::npos) line = line.substr(0, cPos);
        string trimmed = Utils::trim(line);
        if (trimmed.empty()) continue;

        // Ignore leading script step numbers (e.g., "1. Create a table...")
        if (fullQuery.empty() && isdigit(trimmed[0])) continue;

        string upper = Utils::toUpper(trimmed);
        if (fullQuery.empty() && (upper == ".EXIT" || upper == ".EXIT;" || upper == "EXIT;" || 
                                 upper == ".QUIT" || upper == "QUIT;")) {
            cout << "Persisting database and exiting... Goodbye!\n";
            break;
        }

        if (fullQuery.empty() && trimmed[0] == '.') {
            fullQuery = trimmed;
        } else {
            if (!fullQuery.empty()) fullQuery += " ";
            fullQuery += trimmed;
            if (fullQuery.back() != ';') continue;
        }

        executeSQL(db, fullQuery);
        fullQuery = "";
    }
    return 0;
}

#include <iostream>
#include <string>
#include <vector>
#include <memory>

#include "include/Utils.cpp"
#include "include/Column.cpp"
#include "include/Row.cpp"
#include "include/Table.cpp"
#include "include/Database.cpp"
#include "include/Command.cpp"

using namespace std;

bool getParenContent(const string& str, string& inside) {
    size_t openP = str.find('(');
    size_t closeP = str.rfind(')');
    if (openP == string::npos || closeP == string::npos || closeP <= openP) {
        return false;
    }
    inside = str.substr(openP + 1, closeP - openP - 1);
    return true;
}

string stripComment(const string& line) {
    size_t p = line.find("--");
    if (p != string::npos) {
        return line.substr(0, p);
    }
    return line;
}

bool parseWhere(string wherePart, string& col, string& op, string& val) {
    wherePart = Utils::trim(wherePart);

    const char* twoCharOps[] = {"!=", "<=", ">=", "=="};
    for (int i = 0; i < 4; i++) {
        size_t p = wherePart.find(twoCharOps[i]);
        if (p != string::npos) {
            col = Utils::trim(wherePart.substr(0, p));
            op = twoCharOps[i];
            string rawVal = Utils::trim(wherePart.substr(p + 2));
            if (rawVal.empty()) return false;
            val = Utils::stripQuotes(rawVal);
            return !col.empty();
        }
    }

    const char* oneCharOps[] = {"=", "<", ">"};
    for (int i = 0; i < 3; i++) {
        size_t p = wherePart.find(oneCharOps[i]);
        if (p != string::npos) {
            col = Utils::trim(wherePart.substr(0, p));
            op = oneCharOps[i];
            string rawVal = Utils::trim(wherePart.substr(p + 1));
            if (rawVal.empty()) return false;
            val = Utils::stripQuotes(rawVal);
            return !col.empty();
        }
    }

    return false;
}

unique_ptr<Command> parseQuery(string query, string& errorMsg) {
    query = Utils::trim(query);
    if (query.empty()) {
        errorMsg = "Empty input.";
        return nullptr;
    }

    if (query.back() == ';') {
        query = Utils::trim(query.substr(0, query.length() - 1));
    }

    string upper = Utils::toUpper(query);

    if (upper == ".HELP" || upper == "HELP") {
        return make_unique<HelpCommand>();
    }

    if (upper.rfind("VIEW", 0) == 0 || upper.rfind("SHOW", 0) == 0) {
        if (upper == "VIEW TABLES" || upper == "SHOW TABLES" || upper == "VIEW" || upper == "SHOW") {
            return make_unique<ViewCommand>("TABLES");
        }
        if (upper.rfind("VIEW ", 0) == 0) return make_unique<ViewCommand>(Utils::trim(query.substr(5)));
        if (upper.rfind("SHOW ", 0) == 0) return make_unique<ViewCommand>(Utils::trim(query.substr(5)));
        return make_unique<ViewCommand>("TABLES");
    }

    if (upper.rfind("CREATE TABLE", 0) == 0) {
        size_t pPos = query.find('(');
        if (pPos == string::npos) {
            errorMsg = "Missing '(' in CREATE TABLE.";
            return nullptr;
        }

        string tableName = Utils::trim(query.substr(12, pPos - 12));
        if (tableName.empty()) {
            errorMsg = "Table name cannot be empty in CREATE TABLE.";
            return nullptr;
        }

        string inside;
        if (!getParenContent(query, inside)) {
            errorMsg = "Invalid parentheses in CREATE TABLE.";
            return nullptr;
        }

        vector<string> colDefinitions = Utils::split(inside, ',');
        if (colDefinitions.empty()) {
            errorMsg = "Table must have at least one column.";
            return nullptr;
        }

        vector<Column> columns;
        int primaryKeyCount = 0;

        for (int i = 0; i < (int)colDefinitions.size(); i++) {
            vector<string> words = Utils::split(colDefinitions[i], ' ');
            if (words.empty() || words[0].empty()) {
                errorMsg = "Invalid column definition: empty column name.";
                return nullptr;
            }

            string colName = words[0];
            string colType = (words.size() > 1) ? Utils::toUpper(words[1]) : "TEXT";
            bool isPk = false;
            bool isUq = false;
            bool isNn = false;

            for (int w = 1; w < (int)words.size(); w++) {
                string wordUpper = Utils::toUpper(words[w]);
                if (wordUpper == "PRIMARY") {
                    if (w + 1 < (int)words.size() && Utils::toUpper(words[w + 1]) == "KEY") {
                        w++;
                    }
                    isPk = true;
                    isUq = true;
                    isNn = true;
                    primaryKeyCount++;
                } else if (wordUpper == "UNIQUE") {
                    isUq = true;
                } else if (wordUpper == "NOT" && w + 1 < (int)words.size() && Utils::toUpper(words[w + 1]) == "NULL") {
                    isNn = true;
                    w++;
                }
            }

            columns.push_back(Column(colName, colType, isPk, isUq, isNn));
        }

        if (primaryKeyCount > 1) {
            errorMsg = "Table cannot have multiple PRIMARY KEY columns.";
            return nullptr;
        }

        return make_unique<CreateTableCommand>(tableName, columns);
    }

    if (upper.rfind("DROP TABLE", 0) == 0) {
        string tableName = Utils::trim(query.substr(10));
        if (tableName.empty()) {
            errorMsg = "Missing table name in DROP TABLE.";
            return nullptr;
        }
        return make_unique<DropCommand>(tableName);
    }

    if (upper.rfind("INSERT INTO", 0) == 0) {
        size_t vPos = upper.find("VALUES");
        if (vPos == string::npos) {
            errorMsg = "Missing 'VALUES' in INSERT.";
            return nullptr;
        }

        string tableName = Utils::trim(query.substr(11, vPos - 11));
        if (tableName.empty()) {
            errorMsg = "Missing table name in INSERT INTO.";
            return nullptr;
        }

        string inside;
        if (!getParenContent(query.substr(vPos), inside)) {
            errorMsg = "Missing '()' after VALUES.";
            return nullptr;
        }

        vector<string> valTokens = Utils::split(inside, ',');
        Row row;
        for (int i = 0; i < (int)valTokens.size(); i++) {
            row.add(Utils::stripQuotes(valTokens[i]));
        }

        return make_unique<InsertCommand>(tableName, row);
    }

    if (upper.rfind("SELECT", 0) == 0) {
        size_t fromPos = upper.find(" FROM ");
        if (fromPos == string::npos) {
            errorMsg = "Missing 'FROM' in SELECT.";
            return nullptr;
        }

        string rest = Utils::trim(query.substr(fromPos + 6));
        string upperRest = Utils::toUpper(rest);
        size_t wherePos = upperRest.find(" WHERE ");

        string tableName = rest;
        string whereCol = "";
        string op = "";
        string whereVal = "";

        if (wherePos != string::npos) {
            tableName = Utils::trim(rest.substr(0, wherePos));
            string wherePart = rest.substr(wherePos + 7);
            if (!parseWhere(wherePart, whereCol, op, whereVal)) {
                errorMsg = "Invalid WHERE clause syntax in SELECT.";
                return nullptr;
            }
        }

        if (tableName.empty()) {
            errorMsg = "Missing table name in SELECT.";
            return nullptr;
        }

        return make_unique<SelectCommand>(tableName, whereCol, op, whereVal);
    }

    if (upper.rfind("UPDATE", 0) == 0) {
        size_t setPos = upper.find(" SET ");
        if (setPos == string::npos) {
            errorMsg = "Missing 'SET' in UPDATE.";
            return nullptr;
        }

        string tableName = Utils::trim(query.substr(6, setPos - 6));
        if (tableName.empty()) {
            errorMsg = "Missing table name in UPDATE.";
            return nullptr;
        }

        string rest = Utils::trim(query.substr(setPos + 5));
        string upperRest = Utils::toUpper(rest);
        size_t wherePos = upperRest.find(" WHERE ");

        string setPart = rest;
        string whereCol = "";
        string op = "";
        string whereVal = "";

        if (wherePos != string::npos) {
            setPart = Utils::trim(rest.substr(0, wherePos));
            string wherePart = rest.substr(wherePos + 7);
            if (!parseWhere(wherePart, whereCol, op, whereVal)) {
                errorMsg = "Invalid WHERE clause syntax in UPDATE.";
                return nullptr;
            }
        }

        size_t eqPos = setPart.find('=');
        if (eqPos == string::npos) {
            errorMsg = "Invalid SET expression: expected 'col = val'.";
            return nullptr;
        }

        string setCol = Utils::trim(setPart.substr(0, eqPos));
        string setVal = Utils::stripQuotes(Utils::trim(setPart.substr(eqPos + 1)));

        if (setCol.empty()) {
            errorMsg = "Missing column name in SET expression.";
            return nullptr;
        }

        return make_unique<UpdateCommand>(tableName, setCol, setVal, whereCol, op, whereVal);
    }

    if (upper.rfind("DELETE FROM", 0) == 0) {
        string rest = Utils::trim(query.substr(11));
        string upperRest = Utils::toUpper(rest);
        size_t wherePos = upperRest.find(" WHERE ");

        string tableName = rest;
        string whereCol = "";
        string op = "";
        string whereVal = "";

        if (wherePos != string::npos) {
            tableName = Utils::trim(rest.substr(0, wherePos));
            string wherePart = rest.substr(wherePos + 7);
            if (!parseWhere(wherePart, whereCol, op, whereVal)) {
                errorMsg = "Invalid WHERE clause syntax in DELETE.";
                return nullptr;
            }
        }

        if (tableName.empty()) {
            errorMsg = "Missing table name in DELETE.";
            return nullptr;
        }

        return make_unique<DeleteCommand>(tableName, whereCol, op, whereVal);
    }

    errorMsg = "Unrecognized command. Type '.help' for help.";
    return nullptr;
}

int main() {
    Database db("student_db");

    cout << "  ###    #    ###  #####  ###    ####   ####  \n";
    cout << " #      # #  #       #   #   #   #   #  #   # \n";
    cout << "  ###  #####  ###    #   #   #   #   #  ####  \n";
    cout << "     # #   #     #   #   #   #   #   #  #   # \n";
    cout << " ###   #   #  ###    #    ###    ####   ####  \n";
    cout << "--------------------------------------------------\n";
    cout << "       Embedded SQL RDBMS | Pure C++ Engine       \n";
    cout << "              Developed by Group H                \n";
    cout << "--------------------------------------------------\n";
    cout << "Type '.help;' or .help for help. End SQL statements with ';'.\n";
    cout << "Type '.exit' or 'exit;' to quit.\n\n";

    string fullQuery = "";

    while (true) {
        if (fullQuery.empty()) {
            cout << "db> ";
        } else {
            cout << "...> ";
        }

        string line;
        if (!getline(cin, line)) break;

        string cleanLine = stripComment(line);
        string trimmed = Utils::trim(cleanLine);
        if (trimmed.empty()) continue;

        if (fullQuery.empty()) {
            if (trimmed.rfind("--", 0) == 0 || (trimmed[0] >= '0' && trimmed[0] <= '9')) {
                continue;
            }
        }

        string upper = Utils::toUpper(trimmed);
        if (fullQuery.empty() && (upper == ".EXIT" || upper == ".EXIT;" || upper == "EXIT;" || upper == ".QUIT" || upper == "QUIT;")) {
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

        string errorMsg;
        unique_ptr<Command> command = parseQuery(fullQuery, errorMsg);
        if (command) {
            command->execute(db);
        } else {
            cout << "Error: " << errorMsg << "\n";
        }

        fullQuery = "";
    }

    return 0;
}

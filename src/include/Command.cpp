#pragma once
#include <iostream>
#include <string>
#include <vector>
#include "Database.cpp"
#include "Utils.cpp"

using namespace std;

class Command {
public:
    virtual ~Command() = default;
    virtual void execute(Database& db) = 0;
};

class CreateTableCommand : public Command {
private:
    string tableName;
    vector<Column> columns;

public:
    CreateTableCommand(const string& name, const vector<Column>& cols) {
        tableName = name;
        columns = cols;
    }

    void execute(Database& db) override {
        string errorMsg;
        if (db.createTable(tableName, columns, errorMsg)) {
            cout << "Table '" << tableName << "' created successfully.\n";
        } else {
            cout << "Error: " << errorMsg << "\n";
        }
    }
};

class DropCommand : public Command {
private:
    string tableName;

public:
    DropCommand(const string& name) {
        tableName = name;
    }

    void execute(Database& db) override {
        string errorMsg;
        if (db.dropTable(tableName, errorMsg)) {
            cout << "Table '" << tableName << "' dropped successfully.\n";
        } else {
            cout << "Error: " << errorMsg << "\n";
        }
    }
};

class InsertCommand : public Command {
private:
    string tableName;
    Row row;

public:
    InsertCommand(const string& name, const Row& r) {
        tableName = name;
        row = r;
    }

    void execute(Database& db) override {
        Table* table = db.getTable(tableName);
        if (!table) {
            cout << "Error: Table '" << tableName << "' not found.\n";
            return;
        }

        string errorMsg;
        if (table->insertRow(row, errorMsg)) {
            db.saveAll();
            cout << "1 row inserted successfully.\n";
        } else {
            cout << "Error: " << errorMsg << "\n";
        }
    }
};

class SelectCommand : public Command {
private:
    string tableName;
    string whereCol;
    string op;
    string whereVal;

public:
    SelectCommand(const string& name, const string& wCol = "", const string& oper = "", const string& wVal = "") {
        tableName = name;
        whereCol = wCol;
        op = oper;
        whereVal = wVal;
    }

    void execute(Database& db) override {
        Table* table = db.getTable(tableName);
        if (!table) {
            cout << "Error: Table '" << tableName << "' not found.\n";
            return;
        }

        int count = 0;
        table->selectRows(whereCol, op, whereVal, count);
        cout << count << " row(s) returned.\n";
    }
};

class UpdateCommand : public Command {
private:
    string tableName;
    string setCol;
    string setVal;
    string whereCol;
    string op;
    string whereVal;

public:
    UpdateCommand(const string& name, const string& sCol, const string& sVal,
                  const string& wCol = "", const string& oper = "", const string& wVal = "") {
        tableName = name;
        setCol = sCol;
        setVal = sVal;
        whereCol = wCol;
        op = oper;
        whereVal = wVal;
    }

    void execute(Database& db) override {
        Table* table = db.getTable(tableName);
        if (!table) {
            cout << "Error: Table '" << tableName << "' not found.\n";
            return;
        }

        int updated = 0;
        string errorMsg;
        if (table->updateRows(setCol, setVal, whereCol, op, whereVal, updated, errorMsg)) {
            db.saveAll();
            cout << updated << " row(s) updated successfully.\n";
        } else {
            cout << "Error: " << errorMsg << "\n";
        }
    }
};

class DeleteCommand : public Command {
private:
    string tableName;
    string whereCol;
    string op;
    string whereVal;

public:
    DeleteCommand(const string& name, const string& wCol = "", const string& oper = "", const string& wVal = "") {
        tableName = name;
        whereCol = wCol;
        op = oper;
        whereVal = wVal;
    }

    void execute(Database& db) override {
        Table* table = db.getTable(tableName);
        if (!table) {
            cout << "Error: Table '" << tableName << "' not found.\n";
            return;
        }

        int deleted = 0;
        if (table->deleteRows(whereCol, op, whereVal, deleted)) {
            db.saveAll();
            cout << deleted << " row(s) deleted successfully.\n";
        }
    }
};

class ViewCommand : public Command {
private:
    string target;

public:
    ViewCommand(const string& t = "TABLES") {
        target = t;
    }

    void execute(Database& db) override {
        if (target.empty() || Utils::toUpper(target) == "TABLES") {
            db.listTables();
        } else {
            Table* table = db.getTable(target);
            if (!table) {
                cout << "Error: Table '" << target << "' not found.\n";
                return;
            }
            table->displaySchema();
        }
    }
};

class HelpCommand : public Command {
public:
    void execute(Database& db) override {
        (void)db;
        cout << "Available SQL Statements:\n"
             << "  CREATE TABLE table_name (col1 TYPE [PRIMARY KEY | UNIQUE | NOT NULL], ...);\n"
             << "  DROP TABLE table_name;\n"
             << "  INSERT INTO table_name VALUES (val1, val2, ...);\n"
             << "  SELECT * FROM table_name [WHERE col = val];\n"
             << "  UPDATE table_name SET col = val [WHERE col = val];\n"
             << "  DELETE FROM table_name [WHERE col = val];\n"
             << "  VIEW TABLES; or VIEW table_name;\n"
             << "  .exit or exit;\n";
    }
};

#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdio>
#include "Table.cpp"
#include "Utils.cpp"

using namespace std;

class Database {
private:
    string dbName;
    vector<Table> tables;

    string getFilename(const string& tableName) const {
        return tableName + "_data.txt";
    }

public:
    Database(string name = "student_db") {
        dbName = name;
        loadAll();
    }

    ~Database() {
        saveAll();
    }

    bool hasTable(const string& name) const {
        for (int i = 0; i < (int)tables.size(); i++) {
            if (Utils::toUpper(tables[i].getName()) == Utils::toUpper(name)) {
                return true;
            }
        }
        return false;
    }

    Table* getTable(const string& name) {
        for (int i = 0; i < (int)tables.size(); i++) {
            if (Utils::toUpper(tables[i].getName()) == Utils::toUpper(name)) {
                return &tables[i];
            }
        }
        return nullptr;
    }

    bool createTable(string name, const vector<Column>& cols, string& errorMsg) {
        name = Utils::trim(name);
        if (name.empty()) {
            errorMsg = "Table name cannot be empty.";
            return false;
        }

        if (hasTable(name)) {
            errorMsg = "Table '" + name + "' already exists.";
            return false;
        }

        Table newTable(name);
        for (int i = 0; i < (int)cols.size(); i++) {
            newTable.addColumn(cols[i]);
        }
        tables.push_back(newTable);

        saveAll();
        return true;
    }

    bool dropTable(const string& name, string& errorMsg) {
        int foundIndex = -1;
        for (int i = 0; i < (int)tables.size(); i++) {
            if (Utils::toUpper(tables[i].getName()) == Utils::toUpper(name)) {
                foundIndex = i;
                break;
            }
        }

        if (foundIndex == -1) {
            errorMsg = "Table '" + name + "' does not exist.";
            return false;
        }

        string file = getFilename(tables[foundIndex].getName());
        tables.erase(tables.begin() + foundIndex);
        remove(file.c_str());

        saveAll();
        return true;
    }

    void listTables() const {
        if (tables.empty()) {
            cout << "No tables found in database '" << dbName << "'.\n";
            return;
        }

        cout << "+----------------------+------------+------------+\n";
        cout << "| Table Name           | Columns    | Rows       |\n";
        cout << "+----------------------+------------+------------+\n";
        for (int i = 0; i < (int)tables.size(); i++) {
            cout << "| " << left << setw(20) << tables[i].getName()
                 << " | " << left << setw(10) << tables[i].getColumnCount()
                 << " | " << left << setw(10) << tables[i].getRowCount()
                 << " |\n";
        }
        cout << "+----------------------+------------+------------+\n";
        cout << tables.size() << " table(s) in total.\n";
    }

    void saveAll() {
        ofstream catalog("db_catalog.txt");
        if (!catalog.is_open()) return;

        for (int i = 0; i < (int)tables.size(); i++) {
            catalog << tables[i].getName() << "\n";
            tables[i].saveToFile(getFilename(tables[i].getName()));
        }
        catalog.close();
    }

    void loadAll() {
        ifstream catalog("db_catalog.txt");
        if (!catalog.is_open()) return;

        tables.clear();
        string tableName;
        while (getline(catalog, tableName)) {
            tableName = Utils::trim(tableName);
            if (tableName.empty()) continue;

            Table table(tableName);
            string errorMsg;
            if (table.loadFromFile(getFilename(tableName), errorMsg)) {
                tables.push_back(table);
            }
        }
        catalog.close();
    }
};

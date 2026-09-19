#pragma once
#include <iostream>
#include <string>
#include "Utils.cpp"

using namespace std;

class Column {
private:
    string name;
    string type;
    bool isPrimaryKey;
    bool isUnique;
    bool isNotNull;

public:
    Column() {
        name = "";
        type = "TEXT";
        isPrimaryKey = false;
        isUnique = false;
        isNotNull = false;
    }

    Column(string columnName, string columnType, bool primaryKey = false, bool unique = false, bool notNull = false) {
        name = columnName;
        type = Utils::toUpper(columnType);
        isPrimaryKey = primaryKey;
        isUnique = unique || primaryKey;
        isNotNull = notNull || primaryKey;
    }

    string getName() const { return name; }
    string getType() const { return type; }
    bool getIsPrimaryKey() const { return isPrimaryKey; }
    bool getIsUnique() const { return isUnique; }
    bool getIsNotNull() const { return isNotNull; }

    void setName(string columnName) { name = columnName; }
    void setType(string columnType) { type = Utils::toUpper(columnType); }

    void setIsPrimaryKey(bool primaryKey) {
        isPrimaryKey = primaryKey;
        if (primaryKey) {
            isUnique = true;
            isNotNull = true;
        }
    }

    void setIsUnique(bool unique) { isUnique = unique; }
    void setIsNotNull(bool notNull) { isNotNull = notNull; }

    bool isInt() const {
        return type == "INT" || type == "INTEGER";
    }

    bool isFloat() const {
        return type == "FLOAT" || type == "DOUBLE";
    }

    bool isText() const {
        return type == "TEXT" || type == "VARCHAR" || type == "STRING";
    }

    bool operator==(const Column& other) const {
        return Utils::toUpper(name) == Utils::toUpper(other.name);
    }
};

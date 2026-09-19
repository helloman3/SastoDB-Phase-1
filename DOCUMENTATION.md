# SastoDB System & Code Architecture Documentation

This document provides a detailed technical reference for the internal architecture, classes, methods, constraint enforcement mechanisms, and storage format of the **SastoDB (Phase 1)** relational database engine.

---

## 1. Architectural Overview

SastoDB is structured into modular layers designed to separate concerns between user interaction, SQL parsing, statement execution, table storage, and disk persistence:

```text
+-------------------------------------------------------------+
|                      User / Client                          |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                     REPL Shell (main.cpp)                   |
|  - Manages multiline input buffer                           |
|  - Filters comments and test script line numbers            |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                    Query Parser (parseQuery)                |
|  - Tokenizes SQL statements                                 |
|  - Instantiates concrete Command objects (polymorphic)      |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                     Command Execution                       |
|  - CreateTableCommand, InsertCommand, SelectCommand,        |
|    UpdateCommand, DeleteCommand, DropCommand, ViewCommand   |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                     Database Coordinator                    |
|  - Maintains list of Table objects                          |
|  - Handles catalog persistence (db_catalog.txt)             |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                         Table Layer                         |
|  - Column Schema (vector<Column>)                           |
|  - Row Storage (vector<Row>)                                |
|  - Constraint validation (PK, UNIQUE, NOT NULL, DataType)   |
|  - Table file persistence (<name>_data.txt)                 |
+-------------------------------------------------------------+
```

---

## 2. Core Modules & Class Reference

### 2.1. `Utils` (`include/Utils.cpp`)
Provides helper functions for string formatting, tokenization, and type conversion.

- `string trim(const string& text)`: Strips whitespace characters from both ends of a string.
- `string toUpper(string text)`: Returns an uppercase copy of the string for case-insensitive keyword comparisons.
- `string stripQuotes(string text)`: Removes enclosing single quotes (`'`) or double quotes (`"`).
- `bool isNumeric(string text)`: Verifies if a string consists exclusively of valid integer digits (with optional leading `+` or `-`).
- `int stringToInt(const string& text)`: Converts string to integer using `std::stoi` with exception fallback.
- `double stringToFloat(const string& text)`: Converts string to double using `std::stod` with exception fallback.
- `vector<string> split(const string& text, char delimiter)`: Tokenizes a string by a delimiter while keeping quoted substrings intact (e.g. `(1, 'Alex Smith', 92)` correctly splits without breaking inside `'Alex Smith'`).

---

### 2.2. `Column` (`include/Column.cpp`)
Represents the metadata and schema of an individual table column.

#### Member Variables
- `string name`: Column identifier.
- `string type`: Data type (`INT`, `TEXT`, `FLOAT`).
- `bool isPrimaryKey`: Indicates whether the column is designated as a Primary Key.
- `bool isUnique`: Indicates whether values must be unique across all rows.
- `bool isNotNull`: Indicates whether the column forbids `NULL` or empty entries.

#### Key Methods
- `Column(string name, string type, bool pk, bool uq, bool nn)`: Constructor. When `pk` is `true`, `isUnique` and `isNotNull` are automatically enabled.
- `bool isInt() const`: Returns `true` if type is `INT` or `INTEGER`.
- `bool isFloat() const`: Returns `true` if type is `FLOAT` or `DOUBLE`.
- `bool isText() const`: Returns `true` if type is `TEXT`, `VARCHAR`, or `STRING`.
- `bool operator==(const Column& other) const`: Compares two columns case-insensitively by name.

---

### 2.3. `Row` (`include/Row.cpp`)
Encapsulates a single record in a table using `vector<string> values`.

#### Key Methods
- `Row(int size)`: Initializes a row with empty strings for a given column width.
- `Row(const vector<string>& initialValues)`: Initializes a row directly from a vector of string values.
- `int getCount() const`: Returns the number of columns in the row.
- `string get(int index) const`: Accesses the value at the given column index safely.
- `void set(int index, const string& value)`: Updates the value at the given column index safely.
- `string& operator[](int index)`: Subscript operator for convenient indexed access (`row[i]`).
- `friend ostream& operator<<(ostream& os, const Row& row)`: Formats and outputs the row cells separated by pipes (`| val1 | val2 |`).

---

### 2.4. `Table` (`include/Table.cpp`)
Maintains the column schema and all row entries for a named relational table.

#### Internal State
- `string name`: Name of the table.
- `vector<Column> columns`: List of column definitions.
- `vector<Row> rows`: Stored records.

#### Operational Methods
- `void addColumn(const Column& col)`: Appends a column definition.
- `int getColumnIndex(const string& colName) const`: Returns the zero-based index of a column by name, or `-1` if not found.
- `bool insertRow(const Row& row, string& errorMsg)`: Validates column count, `NOT NULL` rules, integer format, and uniqueness before appending to `rows`.
- `bool selectRows(const string& whereCol, const string& op, const string& whereVal, int& outCount) const`:
  - Evaluates rows against the `WHERE` condition (or selects all if omitted).
  - Dynamically computes column widths.
  - Formats output using `<iomanip>` (`setw`, `left`) into a clean ASCII grid.
- `bool updateRows(...)`: Updates target column values for rows matching the condition while preventing constraint violations.
- `bool deleteRows(...)`: Erases matching rows from memory.
- `void displaySchema() const`: Displays column names, types, and constraint flags in a formatted table.
- `bool saveToFile(const string& filename) const`: Serializes schema and rows to disk.
- `bool loadFromFile(const string& filename, string& errorMsg)`: Deserializes schema and rows from disk.

---

### 2.5. `Database` (`include/Database.cpp`)
Acts as the central coordinator for multiple tables in the database.

#### Key Methods
- `bool createTable(string name, const vector<Column>& cols, string& errorMsg)`: Validates uniqueness of table name, creates `Table`, and persists state.
- `bool dropTable(const string& name, string& errorMsg)`: Removes table from memory and deletes its corresponding disk file (`<name>_data.txt`).
- `Table* getTable(const string& name)`: Searches and returns a pointer to the active table instance.
- `void listTables() const`: Prints a formatted directory of all tables, their column counts, and row counts.
- `void saveAll()`: Writes all active table names to `db_catalog.txt` and invokes `saveToFile()` on each.
- `void loadAll()`: Reads `db_catalog.txt` on startup and restores tables into memory.

---

### 2.6. Command Pattern Hierarchy (`include/Command.cpp`)
Implements the Command design pattern using dynamic polymorphism:

- `class Command`: Abstract base class with `virtual void execute(Database& db) = 0;` and `virtual ~Command() = default;`.
- **Concrete Commands**:
  - `CreateTableCommand`: Executes table creation.
  - `DropCommand`: Drops a table and cleans up storage.
  - `InsertCommand`: Inserts records and saves state.
  - `SelectCommand`: Queries rows and displays output.
  - `UpdateCommand`: Modifies matching records.
  - `DeleteCommand`: Deletes matching records.
  - `ViewCommand`: Shows catalog list or schema inspection.
  - `HelpCommand`: Displays SQL syntax help.

---

### 2.7. Shell & Parser (`main.cpp`)
- `unique_ptr<Command> parseQuery(string query, string& errorMsg)`: Parses SQL query strings into their corresponding polymorphic command instances.
- `int main()`: Runs the continuous interactive REPL loop (`db> ` / `...> `), handles `.exit` termination, and manages execution lifecycle.

---

## 3. Constraint Enforcement System

SastoDB implements strict validation before any row modification occurs:

1. **NOT NULL Constraint**:
   - If a column is defined with `NOT NULL` (or `PRIMARY KEY`), any empty string or string equaling `"NULL"` (case-insensitive) causes insertion/update rejection.
2. **PRIMARY KEY & UNIQUE Constraints**:
   - Uniqueness is verified by scanning existing rows in the table for matching values in the specified column index.
   - Any duplicate value produces an explicit error message.
3. **Multi-Row Update Collision Check**:
   - If an `UPDATE` statement matches multiple rows and targets a `PRIMARY KEY` or `UNIQUE` column, the engine aborts the entire update to prevent creating duplicate records.
4. **Data Type Validation**:
   - For `INT` columns, input values are checked via `Utils::isNumeric()`. Non-integer text (e.g. `'abc'`) is rejected immediately.

---

## 4. File Storage Format

Data is stored in plain text files formatted for transparent inspection and human readability:

### Catalog File (`db_catalog.txt`)
Contains a newline-separated list of active table names:
```text
students
inventory
```

### Table Data File (`<tableName>_data.txt`)
Contains table metadata, column definitions, and tab-separated rows:
```text
TABLE:students
COLUMNS:3
id:INT:1:1:1
name:TEXT:0:1:0
score:INT:0:0:1
ROWS:2
1	Alex	95
2	Bella	88
```
- Column format: `<name>:<type>:<isPrimaryKey>:<isUnique>:<isNotNull>` (`1` = true, `0` = false).
- Row format: Tab-delimited record values (`\t`).

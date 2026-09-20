# SastoDB System & Code Architecture Documentation

This document provides a comprehensive technical reference for the internal architecture, classes, methods, constraint enforcement mechanisms, disk persistence specification, and an in-depth **line-by-line / section-by-section code walkthrough** of the **SastoDB (Phase 1) Single-File Engine**.

---

## 1. Architectural Overview & Design Philosophy

SastoDB is an embedded relational database management system (RDBMS) engine written from scratch in pure C++17. Unlike typical multi-file database architectures that introduce deep class hierarchies, polymorphic indirection, and fragmented header dependencies, SastoDB employs a **streamlined, self-contained single-file architecture** contained in `src/main.cpp`.

### 1.1. Why the Streamlined Single-File Engine?
1. **Zero Header Overhead**: Eliminates fragile circular `.cpp`/`.h` includes and unified compilation issues.
2. **Direct Execution Model**: Replaces the bulky Command pattern (which added 220+ lines of polymorphic wrappers) with direct, zero-indirection SQL dispatching.
3. **Transparent Data Flow**: A query moves linearly: Input $\rightarrow$ Comment Stripper $\rightarrow$ SQL Tokenizer $\rightarrow$ Table Operations $\rightarrow$ Immediate Disk Persistence.
4. **Pedagogical Clarity**: Developers and evaluators can read the entire codebase sequentially from top to bottom in under 10 minutes.

### 1.2. High-Level Architecture Diagram

```text
+-------------------------------------------------------------+
|                      User / Client                          |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                 REPL Shell (Lines 482-530)                  |
|  - Manages multiline input buffer (waits for ';')           |
|  - Filters SQL comments ('--') and test script line numbers |
|  - Handles shell commands (.help, .exit)                    |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|              Query Parser & Dispatcher (Lines 337-480)       |
|  - parseWhere: Parses comparison operators (=, !=, <, >, etc)|
|  - executeSQL: Tokenizes DDL & DML statements directly      |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|               Database Coordinator (Lines 262-335)          |
|  - Manages table catalog in memory (vector<Table>)          |
|  - Coordinates catalog persistence (db_catalog.txt)         |
|  - Handles CREATE TABLE and DROP TABLE lifecycle            |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
|                   Table Engine (Lines 90-260)               |
|  - Schema definition (vector<Column>)                       |
|  - Record storage (vector<Row>, where Row = vector<string>) |
|  - Constraint enforcement (PK, UNIQUE, NOT NULL, DataType)  |
|  - Formatted ASCII tabular grid renderer (<iomanip>)        |
|  - File serialization (<tableName>_data.txt)                |
+-------------------------------------------------------------+
```

---

## 2. In-Depth Line-by-Line Code Walkthrough

The entire engine resides in `src/main.cpp`. Below is a section-by-section and function-by-function explanation of how every part functions:

### 2.1. Standard Library Includes (Lines 1–10)
```cpp
#include <iostream>  // Standard I/O (cin, cout)
#include <string>    // std::string manipulation
#include <vector>    // Dynamic arrays for rows and columns
#include <fstream>   // File streams for disk persistence
#include <iomanip>   // std::setw, std::left for aligned ASCII tables
#include <cctype>    // std::isdigit, std::toupper
#include <cstdio>    // remove() for deleting dropped table files
using namespace std;
```
- **Rationale**: Uses standard C++17 headers exclusively. No external libraries or third-party dependencies are required.

---

### 2.2. Section 1: Core Data Structures (Lines 12–26)

#### `struct Column` (Lines 14–24)
Represents the metadata and constraint configuration of a table column:
- `string name`: Column name (e.g., `id`, `name`, `score`).
- `string type = "TEXT"`: Storage type (`INT`, `FLOAT`, `TEXT`).
- `bool isPK = false`: Indicates whether the column is designated as `PRIMARY KEY`. When true, it automatically enforces both uniqueness and non-null values.
- `bool isUnique = false`: Rejects duplicate values across rows.
- `bool isNotNull = false`: Forbids empty or `NULL` values.
- **Helper Methods**:
  - `isInt()`: Returns true if column type is `INT` or `INTEGER`.
  - `isFloat()`: Returns true if column type is `FLOAT` or `DOUBLE`.
  - `isText()`: Returns true if column type is `TEXT`, `VARCHAR`, or `STRING`.

#### `using Row = vector<string>;` (Line 26)
- Replaces 65 lines of wrapper boilerplate with a direct C++ type alias. Every record is an array of strings representing cell values.

---

### 2.3. Section 2: Utilities (`namespace Utils`) (Lines 28–88)

- **`trim(const string& s)` (Lines 32–37)**: Finds the first and last non-whitespace characters (`' '`, `\t`, `\r`, `\n`) and returns the trimmed substring. If the string contains only spaces, it returns `""`.
- **`toUpper(string s)` (Lines 39–42)**: Returns an uppercase copy of a string for case-insensitive SQL keyword comparisons (e.g., matching `select` with `SELECT`).
- **`stripQuotes(string s)` (Lines 44–51)**: Checks if a value starts and ends with matching single quotes (`'`) or double quotes (`"`). If so, it strips them (e.g., `'Alex'` becomes `Alex`).
- **`isNumeric(string s)` (Lines 53–62)**: Validates whether a string is a valid integer. Supports optional leading `+` or `-` followed by digits only.
- **`split(const string& text, char delimiter)` (Lines 64–79)**: Splits text by a delimiter while respecting quotes. If a comma appears inside quotes (e.g., `'New York, NY'`), it is **not** treated as a separator.
- **`getParenContent(const string& s, string& inside)` (Lines 81–87)**: Extracts content between the first `(` and the last `)` in a query string.

---

### 2.4. Section 3: Table Engine (`class Table`) (Lines 90–260)

The `Table` class encapsulates schema definition, in-memory records, query filtering, constraint enforcement, and disk I/O.

#### Member Variables (Lines 92–95)
- `string name`: Name of the table.
- `vector<Column> columns`: Ordered list of column definitions.
- `vector<Row> rows`: Collection of row vectors.

#### `matchCondition(...)` (Lines 97–124)
Evaluates comparison operators (`=`, `==`, `!=`, `<`, `>`, `<=`, `>=`) based on column data type:
1. **INT**: Parses values using `stoi()`, performs numeric comparison.
2. **FLOAT**: Parses values using `stod()`, performs floating-point comparison.
3. **TEXT**: Performs lexicographical string comparison.

#### `insertRow(const Row& row, string& err)` (Lines 147–177)
Executes a 4-step constraint validation before appending a row:
1. **Column Count Validation**: Ensures the number of values matches `columns.size()`.
2. **NOT NULL Check**: Rejects empty values or explicit `NULL` if column has `isNotNull = true`.
3. **Data Type Validation**: If column is `INT`, verifies value using `Utils::isNumeric()`. Rejects strings like `'abc'`.
4. **Uniqueness Check**: If column is `isPK` or `isUnique`, iterates through existing rows. If a duplicate is found, aborts with a descriptive constraint violation error.

#### `selectRows(...)` (Lines 179–214)
1. Resolves `whereCol` index if provided (returns an error if the column does not exist).
2. Computes dynamic column widths using the maximum length among column headers and matched cell contents.
3. Renders a formatted ASCII table using `std::setw()` and `std::left`.
4. Reports total matching row count.

#### `updateRows(...)` (Lines 216–264)
1. Validates that the target column and WHERE column exist.
2. Validates NOT NULL and INT constraints on the new value.
3. **Multi-Row Protection**: If target column is `isPK` or `isUnique`:
   - Rejects updates if multiple rows would be updated to the exact same value.
   - Rejects update if any non-matching row already possesses the target value.
4. Updates matching rows in-place and returns updated count.

#### `deleteRows(...)` (Lines 266–283)
1. Validates WHERE column.
2. Filters out matching rows and keeps non-matching rows.
3. Returns deleted count.

#### `displaySchema()` (Lines 285–302)
Prints an aligned ASCII schema grid showing Column Name, Type, Primary Key (YES/NO), Unique (YES/NO), and Not Null (YES/NO).

#### `saveToFile(...)` and `loadFromFile(...)` (Lines 304–345)
Handles disk serialization using a clean, human-readable TSV format (see Section 4).

---

### 2.5. Section 4: Database Catalog (`class Database`) (Lines 262–335)

- `createTable(name, cols, err)`: Verifies table name uniqueness, instantiates `Table`, appends to `tables`, and calls `saveAll()`.
- `dropTable(name, err)`: Removes table from memory, calls `remove()` to delete the `<tableName>_data.txt` file from disk, and updates `db_catalog.txt`.
- `listTables()`: Displays all existing tables, column counts, and row counts in an ASCII summary table.
- `saveAll()`: Writes all table names to `db_catalog.txt` and triggers `saveToFile()` on each table.
- `loadAll()`: Reads `db_catalog.txt` and rehydrates each table from its corresponding data file.

---

### 2.6. Section 5: Query Parser & Dispatcher (Lines 337–480)

- **`parseWhere(string wherePart, string& col, string& op, string& val)` (Lines 339–363)**:
  - Scans for 2-character operators (`!=`, `<=`, `>=`, `==`) first, then 1-character operators (`=`, `<`, `>`).
  - Extracts the column name and comparison value cleanly.
- **`executeSQL(Database& db, string sql)` (Lines 365–480)**:
  - Strips trailing semicolons (`;`).
  - Dispatches commands directly via case-insensitive prefix checks:
    - `HELP` $\rightarrow$ prints available SQL commands.
    - `VIEW TABLES` / `VIEW <name>` $\rightarrow$ lists tables or inspects table schema.
    - `CREATE TABLE` $\rightarrow$ parses column definitions, types, and constraints (`PRIMARY KEY`, `UNIQUE`, `NOT NULL`).
    - `DROP TABLE` $\rightarrow$ drops table and deletes disk file.
    - `INSERT INTO` $\rightarrow$ extracts VALUES tuple, strips quotes, and inserts.
    - `SELECT` $\rightarrow$ parses table name, optional WHERE clause, and displays results.
    - `UPDATE` $\rightarrow$ parses SET clause, optional WHERE clause, and applies updates.
    - `DELETE FROM` $\rightarrow$ parses optional WHERE clause and removes rows.

---

### 2.7. Section 6: REPL Shell in `main()` (Lines 482–530)

- Initializes `Database db("student_db")` (automatically loads existing tables on startup).
- Displays the ASCII welcome banner and user instructions.
- Executes an interactive input loop:
  - Displays prompt: `db> ` (or continuation prompt `...> ` during multiline input).
  - Strips SQL comments starting with `--`.
  - Ignores leading numbers from test scripts (e.g., `1. Create table...`).
  - Aggregates lines into a buffer until a semicolon (`;`) or dot-command (`.help`, `.exit`) is detected.
  - On `.exit`, triggers database persistence and cleanly shuts down.

---

## 3. Constraint Enforcement System

| Constraint | Enforcement Mechanism | Behavior on Violation |
| :--- | :--- | :--- |
| **`PRIMARY KEY`** | Sets both `isUnique = true` and `isNotNull = true`. | Rejects duplicates or NULLs with `Error: Duplicate value 'X' violates PRIMARY KEY constraint...` |
| **`UNIQUE`** | Scans table rows for existing value matches. | Rejects duplicate insertions with `Error: Duplicate value 'X' violates UNIQUE constraint...` |
| **`NOT NULL`** | Checks if value is empty or equal to `"NULL"`. | Rejects empty values with `Error: Column 'col' cannot be NULL.` |
| **`INT` Type Check** | `Utils::isNumeric()` checks characters against `0-9`. | Rejects non-integer strings with `Error: Invalid integer value 'abc' for column 'col'.` |
| **Multi-Row UPDATE Protection** | Verifies update count before modifying unique/PK column. | Rejects multi-row duplicate assignments with `Error: Cannot update multiple rows to identical value...` |

---

## 4. Disk Storage & File Format Specification

SastoDB guarantees complete crash-resilient disk persistence across two file layers:

### 4.1. Catalog File (`db_catalog.txt`)
Contains table names registered in the database, one per line:
```text
students
inventory
```

### 4.2. Table Data File (`<tableName>_data.txt`)
Each table persists its complete schema and records in a tab-separated text format:
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

- **Line 1**: Table identifier (`TABLE:<name>`).
- **Line 2**: Total column count (`COLUMNS:<count>`).
- **Lines 3 to N+2**: Column definition formatted as `<name>:<type>:<isPK>:<isUnique>:<isNotNull>`.
- **Line N+3**: Row count (`ROWS:<count>`).
- **Lines N+4 to end**: Row records, tab-delimited (`\t`).

---

## 5. SQL Syntax Reference

### 1. DDL (Data Definition Language)
```sql
-- Create table with constraints
CREATE TABLE students (id INT PRIMARY KEY, name TEXT UNIQUE, score INT NOT NULL);

-- Drop table and remove file
DROP TABLE students;
```

### 2. DML (Data Manipulation Language)
```sql
-- Insert values (supports quotes or unquoted text)
INSERT INTO students VALUES (1, 'Alex', 92);
INSERT INTO students VALUES (2, Bella, 88);

-- Select all records
SELECT * FROM students;

-- Select with comparison filter
SELECT * FROM students WHERE id = 1;
SELECT * FROM students WHERE score >= 90;

-- Update records
UPDATE students SET score = 95 WHERE id = 1;

-- Delete records
DELETE FROM students WHERE id = 2;
```

### 3. Inspection & Meta Commands
```sql
-- List all tables in catalog
VIEW TABLES;

-- Inspect column schema of a table
VIEW students;

-- View help
.help;

-- Exit shell and persist database
.exit
```

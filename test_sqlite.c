#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "sqlite_utils.h"

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s\n", message); \
        printf("  Error: %s\n", sqlite_get_last_error()); \
    } \
} while(0)

void test_database_operations() {
    printf("\n=== Testing Database Operations ===\n");
    
    // Test opening in-memory database
    SqliteDatabase* db = sqlite_open_database(":memory:", SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
    TEST_ASSERT(db != NULL, "Open in-memory database");
    TEST_ASSERT(sqlite_is_database_open(db), "Database is open");
    
    if (db) {
        // Test database path
        const char* path = sqlite_get_database_path(db);
        TEST_ASSERT(path && strcmp(path, ":memory:") == 0, "Database path is correct");
        
        // Test database stats
        SqliteDatabaseStats* stats = sqlite_get_database_stats(db);
        TEST_ASSERT(stats != NULL, "Get database stats");
        if (stats) {
            printf("  Page size: %lld bytes\n", (long long)stats->page_size);
            printf("  SQLite version: %s\n", stats->sqlite_version);
            sqlite_free_database_stats(stats);
        }
        
        // Test pragma operations
        bool pragma_ok = sqlite_set_pragma(db, "synchronous", "NORMAL");
        TEST_ASSERT(pragma_ok, "Set pragma value");
        
        char* pragma_val = sqlite_get_pragma(db, "synchronous");
        TEST_ASSERT(pragma_val != NULL, "Get pragma value");
        if (pragma_val) {
            printf("  Synchronous mode: %s\n", pragma_val);
            free(pragma_val);
        }
        
        // Test foreign keys
        bool fk_ok = sqlite_set_foreign_keys(db, true);
        TEST_ASSERT(fk_ok, "Enable foreign keys");
        
        // Close database
        bool close_ok = sqlite_close_database(db);
        TEST_ASSERT(close_ok, "Close database");
    }
}

void test_sql_execution() {
    printf("\n=== Testing SQL Execution ===\n");
    
    SqliteDatabase* db = sqlite_open_database(":memory:", SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
    if (!db) return;
    
    // Create test table
    const char* create_sql = 
        "CREATE TABLE users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "email TEXT UNIQUE,"
        "age INTEGER,"
        "balance REAL,"
        "data BLOB"
        ")";
    
    int result = sqlite_execute_sql(db, create_sql);
    TEST_ASSERT(result >= 0, "Create table");
    
    // Insert test data
    const char* insert_sql = 
        "INSERT INTO users (name, email, age, balance) VALUES "
        "('John Doe', 'john@example.com', 30, 1000.50),"
        "('Jane Smith', 'jane@example.com', 25, 2500.75),"
        "('Bob Johnson', 'bob@example.com', 35, 500.25)";
    
    result = sqlite_execute_sql(db, insert_sql);
    TEST_ASSERT(result == 3, "Insert multiple rows");
    
    // Test last insert rowid
    int64_t last_id = sqlite_last_insert_rowid(db);
    TEST_ASSERT(last_id > 0, "Get last insert rowid");
    printf("  Last insert ID: %lld\n", (long long)last_id);
    
    // Test changes
    int changes = sqlite_changes(db);
    TEST_ASSERT(changes > 0, "Get number of changes");
    printf("  Changes: %d\n", changes);
    
    sqlite_close_database(db);
}

void test_query_execution() {
    printf("\n=== Testing Query Execution ===\n");
    
    SqliteDatabase* db = sqlite_open_database(":memory:", SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
    if (!db) return;
    
    // Create and populate test table
    sqlite_execute_sql(db, 
        "CREATE TABLE products ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT,"
        "price REAL,"
        "in_stock INTEGER"
        ")");
    
    sqlite_execute_sql(db,
        "INSERT INTO products (name, price, in_stock) VALUES "
        "('Laptop', 999.99, 1),"
        "('Mouse', 29.99, 0),"
        "('Keyboard', 79.99, 1)");
    
    // Execute query
    SqliteResult* result = sqlite_execute_query(db, "SELECT * FROM products WHERE in_stock = 1");
    TEST_ASSERT(result != NULL, "Execute SELECT query");
    
    if (result) {
        int row_count = sqlite_result_row_count(result);
        int col_count = sqlite_result_column_count(result);
        
        printf("  Query returned %d rows, %d columns\n", row_count, col_count);
        TEST_ASSERT(col_count == 4, "Correct column count");
        
        // Test column names
        for (int i = 0; i < col_count; i++) {
            const char* col_name = sqlite_result_column_name(result, i);
            printf("  Column %d: %s\n", i, col_name ? col_name : "NULL");
        }
        
        // Iterate through rows
        SqliteRow* row = sqlite_result_first_row(result);
        int actual_rows = 0;
        while (row) {
            actual_rows++;
            
            int64_t id = sqlite_row_get_int(row, 0, -1);
            const char* name = sqlite_row_get_text(row, 1);
            double price = sqlite_row_get_real(row, 2, 0.0);
            int64_t in_stock = sqlite_row_get_int(row, 3, -1);
            
            printf("  Row %d: ID=%lld, Name=%s, Price=%.2f, InStock=%lld\n", 
                   actual_rows, (long long)id, name ? name : "NULL", price, (long long)in_stock);
            
            free(row);
            row = sqlite_result_next_row(result);
        }
        
        TEST_ASSERT(actual_rows == 2, "Correct number of rows returned");
        sqlite_free_result(result);
    }
    
    sqlite_close_database(db);
}

void test_prepared_statements() {
    printf("\n=== Testing Prepared Statements ===\n");
    
    SqliteDatabase* db = sqlite_open_database(":memory:", SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
    if (!db) return;
    
    // Create test table
    sqlite_execute_sql(db, 
        "CREATE TABLE orders ("
        "id INTEGER PRIMARY KEY,"
        "customer_name TEXT,"
        "amount REAL,"
        "order_date TEXT"
        ")");
    
    // Prepare insert statement
    SqliteStatement* stmt = sqlite_prepare_statement(db, 
        "INSERT INTO orders (customer_name, amount, order_date) VALUES (?, ?, ?)");
    TEST_ASSERT(stmt != NULL, "Prepare INSERT statement");
    
    if (stmt) {
        // Bind parameters and execute
        bool bind_ok = true;
        bind_ok &= sqlite_bind_text(stmt, 1, "Alice Johnson");
        bind_ok &= sqlite_bind_real(stmt, 2, 156.78);
        bind_ok &= sqlite_bind_text(stmt, 3, "2023-12-01");
        
        TEST_ASSERT(bind_ok, "Bind parameters");
        
        SqliteResult* result = sqlite_execute_statement(stmt);
        TEST_ASSERT(result != NULL, "Execute prepared statement");
        if (result) {
            sqlite_free_result(result);
        }
        
        // Reset and execute again with different parameters
        bool reset_ok = sqlite_reset_statement(stmt);
        TEST_ASSERT(reset_ok, "Reset statement");
        
        sqlite_clear_bindings(stmt);
        sqlite_bind_text(stmt, 1, "Bob Wilson");
        sqlite_bind_real(stmt, 2, 299.99);
        sqlite_bind_text(stmt, 3, "2023-12-02");
        
        result = sqlite_execute_statement(stmt);
        TEST_ASSERT(result != NULL, "Execute statement again");
        if (result) {
            sqlite_free_result(result);
        }
        
        sqlite_finalize_statement(stmt);
    }
    
    // Verify inserted data
    SqliteResult* verify_result = sqlite_execute_query(db, "SELECT COUNT(*) FROM orders");
    if (verify_result) {
        SqliteRow* row = sqlite_result_first_row(verify_result);
        if (row) {
            int64_t count = sqlite_row_get_int(row, 0, 0);
            TEST_ASSERT(count == 2, "Correct number of rows inserted via prepared statements");
            free(row);
        }
        sqlite_free_result(verify_result);
    }
    
    sqlite_close_database(db);
}

void test_transactions() {
    printf("\n=== Testing Transactions ===\n");
    
    SqliteDatabase* db = sqlite_open_database(":memory:", SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
    if (!db) return;
    
    // Create test table
    sqlite_execute_sql(db, 
        "CREATE TABLE accounts ("
        "id INTEGER PRIMARY KEY,"
        "name TEXT,"
        "balance REAL"
        ")");
    
    sqlite_execute_sql(db, 
        "INSERT INTO accounts (name, balance) VALUES "
        "('Account A', 1000.00),"
        "('Account B', 500.00)");
    
    // Test successful transaction
    SqliteTransaction* transaction = sqlite_begin_transaction(db, TRANSACTION_DEFERRED);
    TEST_ASSERT(transaction != NULL, "Begin transaction");
    
    if (transaction) {
        // Transfer money from A to B
        int result1 = sqlite_execute_sql(db, "UPDATE accounts SET balance = balance - 200 WHERE name = 'Account A'");
        int result2 = sqlite_execute_sql(db, "UPDATE accounts SET balance = balance + 200 WHERE name = 'Account B'");
        
        TEST_ASSERT(result1 >= 0 && result2 >= 0, "Execute transaction statements");
        
        bool commit_ok = sqlite_commit_transaction(transaction);
        TEST_ASSERT(commit_ok, "Commit transaction");
    }
    
    // Verify transaction results
    SqliteResult* result = sqlite_execute_query(db, "SELECT name, balance FROM accounts ORDER BY name");
    if (result) {
        SqliteRow* row = sqlite_result_first_row(result);
        if (row) {
            const char* name = sqlite_row_get_text(row, 0);
            double balance = sqlite_row_get_real(row, 1, 0.0);
            TEST_ASSERT(name && strcmp(name, "Account A") == 0 && balance == 800.0, "Account A balance updated");
            free(row);
            
            row = sqlite_result_next_row(result);
            if (row) {
                name = sqlite_row_get_text(row, 0);
                balance = sqlite_row_get_real(row, 1, 0.0);
                TEST_ASSERT(name && strcmp(name, "Account B") == 0 && balance == 700.0, "Account B balance updated");
                free(row);
            }
        }
        sqlite_free_result(result);
    }
    
    // Test rollback
    transaction = sqlite_begin_transaction(db, TRANSACTION_IMMEDIATE);
    if (transaction) {
        sqlite_execute_sql(db, "UPDATE accounts SET balance = 0 WHERE name = 'Account A'");
        
        bool rollback_ok = sqlite_rollback_transaction(transaction);
        TEST_ASSERT(rollback_ok, "Rollback transaction");
        
        // Verify rollback
        result = sqlite_execute_query(db, "SELECT balance FROM accounts WHERE name = 'Account A'");
        if (result) {
            SqliteRow* row = sqlite_result_first_row(result);
            if (row) {
                double balance = sqlite_row_get_real(row, 0, -1.0);
                TEST_ASSERT(balance == 800.0, "Transaction rolled back successfully");
                free(row);
            }
            sqlite_free_result(result);
        }
    }
    
    sqlite_close_database(db);
}

void test_schema_operations() {
    printf("\n=== Testing Schema Operations ===\n");
    
    SqliteDatabase* db = sqlite_open_database(":memory:", SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
    if (!db) return;
    
    // Create test tables
    sqlite_execute_sql(db, 
        "CREATE TABLE customers ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "email TEXT UNIQUE,"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ")");
    
    sqlite_execute_sql(db,
        "CREATE TABLE orders ("
        "id INTEGER PRIMARY KEY,"
        "customer_id INTEGER REFERENCES customers(id),"
        "total REAL NOT NULL"
        ")");
    
    // Test table existence
    bool customers_exists = sqlite_table_exists(db, "customers");
    bool orders_exists = sqlite_table_exists(db, "orders");
    bool fake_exists = sqlite_table_exists(db, "fake_table");
    
    TEST_ASSERT(customers_exists, "Customers table exists");
    TEST_ASSERT(orders_exists, "Orders table exists");
    TEST_ASSERT(!fake_exists, "Fake table does not exist");
    
    // Get table list
    int table_count;
    char** tables = sqlite_get_table_list(db, &table_count);
    TEST_ASSERT(tables != NULL && table_count == 2, "Get table list");
    
    if (tables) {
        printf("  Found %d tables:\n", table_count);
        for (int i = 0; i < table_count; i++) {
            printf("    - %s\n", tables[i]);
            free(tables[i]);
        }
        free(tables);
    }
    
    // Get table info
    SqliteTableInfo* table_info = sqlite_get_table_info(db, "customers");
    TEST_ASSERT(table_info != NULL, "Get customers table info");
    
    if (table_info) {
        printf("  Table: %s\n", table_info->name);
        printf("  Columns: %d\n", table_info->column_count);
        
        for (int i = 0; i < table_info->column_count; i++) {
            SqliteColumnInfo* col = &table_info->columns[i];
            printf("    %s: type=%d, not_null=%s, pk=%s\n",
                   col->name,
                   col->type,
                   col->not_null ? "YES" : "NO",
                   col->primary_key ? "YES" : "NO");
        }
        
        sqlite_free_table_info(table_info);
    }
    
    // Test row count
    sqlite_execute_sql(db, "INSERT INTO customers (name, email) VALUES ('Test User', 'test@example.com')");
    int64_t row_count = sqlite_get_table_row_count(db, "customers");
    TEST_ASSERT(row_count == 1, "Get table row count");
    
    sqlite_close_database(db);
}

void test_utility_functions() {
    printf("\n=== Testing Utility Functions ===\n");
    
    SqliteDatabase* db = sqlite_open_database(":memory:", SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
    if (!db) return;
    
    // Create test table with data
    sqlite_execute_sql(db,
        "CREATE TABLE test_data (id INTEGER, value TEXT);"
        "INSERT INTO test_data VALUES (1, 'test'), (2, 'data');");
    
    // Test string escaping
    const char* unsafe_string = "O'Reilly's \"Book\"";
    char* escaped = sqlite_escape_string(unsafe_string);
    TEST_ASSERT(escaped != NULL, "Escape SQL string");
    if (escaped) {
        printf("  Original: %s\n", unsafe_string);
        printf("  Escaped:  %s\n", escaped);
        free(escaped);
    }
    
    // Test database integrity
    bool integrity_ok = sqlite_check_integrity(db);
    TEST_ASSERT(integrity_ok, "Database integrity check");
    
    // Test analyze
    bool analyze_ok = sqlite_analyze_database(db);
    TEST_ASSERT(analyze_ok, "Analyze database");
    
    // Test vacuum
    bool vacuum_ok = sqlite_vacuum_database(db);
    TEST_ASSERT(vacuum_ok, "Vacuum database");
    
    // Test backup
    const char* backup_file = "/tmp/test_backup.db";
    bool backup_ok = sqlite_backup_database(db, backup_file);
    TEST_ASSERT(backup_ok, "Backup database");
    
    if (backup_ok) {
        // Test restore to new database
        SqliteDatabase* restore_db = sqlite_open_database("/tmp/test_restore.db", 
            SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
        if (restore_db) {
            bool restore_ok = sqlite_restore_database(restore_db, backup_file);
            TEST_ASSERT(restore_ok, "Restore database");
            
            if (restore_ok) {
                // Verify restored data
                SqliteResult* result = sqlite_execute_query(restore_db, "SELECT COUNT(*) FROM test_data");
                if (result) {
                    SqliteRow* row = sqlite_result_first_row(result);
                    if (row) {
                        int64_t count = sqlite_row_get_int(row, 0, 0);
                        TEST_ASSERT(count == 2, "Restored data is correct");
                        free(row);
                    }
                    sqlite_free_result(result);
                }
            }
            
            sqlite_close_database(restore_db);
        }
        
        // Cleanup
        unlink(backup_file);
        unlink("/tmp/test_restore.db");
    }
    
    sqlite_close_database(db);
}

void test_value_functions() {
    printf("\n=== Testing Value Functions ===\n");
    
    // Test value creation
    SqliteValue* int_val = sqlite_value_create_int(12345);
    SqliteValue* real_val = sqlite_value_create_real(123.45);
    SqliteValue* text_val = sqlite_value_create_text("Hello, World!");
    SqliteValue* null_val = sqlite_value_create_null();
    
    TEST_ASSERT(int_val && int_val->type == SQLITE_TYPE_INTEGER, "Create integer value");
    TEST_ASSERT(real_val && real_val->type == SQLITE_TYPE_REAL, "Create real value");
    TEST_ASSERT(text_val && text_val->type == SQLITE_TYPE_TEXT, "Create text value");
    TEST_ASSERT(null_val && null_val->type == SQLITE_TYPE_NULL, "Create null value");
    
    // Test value to string conversion
    if (int_val) {
        char* str = sqlite_value_to_string(int_val);
        TEST_ASSERT(str && strcmp(str, "12345") == 0, "Integer to string");
        printf("  Integer value: %s\n", str);
        free(str);
    }
    
    if (real_val) {
        char* str = sqlite_value_to_string(real_val);
        TEST_ASSERT(str != NULL, "Real to string");
        printf("  Real value: %s\n", str);
        free(str);
    }
    
    if (text_val) {
        char* str = sqlite_value_to_string(text_val);
        TEST_ASSERT(str && strcmp(str, "Hello, World!") == 0, "Text to string");
        printf("  Text value: %s\n", str);
        free(str);
    }
    
    // Test blob value
    const char* blob_data = "Binary\0Data\xFF";
    SqliteValue* blob_val = sqlite_value_create_blob(blob_data, 11);
    TEST_ASSERT(blob_val && blob_val->type == SQLITE_TYPE_BLOB, "Create blob value");
    
    if (blob_val) {
        char* str = sqlite_value_to_string(blob_val);
        TEST_ASSERT(str != NULL, "Blob to string (hex)");
        printf("  Blob value (hex): %s\n", str);
        free(str);
    }
    
    // Cleanup
    sqlite_value_free(int_val);
    sqlite_value_free(real_val);
    sqlite_value_free(text_val);
    sqlite_value_free(null_val);
    sqlite_value_free(blob_val);
}

void test_error_handling() {
    printf("\n=== Testing Error Handling ===\n");
    
    // Test opening non-existent file with readonly flag
    SqliteDatabase* db = sqlite_open_database("/nonexistent/path/database.db", SQLITE_OPEN_READONLY_FLAG);
    TEST_ASSERT(db == NULL, "Opening non-existent readonly database fails");
    printf("  Expected error: %s\n", sqlite_get_last_error());
    
    sqlite_clear_error();
    const char* error_after_clear = sqlite_get_last_error();
    TEST_ASSERT(strlen(error_after_clear) == 0, "Error cleared successfully");
    
    // Test invalid SQL
    db = sqlite_open_database(":memory:", SQLITE_OPEN_READWRITE_FLAG | SQLITE_OPEN_CREATE_FLAG);
    if (db) {
        int result = sqlite_execute_sql(db, "INVALID SQL STATEMENT");
        TEST_ASSERT(result < 0, "Invalid SQL fails appropriately");
        printf("  SQL error: %s\n", sqlite_get_last_error());
        
        sqlite_close_database(db);
    }
}

void test_version_info() {
    printf("\n=== Testing Version Information ===\n");
    
    const char* sqlite_version = sqlite_get_version();
    const char* utils_version = sqlite_utils_get_version();
    
    TEST_ASSERT(sqlite_version != NULL, "Get SQLite version");
    TEST_ASSERT(utils_version != NULL, "Get utils version");
    
    printf("  SQLite version: %s\n", sqlite_version);
    printf("  Utils version: %s\n", utils_version);
}

int main() {
    printf("SQLite Utils Library Test Suite\n");
    printf("===============================\n");
    
    test_database_operations();
    test_sql_execution();
    test_query_execution();
    test_prepared_statements();
    test_transactions();
    test_schema_operations();
    test_utility_functions();
    test_value_functions();
    test_error_handling();
    test_version_info();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", 
           tests_run > 0 ? (double)tests_passed / tests_run * 100 : 0);
    
    if (tests_passed == tests_run) {
        printf("\n🎉 All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed!\n");
        return 1;
    }
}
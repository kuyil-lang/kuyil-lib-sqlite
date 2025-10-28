#ifndef SQLITE_UTILS_H
#define SQLITE_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

// Version information
#define SQLITE_UTILS_VERSION_MAJOR 1
#define SQLITE_UTILS_VERSION_MINOR 0
#define SQLITE_UTILS_VERSION_PATCH 0
#define SQLITE_UTILS_VERSION_STRING "1.0.0"

// Maximum limits
#define MAX_SQL_LENGTH 4096
#define MAX_ERROR_LENGTH 512
#define MAX_COLUMN_NAME_LENGTH 256
#define MAX_TABLE_NAME_LENGTH 256

// Forward declarations
typedef struct SqliteDatabase SqliteDatabase;
typedef struct SqliteStatement SqliteStatement;
typedef struct SqliteResult SqliteResult;
typedef struct SqliteRow SqliteRow;
typedef struct SqliteTransaction SqliteTransaction;

// Data types for SQLite values
typedef enum {
    SQLITE_TYPE_NULL = 0,
    SQLITE_TYPE_INTEGER,
    SQLITE_TYPE_REAL,
    SQLITE_TYPE_TEXT,
    SQLITE_TYPE_BLOB
} SqliteValueType;

// Transaction types
typedef enum {
    TRANSACTION_DEFERRED = 0,
    TRANSACTION_IMMEDIATE,
    TRANSACTION_EXCLUSIVE
} TransactionType;

// Connection flags
typedef enum {
    SQLITE_OPEN_READONLY_FLAG = 0x01,
    SQLITE_OPEN_READWRITE_FLAG = 0x02,
    SQLITE_OPEN_CREATE_FLAG = 0x04,
    SQLITE_OPEN_MEMORY_FLAG = 0x08
} SqliteOpenFlags;

// Value structure for SQLite data
typedef struct {
    SqliteValueType type;
    union {
        int64_t integer_val;
        double real_val;
        char* text_val;
        struct {
            void* data;
            size_t size;
        } blob_val;
    } value;
} SqliteValue;

// Column information
typedef struct {
    char name[MAX_COLUMN_NAME_LENGTH];
    SqliteValueType type;
    bool not_null;
    bool primary_key;
    char* default_value;
} SqliteColumnInfo;

// Table information
typedef struct {
    char name[MAX_TABLE_NAME_LENGTH];
    int column_count;
    SqliteColumnInfo* columns;
    int64_t row_count;
} SqliteTableInfo;

// Database statistics
typedef struct {
    int64_t page_count;
    int64_t page_size;
    int64_t freelist_count;
    int64_t schema_version;
    int64_t user_version;
    char* sqlite_version;
} SqliteDatabaseStats;

// ============================================================================
// Database Management Functions
// ============================================================================

/**
 * Open a SQLite database connection
 * @param filename Database file path (":memory:" for in-memory database)
 * @param flags Connection flags (combination of SqliteOpenFlags)
 * @return Database handle or NULL on error
 */
SqliteDatabase* sqlite_open_database(const char* filename, int flags);

/**
 * Close a SQLite database connection
 * @param db Database handle
 * @return true on success, false on error
 */
bool sqlite_close_database(SqliteDatabase* db);

/**
 * Check if database connection is valid
 * @param db Database handle
 * @return true if valid, false otherwise
 */
bool sqlite_is_database_open(SqliteDatabase* db);

/**
 * Get database file path
 * @param db Database handle
 * @return Database file path or NULL
 */
const char* sqlite_get_database_path(SqliteDatabase* db);

/**
 * Get database statistics
 * @param db Database handle
 * @return Statistics structure or NULL on error
 */
SqliteDatabaseStats* sqlite_get_database_stats(SqliteDatabase* db);

/**
 * Free database statistics structure
 * @param stats Statistics structure to free
 */
void sqlite_free_database_stats(SqliteDatabaseStats* stats);

/**
 * Enable or disable foreign key constraints
 * @param db Database handle
 * @param enable true to enable, false to disable
 * @return true on success, false on error
 */
bool sqlite_set_foreign_keys(SqliteDatabase* db, bool enable);

/**
 * Set database pragma value
 * @param db Database handle
 * @param pragma_name Pragma name (e.g., "journal_mode", "synchronous")
 * @param pragma_value Pragma value
 * @return true on success, false on error
 */
bool sqlite_set_pragma(SqliteDatabase* db, const char* pragma_name, const char* pragma_value);

/**
 * Get database pragma value
 * @param db Database handle
 * @param pragma_name Pragma name
 * @return Pragma value or NULL on error (must be freed)
 */
char* sqlite_get_pragma(SqliteDatabase* db, const char* pragma_name);

// ============================================================================
// SQL Execution Functions
// ============================================================================

/**
 * Execute a SQL statement (for non-SELECT queries)
 * @param db Database handle
 * @param sql SQL statement
 * @return Number of affected rows or -1 on error
 */
int sqlite_execute_sql(SqliteDatabase* db, const char* sql);

/**
 * Execute a SQL query and return results (for SELECT queries)
 * @param db Database handle
 * @param sql SQL query
 * @return Result set or NULL on error
 */
SqliteResult* sqlite_execute_query(SqliteDatabase* db, const char* sql);

/**
 * Prepare a SQL statement for execution
 * @param db Database handle
 * @param sql SQL statement
 * @return Prepared statement or NULL on error
 */
SqliteStatement* sqlite_prepare_statement(SqliteDatabase* db, const char* sql);

/**
 * Execute a prepared statement
 * @param stmt Prepared statement
 * @return Result set or NULL on error
 */
SqliteResult* sqlite_execute_statement(SqliteStatement* stmt);

/**
 * Finalize a prepared statement
 * @param stmt Prepared statement
 */
void sqlite_finalize_statement(SqliteStatement* stmt);

/**
 * Reset a prepared statement for reuse
 * @param stmt Prepared statement
 * @return true on success, false on error
 */
bool sqlite_reset_statement(SqliteStatement* stmt);

// ============================================================================
// Parameter Binding Functions
// ============================================================================

/**
 * Bind integer parameter to prepared statement
 * @param stmt Prepared statement
 * @param index Parameter index (1-based)
 * @param value Integer value
 * @return true on success, false on error
 */
bool sqlite_bind_int(SqliteStatement* stmt, int index, int64_t value);

/**
 * Bind real parameter to prepared statement
 * @param stmt Prepared statement
 * @param index Parameter index (1-based)
 * @param value Real value
 * @return true on success, false on error
 */
bool sqlite_bind_real(SqliteStatement* stmt, int index, double value);

/**
 * Bind text parameter to prepared statement
 * @param stmt Prepared statement
 * @param index Parameter index (1-based)
 * @param value Text value
 * @return true on success, false on error
 */
bool sqlite_bind_text(SqliteStatement* stmt, int index, const char* value);

/**
 * Bind blob parameter to prepared statement
 * @param stmt Prepared statement
 * @param index Parameter index (1-based)
 * @param data Blob data
 * @param size Data size
 * @return true on success, false on error
 */
bool sqlite_bind_blob(SqliteStatement* stmt, int index, const void* data, size_t size);

/**
 * Bind NULL parameter to prepared statement
 * @param stmt Prepared statement
 * @param index Parameter index (1-based)
 * @return true on success, false on error
 */
bool sqlite_bind_null(SqliteStatement* stmt, int index);

/**
 * Clear all parameter bindings
 * @param stmt Prepared statement
 * @return true on success, false on error
 */
bool sqlite_clear_bindings(SqliteStatement* stmt);

// ============================================================================
// Result Set Functions
// ============================================================================

/**
 * Get number of rows in result set
 * @param result Result set
 * @return Number of rows
 */
int sqlite_result_row_count(SqliteResult* result);

/**
 * Get number of columns in result set
 * @param result Result set
 * @return Number of columns
 */
int sqlite_result_column_count(SqliteResult* result);

/**
 * Get column name by index
 * @param result Result set
 * @param column_index Column index (0-based)
 * @return Column name or NULL
 */
const char* sqlite_result_column_name(SqliteResult* result, int column_index);

/**
 * Get column type by index
 * @param result Result set
 * @param column_index Column index (0-based)
 * @return Column type
 */
SqliteValueType sqlite_result_column_type(SqliteResult* result, int column_index);

/**
 * Get first row from result set
 * @param result Result set
 * @return First row or NULL
 */
SqliteRow* sqlite_result_first_row(SqliteResult* result);

/**
 * Get next row from result set
 * @param result Result set
 * @return Next row or NULL
 */
SqliteRow* sqlite_result_next_row(SqliteResult* result);

/**
 * Reset result set iterator to beginning
 * @param result Result set
 */
void sqlite_result_reset(SqliteResult* result);

/**
 * Free result set
 * @param result Result set
 */
void sqlite_free_result(SqliteResult* result);

// ============================================================================
// Row Access Functions
// ============================================================================

/**
 * Get value from row by column index
 * @param row Row handle
 * @param column_index Column index (0-based)
 * @return Value or NULL
 */
SqliteValue* sqlite_row_get_value(SqliteRow* row, int column_index);

/**
 * Get value from row by column name
 * @param row Row handle
 * @param column_name Column name
 * @return Value or NULL
 */
SqliteValue* sqlite_row_get_value_by_name(SqliteRow* row, const char* column_name);

/**
 * Get integer value from row
 * @param row Row handle
 * @param column_index Column index (0-based)
 * @param default_value Default value if NULL or conversion fails
 * @return Integer value
 */
int64_t sqlite_row_get_int(SqliteRow* row, int column_index, int64_t default_value);

/**
 * Get real value from row
 * @param row Row handle
 * @param column_index Column index (0-based)
 * @param default_value Default value if NULL or conversion fails
 * @return Real value
 */
double sqlite_row_get_real(SqliteRow* row, int column_index, double default_value);

/**
 * Get text value from row
 * @param row Row handle
 * @param column_index Column index (0-based)
 * @return Text value or NULL (must not be freed)
 */
const char* sqlite_row_get_text(SqliteRow* row, int column_index);

/**
 * Get blob value from row
 * @param row Row handle
 * @param column_index Column index (0-based)
 * @param size Output parameter for blob size
 * @return Blob data or NULL (must not be freed)
 */
const void* sqlite_row_get_blob(SqliteRow* row, int column_index, size_t* size);

/**
 * Check if row value is NULL
 * @param row Row handle
 * @param column_index Column index (0-based)
 * @return true if NULL, false otherwise
 */
bool sqlite_row_is_null(SqliteRow* row, int column_index);

// ============================================================================
// Transaction Functions
// ============================================================================

/**
 * Begin a transaction
 * @param db Database handle
 * @param type Transaction type
 * @return Transaction handle or NULL on error
 */
SqliteTransaction* sqlite_begin_transaction(SqliteDatabase* db, TransactionType type);

/**
 * Commit a transaction
 * @param transaction Transaction handle
 * @return true on success, false on error
 */
bool sqlite_commit_transaction(SqliteTransaction* transaction);

/**
 * Rollback a transaction
 * @param transaction Transaction handle
 * @return true on success, false on error
 */
bool sqlite_rollback_transaction(SqliteTransaction* transaction);

/**
 * Create a savepoint
 * @param db Database handle
 * @param savepoint_name Savepoint name
 * @return true on success, false on error
 */
bool sqlite_create_savepoint(SqliteDatabase* db, const char* savepoint_name);

/**
 * Release a savepoint
 * @param db Database handle
 * @param savepoint_name Savepoint name
 * @return true on success, false on error
 */
bool sqlite_release_savepoint(SqliteDatabase* db, const char* savepoint_name);

/**
 * Rollback to a savepoint
 * @param db Database handle
 * @param savepoint_name Savepoint name
 * @return true on success, false on error
 */
bool sqlite_rollback_to_savepoint(SqliteDatabase* db, const char* savepoint_name);

// ============================================================================
// Schema Information Functions
// ============================================================================

/**
 * Get list of all tables in database
 * @param db Database handle
 * @param count Output parameter for table count
 * @return Array of table names (must be freed)
 */
char** sqlite_get_table_list(SqliteDatabase* db, int* count);

/**
 * Get table information
 * @param db Database handle
 * @param table_name Table name
 * @return Table information or NULL on error
 */
SqliteTableInfo* sqlite_get_table_info(SqliteDatabase* db, const char* table_name);

/**
 * Free table information structure
 * @param table_info Table information
 */
void sqlite_free_table_info(SqliteTableInfo* table_info);

/**
 * Check if table exists
 * @param db Database handle
 * @param table_name Table name
 * @return true if exists, false otherwise
 */
bool sqlite_table_exists(SqliteDatabase* db, const char* table_name);

/**
 * Get table row count
 * @param db Database handle
 * @param table_name Table name
 * @return Number of rows or -1 on error
 */
int64_t sqlite_get_table_row_count(SqliteDatabase* db, const char* table_name);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Escape SQL string for safe insertion
 * @param input Input string
 * @return Escaped string (must be freed)
 */
char* sqlite_escape_string(const char* input);

/**
 * Get last insert row ID
 * @param db Database handle
 * @return Last inserted row ID
 */
int64_t sqlite_last_insert_rowid(SqliteDatabase* db);

/**
 * Get number of changes from last operation
 * @param db Database handle
 * @return Number of changes
 */
int sqlite_changes(SqliteDatabase* db);

/**
 * Get total number of changes since database opened
 * @param db Database handle
 * @return Total number of changes
 */
int sqlite_total_changes(SqliteDatabase* db);

/**
 * Vacuum database (reclaim unused space)
 * @param db Database handle
 * @return true on success, false on error
 */
bool sqlite_vacuum_database(SqliteDatabase* db);

/**
 * Analyze database (update statistics)
 * @param db Database handle
 * @return true on success, false on error
 */
bool sqlite_analyze_database(SqliteDatabase* db);

/**
 * Check database integrity
 * @param db Database handle
 * @return true if integrity is OK, false otherwise
 */
bool sqlite_check_integrity(SqliteDatabase* db);

/**
 * Create database backup
 * @param source_db Source database
 * @param dest_filename Destination filename
 * @return true on success, false on error
 */
bool sqlite_backup_database(SqliteDatabase* source_db, const char* dest_filename);

/**
 * Restore database from backup
 * @param dest_db Destination database
 * @param source_filename Source backup filename
 * @return true on success, false on error
 */
bool sqlite_restore_database(SqliteDatabase* dest_db, const char* source_filename);

// ============================================================================
// Error Handling Functions
// ============================================================================

/**
 * Get last error message
 * @return Error message string
 */
const char* sqlite_get_last_error(void);

/**
 * Clear last error message
 */
void sqlite_clear_error(void);

/**
 * Get SQLite library version
 * @return Version string
 */
const char* sqlite_get_version(void);

/**
 * Get SQLite utils library version
 * @return Version string
 */
const char* sqlite_utils_get_version(void);

// ============================================================================
// Value Helper Functions
// ============================================================================

/**
 * Create integer value
 * @param value Integer value
 * @return SqliteValue structure (must be freed)
 */
SqliteValue* sqlite_value_create_int(int64_t value);

/**
 * Create real value
 * @param value Real value
 * @return SqliteValue structure (must be freed)
 */
SqliteValue* sqlite_value_create_real(double value);

/**
 * Create text value
 * @param value Text value
 * @return SqliteValue structure (must be freed)
 */
SqliteValue* sqlite_value_create_text(const char* value);

/**
 * Create blob value
 * @param data Blob data
 * @param size Data size
 * @return SqliteValue structure (must be freed)
 */
SqliteValue* sqlite_value_create_blob(const void* data, size_t size);

/**
 * Create NULL value
 * @return SqliteValue structure (must be freed)
 */
SqliteValue* sqlite_value_create_null(void);

/**
 * Free SQLite value
 * @param value Value to free
 */
void sqlite_value_free(SqliteValue* value);

/**
 * Convert value to string representation
 * @param value Value to convert
 * @return String representation (must be freed)
 */
char* sqlite_value_to_string(SqliteValue* value);

#ifdef __cplusplus
}
#endif

#endif // SQLITE_UTILS_H
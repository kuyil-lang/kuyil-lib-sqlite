#include "sqlite_utils.h"
#include <assert.h>

// Internal structures
struct SqliteDatabase {
    sqlite3* db;
    char* filename;
    int flags;
    bool is_open;
};

struct SqliteStatement {
    sqlite3_stmt* stmt;
    SqliteDatabase* db;
    char* sql;
};

struct SqliteResult {
    sqlite3_stmt* stmt;
    int column_count;
    int row_count;
    int current_row;
    char** column_names;
    SqliteValueType* column_types;
    bool has_data;
};

struct SqliteRow {
    SqliteResult* result;
    int row_index;
};

struct SqliteTransaction {
    SqliteDatabase* db;
    TransactionType type;
    bool is_active;
};

// Global error state
static char g_sqlite_error[MAX_ERROR_LENGTH] = {0};

// Helper function to set error message
static void set_sqlite_error(const char* message) {
    snprintf(g_sqlite_error, sizeof(g_sqlite_error), "%s", message);
}

static void set_sqlite_error_from_db(sqlite3* db) {
    const char* err_msg = sqlite3_errmsg(db);
    set_sqlite_error(err_msg ? err_msg : "Unknown SQLite error");
}

// ============================================================================
// Database Management Implementation
// ============================================================================

SqliteDatabase* sqlite_open_database(const char* filename, int flags) {
    if (!filename) {
        set_sqlite_error("Database filename cannot be NULL");
        return NULL;
    }
    
    SqliteDatabase* db = malloc(sizeof(SqliteDatabase));
    if (!db) {
        set_sqlite_error("Memory allocation failed for database handle");
        return NULL;
    }
    
    // Convert our flags to SQLite flags
    int sqlite_flags = 0;
    if (flags & SQLITE_OPEN_READONLY_FLAG) {
        sqlite_flags |= SQLITE_OPEN_READONLY;
    }
    if (flags & SQLITE_OPEN_READWRITE_FLAG) {
        sqlite_flags |= SQLITE_OPEN_READWRITE;
    }
    if (flags & SQLITE_OPEN_CREATE_FLAG) {
        sqlite_flags |= SQLITE_OPEN_CREATE;
    }
    
    // Default to READWRITE | CREATE if no flags specified
    if (sqlite_flags == 0) {
        sqlite_flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    }
    
    int result = sqlite3_open_v2(filename, &db->db, sqlite_flags, NULL);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(db->db);
        sqlite3_close(db->db);
        free(db);
        return NULL;
    }
    
    db->filename = strdup(filename);
    db->flags = flags;
    db->is_open = true;
    
    return db;
}

bool sqlite_close_database(SqliteDatabase* db) {
    if (!db || !db->is_open) {
        set_sqlite_error("Invalid database handle");
        return false;
    }
    
    int result = sqlite3_close(db->db);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(db->db);
        return false;
    }
    
    free(db->filename);
    db->is_open = false;
    free(db);
    
    return true;
}

bool sqlite_is_database_open(SqliteDatabase* db) {
    return db && db->is_open && db->db;
}

const char* sqlite_get_database_path(SqliteDatabase* db) {
    if (!sqlite_is_database_open(db)) {
        return NULL;
    }
    return db->filename;
}

SqliteDatabaseStats* sqlite_get_database_stats(SqliteDatabase* db) {
    if (!sqlite_is_database_open(db)) {
        set_sqlite_error("Database is not open");
        return NULL;
    }
    
    SqliteDatabaseStats* stats = malloc(sizeof(SqliteDatabaseStats));
    if (!stats) {
        set_sqlite_error("Memory allocation failed for database stats");
        return NULL;
    }
    
    // Get various database statistics using PRAGMA statements
    sqlite3_stmt* stmt;
    
    // Page count
    sqlite3_prepare_v2(db->db, "PRAGMA page_count", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->page_count = sqlite3_column_int64(stmt, 0);
    } else {
        stats->page_count = -1;
    }
    sqlite3_finalize(stmt);
    
    // Page size
    sqlite3_prepare_v2(db->db, "PRAGMA page_size", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->page_size = sqlite3_column_int64(stmt, 0);
    } else {
        stats->page_size = -1;
    }
    sqlite3_finalize(stmt);
    
    // Freelist count
    sqlite3_prepare_v2(db->db, "PRAGMA freelist_count", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->freelist_count = sqlite3_column_int64(stmt, 0);
    } else {
        stats->freelist_count = -1;
    }
    sqlite3_finalize(stmt);
    
    // Schema version
    sqlite3_prepare_v2(db->db, "PRAGMA schema_version", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->schema_version = sqlite3_column_int64(stmt, 0);
    } else {
        stats->schema_version = -1;
    }
    sqlite3_finalize(stmt);
    
    // User version
    sqlite3_prepare_v2(db->db, "PRAGMA user_version", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->user_version = sqlite3_column_int64(stmt, 0);
    } else {
        stats->user_version = -1;
    }
    sqlite3_finalize(stmt);
    
    // SQLite version
    stats->sqlite_version = strdup(sqlite3_libversion());
    
    return stats;
}

void sqlite_free_database_stats(SqliteDatabaseStats* stats) {
    if (stats) {
        free(stats->sqlite_version);
        free(stats);
    }
}

bool sqlite_set_foreign_keys(SqliteDatabase* db, bool enable) {
    if (!sqlite_is_database_open(db)) {
        set_sqlite_error("Database is not open");
        return false;
    }
    
    const char* sql = enable ? "PRAGMA foreign_keys = ON" : "PRAGMA foreign_keys = OFF";
    return sqlite_execute_sql(db, sql) >= 0;
}

bool sqlite_set_pragma(SqliteDatabase* db, const char* pragma_name, const char* pragma_value) {
    if (!sqlite_is_database_open(db) || !pragma_name || !pragma_value) {
        set_sqlite_error("Invalid parameters for pragma setting");
        return false;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), "PRAGMA %s = %s", pragma_name, pragma_value);
    
    return sqlite_execute_sql(db, sql) >= 0;
}

char* sqlite_get_pragma(SqliteDatabase* db, const char* pragma_name) {
    if (!sqlite_is_database_open(db) || !pragma_name) {
        set_sqlite_error("Invalid parameters for pragma query");
        return NULL;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), "PRAGMA %s", pragma_name);
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(db->db);
        return NULL;
    }
    
    char* value = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = (const char*)sqlite3_column_text(stmt, 0);
        if (text) {
            value = strdup(text);
        }
    }
    
    sqlite3_finalize(stmt);
    return value;
}

// ============================================================================
// SQL Execution Implementation
// ============================================================================

int sqlite_execute_sql(SqliteDatabase* db, const char* sql) {
    if (!sqlite_is_database_open(db) || !sql) {
        set_sqlite_error("Invalid parameters for SQL execution");
        return -1;
    }
    
    char* error_msg = NULL;
    int result = sqlite3_exec(db->db, sql, NULL, NULL, &error_msg);
    
    if (result != SQLITE_OK) {
        if (error_msg) {
            set_sqlite_error(error_msg);
            sqlite3_free(error_msg);
        } else {
            set_sqlite_error_from_db(db->db);
        }
        return -1;
    }
    
    return sqlite3_changes(db->db);
}

SqliteResult* sqlite_execute_query(SqliteDatabase* db, const char* sql) {
    if (!sqlite_is_database_open(db) || !sql) {
        set_sqlite_error("Invalid parameters for query execution");
        return NULL;
    }
    
    sqlite3_stmt* stmt;
    int result = sqlite3_prepare_v2(db->db, sql, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(db->db);
        return NULL;
    }
    
    SqliteResult* query_result = malloc(sizeof(SqliteResult));
    if (!query_result) {
        sqlite3_finalize(stmt);
        set_sqlite_error("Memory allocation failed for query result");
        return NULL;
    }
    
    query_result->stmt = stmt;
    query_result->column_count = sqlite3_column_count(stmt);
    query_result->row_count = 0;
    query_result->current_row = -1;
    query_result->has_data = false;
    
    // Allocate arrays for column information
    query_result->column_names = malloc(query_result->column_count * sizeof(char*));
    query_result->column_types = malloc(query_result->column_count * sizeof(SqliteValueType));
    
    if (!query_result->column_names || !query_result->column_types) {
        sqlite3_finalize(stmt);
        free(query_result->column_names);
        free(query_result->column_types);
        free(query_result);
        set_sqlite_error("Memory allocation failed for column information");
        return NULL;
    }
    
    // Store column names and types
    for (int i = 0; i < query_result->column_count; i++) {
        query_result->column_names[i] = strdup(sqlite3_column_name(stmt, i));
        query_result->column_types[i] = SQLITE_TYPE_NULL; // Will be determined when reading rows
    }
    
    // Check if there's at least one row
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        query_result->has_data = true;
        query_result->current_row = 0;
        query_result->row_count = 1; // We'll count more as we iterate
        
        // Determine column types from first row
        for (int i = 0; i < query_result->column_count; i++) {
            int sqlite_type = sqlite3_column_type(stmt, i);
            switch (sqlite_type) {
                case SQLITE_INTEGER:
                    query_result->column_types[i] = SQLITE_TYPE_INTEGER;
                    break;
                case SQLITE_FLOAT:
                    query_result->column_types[i] = SQLITE_TYPE_REAL;
                    break;
                case SQLITE_TEXT:
                    query_result->column_types[i] = SQLITE_TYPE_TEXT;
                    break;
                case SQLITE_BLOB:
                    query_result->column_types[i] = SQLITE_TYPE_BLOB;
                    break;
                default:
                    query_result->column_types[i] = SQLITE_TYPE_NULL;
                    break;
            }
        }
    } else {
        // Reset to beginning for iteration
        sqlite3_reset(stmt);
    }
    
    return query_result;
}

SqliteStatement* sqlite_prepare_statement(SqliteDatabase* db, const char* sql) {
    if (!sqlite_is_database_open(db) || !sql) {
        set_sqlite_error("Invalid parameters for statement preparation");
        return NULL;
    }
    
    SqliteStatement* stmt_handle = malloc(sizeof(SqliteStatement));
    if (!stmt_handle) {
        set_sqlite_error("Memory allocation failed for statement handle");
        return NULL;
    }
    
    int result = sqlite3_prepare_v2(db->db, sql, -1, &stmt_handle->stmt, NULL);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(db->db);
        free(stmt_handle);
        return NULL;
    }
    
    stmt_handle->db = db;
    stmt_handle->sql = strdup(sql);
    
    return stmt_handle;
}

SqliteResult* sqlite_execute_statement(SqliteStatement* stmt) {
    if (!stmt || !stmt->stmt) {
        set_sqlite_error("Invalid statement handle");
        return NULL;
    }
    
    // Create result from the prepared statement (similar to execute_query)
    SqliteResult* result = malloc(sizeof(SqliteResult));
    if (!result) {
        set_sqlite_error("Memory allocation failed for statement result");
        return NULL;
    }
    
    result->stmt = stmt->stmt;
    result->column_count = sqlite3_column_count(stmt->stmt);
    result->row_count = 0;
    result->current_row = -1;
    result->has_data = false;
    
    // Allocate column information arrays
    result->column_names = malloc(result->column_count * sizeof(char*));
    result->column_types = malloc(result->column_count * sizeof(SqliteValueType));
    
    if (!result->column_names || !result->column_types) {
        free(result->column_names);
        free(result->column_types);
        free(result);
        set_sqlite_error("Memory allocation failed for column information");
        return NULL;
    }
    
    // Store column information
    for (int i = 0; i < result->column_count; i++) {
        result->column_names[i] = strdup(sqlite3_column_name(stmt->stmt, i));
        result->column_types[i] = SQLITE_TYPE_NULL;
    }
    
    // Check for first row
    if (sqlite3_step(stmt->stmt) == SQLITE_ROW) {
        result->has_data = true;
        result->current_row = 0;
        result->row_count = 1;
        
        // Determine column types
        for (int i = 0; i < result->column_count; i++) {
            int sqlite_type = sqlite3_column_type(stmt->stmt, i);
            switch (sqlite_type) {
                case SQLITE_INTEGER:
                    result->column_types[i] = SQLITE_TYPE_INTEGER;
                    break;
                case SQLITE_FLOAT:
                    result->column_types[i] = SQLITE_TYPE_REAL;
                    break;
                case SQLITE_TEXT:
                    result->column_types[i] = SQLITE_TYPE_TEXT;
                    break;
                case SQLITE_BLOB:
                    result->column_types[i] = SQLITE_TYPE_BLOB;
                    break;
                default:
                    result->column_types[i] = SQLITE_TYPE_NULL;
                    break;
            }
        }
    }
    
    return result;
}

void sqlite_finalize_statement(SqliteStatement* stmt) {
    if (stmt) {
        if (stmt->stmt) {
            sqlite3_finalize(stmt->stmt);
        }
        free(stmt->sql);
        free(stmt);
    }
}

bool sqlite_reset_statement(SqliteStatement* stmt) {
    if (!stmt || !stmt->stmt) {
        set_sqlite_error("Invalid statement handle");
        return false;
    }
    
    int result = sqlite3_reset(stmt->stmt);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(stmt->db->db);
        return false;
    }
    
    return true;
}

// ============================================================================
// Parameter Binding Implementation
// ============================================================================

bool sqlite_bind_int(SqliteStatement* stmt, int index, int64_t value) {
    if (!stmt || !stmt->stmt) {
        set_sqlite_error("Invalid statement handle");
        return false;
    }
    
    int result = sqlite3_bind_int64(stmt->stmt, index, value);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(stmt->db->db);
        return false;
    }
    
    return true;
}

bool sqlite_bind_real(SqliteStatement* stmt, int index, double value) {
    if (!stmt || !stmt->stmt) {
        set_sqlite_error("Invalid statement handle");
        return false;
    }
    
    int result = sqlite3_bind_double(stmt->stmt, index, value);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(stmt->db->db);
        return false;
    }
    
    return true;
}

bool sqlite_bind_text(SqliteStatement* stmt, int index, const char* value) {
    if (!stmt || !stmt->stmt) {
        set_sqlite_error("Invalid statement handle");
        return false;
    }
    
    int result = sqlite3_bind_text(stmt->stmt, index, value, -1, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(stmt->db->db);
        return false;
    }
    
    return true;
}

bool sqlite_bind_blob(SqliteStatement* stmt, int index, const void* data, size_t size) {
    if (!stmt || !stmt->stmt) {
        set_sqlite_error("Invalid statement handle");
        return false;
    }
    
    int result = sqlite3_bind_blob(stmt->stmt, index, data, size, SQLITE_TRANSIENT);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(stmt->db->db);
        return false;
    }
    
    return true;
}

bool sqlite_bind_null(SqliteStatement* stmt, int index) {
    if (!stmt || !stmt->stmt) {
        set_sqlite_error("Invalid statement handle");
        return false;
    }
    
    int result = sqlite3_bind_null(stmt->stmt, index);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(stmt->db->db);
        return false;
    }
    
    return true;
}

bool sqlite_clear_bindings(SqliteStatement* stmt) {
    if (!stmt || !stmt->stmt) {
        set_sqlite_error("Invalid statement handle");
        return false;
    }
    
    int result = sqlite3_clear_bindings(stmt->stmt);
    if (result != SQLITE_OK) {
        set_sqlite_error_from_db(stmt->db->db);
        return false;
    }
    
    return true;
}

// ============================================================================
// Result Set Implementation
// ============================================================================

int sqlite_result_row_count(SqliteResult* result) {
    return result ? result->row_count : 0;
}

int sqlite_result_column_count(SqliteResult* result) {
    return result ? result->column_count : 0;
}

const char* sqlite_result_column_name(SqliteResult* result, int column_index) {
    if (!result || column_index < 0 || column_index >= result->column_count) {
        return NULL;
    }
    return result->column_names[column_index];
}

SqliteValueType sqlite_result_column_type(SqliteResult* result, int column_index) {
    if (!result || column_index < 0 || column_index >= result->column_count) {
        return SQLITE_TYPE_NULL;
    }
    return result->column_types[column_index];
}

SqliteRow* sqlite_result_first_row(SqliteResult* result) {
    if (!result || !result->has_data) {
        return NULL;
    }
    
    sqlite3_reset(result->stmt);
    if (sqlite3_step(result->stmt) == SQLITE_ROW) {
        result->current_row = 0;
        
        SqliteRow* row = malloc(sizeof(SqliteRow));
        if (!row) {
            set_sqlite_error("Memory allocation failed for row");
            return NULL;
        }
        
        row->result = result;
        row->row_index = 0;
        return row;
    }
    
    return NULL;
}

SqliteRow* sqlite_result_next_row(SqliteResult* result) {
    if (!result || !result->has_data) {
        return NULL;
    }
    
    if (sqlite3_step(result->stmt) == SQLITE_ROW) {
        result->current_row++;
        
        SqliteRow* row = malloc(sizeof(SqliteRow));
        if (!row) {
            set_sqlite_error("Memory allocation failed for row");
            return NULL;
        }
        
        row->result = result;
        row->row_index = result->current_row;
        return row;
    }
    
    return NULL;
}

void sqlite_result_reset(SqliteResult* result) {
    if (result && result->stmt) {
        sqlite3_reset(result->stmt);
        result->current_row = -1;
    }
}

void sqlite_free_result(SqliteResult* result) {
    if (result) {
        if (result->column_names) {
            for (int i = 0; i < result->column_count; i++) {
                free(result->column_names[i]);
            }
            free(result->column_names);
        }
        free(result->column_types);
        // Note: We don't finalize stmt here as it might be owned by a SqliteStatement
        free(result);
    }
}

// ============================================================================
// Row Access Implementation  
// ============================================================================

SqliteValue* sqlite_row_get_value(SqliteRow* row, int column_index) {
    if (!row || !row->result || column_index < 0 || column_index >= row->result->column_count) {
        set_sqlite_error("Invalid row or column index");
        return NULL;
    }
    
    SqliteValue* value = malloc(sizeof(SqliteValue));
    if (!value) {
        set_sqlite_error("Memory allocation failed for value");
        return NULL;
    }
    
    sqlite3_stmt* stmt = row->result->stmt;
    int sqlite_type = sqlite3_column_type(stmt, column_index);
    
    switch (sqlite_type) {
        case SQLITE_INTEGER:
            value->type = SQLITE_TYPE_INTEGER;
            value->value.integer_val = sqlite3_column_int64(stmt, column_index);
            break;
            
        case SQLITE_FLOAT:
            value->type = SQLITE_TYPE_REAL;
            value->value.real_val = sqlite3_column_double(stmt, column_index);
            break;
            
        case SQLITE_TEXT: {
            value->type = SQLITE_TYPE_TEXT;
            const char* text = (const char*)sqlite3_column_text(stmt, column_index);
            value->value.text_val = text ? strdup(text) : NULL;
            break;
        }
        
        case SQLITE_BLOB: {
            value->type = SQLITE_TYPE_BLOB;
            const void* blob_data = sqlite3_column_blob(stmt, column_index);
            int blob_size = sqlite3_column_bytes(stmt, column_index);
            
            if (blob_data && blob_size > 0) {
                value->value.blob_val.data = malloc(blob_size);
                if (value->value.blob_val.data) {
                    memcpy(value->value.blob_val.data, blob_data, blob_size);
                    value->value.blob_val.size = blob_size;
                } else {
                    value->value.blob_val.data = NULL;
                    value->value.blob_val.size = 0;
                }
            } else {
                value->value.blob_val.data = NULL;
                value->value.blob_val.size = 0;
            }
            break;
        }
        
        default:
            value->type = SQLITE_TYPE_NULL;
            break;
    }
    
    return value;
}

SqliteValue* sqlite_row_get_value_by_name(SqliteRow* row, const char* column_name) {
    if (!row || !row->result || !column_name) {
        set_sqlite_error("Invalid row or column name");
        return NULL;
    }
    
    // Find column index by name
    for (int i = 0; i < row->result->column_count; i++) {
        if (strcmp(row->result->column_names[i], column_name) == 0) {
            return sqlite_row_get_value(row, i);
        }
    }
    
    set_sqlite_error("Column name not found");
    return NULL;
}

int64_t sqlite_row_get_int(SqliteRow* row, int column_index, int64_t default_value) {
    SqliteValue* value = sqlite_row_get_value(row, column_index);
    if (!value) {
        return default_value;
    }
    
    int64_t result = default_value;
    
    switch (value->type) {
        case SQLITE_TYPE_INTEGER:
            result = value->value.integer_val;
            break;
        case SQLITE_TYPE_REAL:
            result = (int64_t)value->value.real_val;
            break;
        case SQLITE_TYPE_TEXT:
            if (value->value.text_val) {
                result = strtoll(value->value.text_val, NULL, 10);
            }
            break;
        default:
            break;
    }
    
    sqlite_value_free(value);
    return result;
}

double sqlite_row_get_real(SqliteRow* row, int column_index, double default_value) {
    SqliteValue* value = sqlite_row_get_value(row, column_index);
    if (!value) {
        return default_value;
    }
    
    double result = default_value;
    
    switch (value->type) {
        case SQLITE_TYPE_INTEGER:
            result = (double)value->value.integer_val;
            break;
        case SQLITE_TYPE_REAL:
            result = value->value.real_val;
            break;
        case SQLITE_TYPE_TEXT:
            if (value->value.text_val) {
                result = strtod(value->value.text_val, NULL);
            }
            break;
        default:
            break;
    }
    
    sqlite_value_free(value);
    return result;
}

const char* sqlite_row_get_text(SqliteRow* row, int column_index) {
    if (!row || !row->result || column_index < 0 || column_index >= row->result->column_count) {
        return NULL;
    }
    
    return (const char*)sqlite3_column_text(row->result->stmt, column_index);
}

const void* sqlite_row_get_blob(SqliteRow* row, int column_index, size_t* size) {
    if (!row || !row->result || column_index < 0 || column_index >= row->result->column_count) {
        if (size) *size = 0;
        return NULL;
    }
    
    const void* data = sqlite3_column_blob(row->result->stmt, column_index);
    if (size) {
        *size = sqlite3_column_bytes(row->result->stmt, column_index);
    }
    
    return data;
}

bool sqlite_row_is_null(SqliteRow* row, int column_index) {
    if (!row || !row->result || column_index < 0 || column_index >= row->result->column_count) {
        return true;
    }
    
    return sqlite3_column_type(row->result->stmt, column_index) == SQLITE_NULL;
}

// ============================================================================
// Transaction Implementation
// ============================================================================

SqliteTransaction* sqlite_begin_transaction(SqliteDatabase* db, TransactionType type) {
    if (!sqlite_is_database_open(db)) {
        set_sqlite_error("Database is not open");
        return NULL;
    }
    
    const char* sql;
    switch (type) {
        case TRANSACTION_IMMEDIATE:
            sql = "BEGIN IMMEDIATE TRANSACTION";
            break;
        case TRANSACTION_EXCLUSIVE:
            sql = "BEGIN EXCLUSIVE TRANSACTION";
            break;
        default:
            sql = "BEGIN DEFERRED TRANSACTION";
            break;
    }
    
    if (sqlite_execute_sql(db, sql) < 0) {
        return NULL;
    }
    
    SqliteTransaction* transaction = malloc(sizeof(SqliteTransaction));
    if (!transaction) {
        sqlite_execute_sql(db, "ROLLBACK");
        set_sqlite_error("Memory allocation failed for transaction");
        return NULL;
    }
    
    transaction->db = db;
    transaction->type = type;
    transaction->is_active = true;
    
    return transaction;
}

bool sqlite_commit_transaction(SqliteTransaction* transaction) {
    if (!transaction || !transaction->is_active) {
        set_sqlite_error("Invalid or inactive transaction");
        return false;
    }
    
    bool success = sqlite_execute_sql(transaction->db, "COMMIT") >= 0;
    transaction->is_active = false;
    free(transaction);
    
    return success;
}

bool sqlite_rollback_transaction(SqliteTransaction* transaction) {
    if (!transaction || !transaction->is_active) {
        set_sqlite_error("Invalid or inactive transaction");
        return false;
    }
    
    bool success = sqlite_execute_sql(transaction->db, "ROLLBACK") >= 0;
    transaction->is_active = false;
    free(transaction);
    
    return success;
}

bool sqlite_create_savepoint(SqliteDatabase* db, const char* savepoint_name) {
    if (!sqlite_is_database_open(db) || !savepoint_name) {
        set_sqlite_error("Invalid parameters for savepoint creation");
        return false;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), "SAVEPOINT %s", savepoint_name);
    
    return sqlite_execute_sql(db, sql) >= 0;
}

bool sqlite_release_savepoint(SqliteDatabase* db, const char* savepoint_name) {
    if (!sqlite_is_database_open(db) || !savepoint_name) {
        set_sqlite_error("Invalid parameters for savepoint release");
        return false;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), "RELEASE SAVEPOINT %s", savepoint_name);
    
    return sqlite_execute_sql(db, sql) >= 0;
}

bool sqlite_rollback_to_savepoint(SqliteDatabase* db, const char* savepoint_name) {
    if (!sqlite_is_database_open(db) || !savepoint_name) {
        set_sqlite_error("Invalid parameters for savepoint rollback");
        return false;
    }
    
    char sql[256];
    snprintf(sql, sizeof(sql), "ROLLBACK TO SAVEPOINT %s", savepoint_name);
    
    return sqlite_execute_sql(db, sql) >= 0;
}

// ============================================================================
// Schema Information Implementation
// ============================================================================

char** sqlite_get_table_list(SqliteDatabase* db, int* count) {
    if (!sqlite_is_database_open(db) || !count) {
        set_sqlite_error("Invalid parameters for table list");
        return NULL;
    }
    
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name";
    SqliteResult* result = sqlite_execute_query(db, sql);
    if (!result) {
        return NULL;
    }
    
    // Count rows first
    *count = 0;
    SqliteRow* row = sqlite_result_first_row(result);
    while (row) {
        (*count)++;
        free(row);
        row = sqlite_result_next_row(result);
    }
    
    if (*count == 0) {
        sqlite_free_result(result);
        return NULL;
    }
    
    // Allocate array for table names
    char** tables = malloc(*count * sizeof(char*));
    if (!tables) {
        sqlite_free_result(result);
        set_sqlite_error("Memory allocation failed for table list");
        return NULL;
    }
    
    // Reset and fill array
    sqlite_result_reset(result);
    int index = 0;
    row = sqlite_result_first_row(result);
    while (row && index < *count) {
        const char* table_name = sqlite_row_get_text(row, 0);
        tables[index] = table_name ? strdup(table_name) : NULL;
        index++;
        free(row);
        row = sqlite_result_next_row(result);
    }
    
    sqlite_free_result(result);
    return tables;
}

SqliteTableInfo* sqlite_get_table_info(SqliteDatabase* db, const char* table_name) {
    if (!sqlite_is_database_open(db) || !table_name) {
        set_sqlite_error("Invalid parameters for table info");
        return NULL;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), "PRAGMA table_info(%s)", table_name);
    
    SqliteResult* result = sqlite_execute_query(db, sql);
    if (!result) {
        return NULL;
    }
    
    // Count columns
    int column_count = 0;
    SqliteRow* row = sqlite_result_first_row(result);
    while (row) {
        column_count++;
        free(row);
        row = sqlite_result_next_row(result);
    }
    
    if (column_count == 0) {
        sqlite_free_result(result);
        set_sqlite_error("Table not found or has no columns");
        return NULL;
    }
    
    SqliteTableInfo* table_info = malloc(sizeof(SqliteTableInfo));
    if (!table_info) {
        sqlite_free_result(result);
        set_sqlite_error("Memory allocation failed for table info");
        return NULL;
    }
    
    strncpy(table_info->name, table_name, MAX_TABLE_NAME_LENGTH - 1);
    table_info->name[MAX_TABLE_NAME_LENGTH - 1] = '\0';
    table_info->column_count = column_count;
    table_info->columns = malloc(column_count * sizeof(SqliteColumnInfo));
    
    if (!table_info->columns) {
        sqlite_free_result(result);
        free(table_info);
        set_sqlite_error("Memory allocation failed for column info");
        return NULL;
    }
    
    // Reset and populate column information
    sqlite_result_reset(result);
    int index = 0;
    row = sqlite_result_first_row(result);
    while (row && index < column_count) {
        // Column structure: cid, name, type, notnull, dflt_value, pk
        const char* col_name = sqlite_row_get_text(row, 1);
        const char* col_type = sqlite_row_get_text(row, 2);
        int64_t not_null = sqlite_row_get_int(row, 3, 0);
        const char* default_val = sqlite_row_get_text(row, 4);
        int64_t primary_key = sqlite_row_get_int(row, 5, 0);
        
        if (col_name) {
            strncpy(table_info->columns[index].name, col_name, MAX_COLUMN_NAME_LENGTH - 1);
            table_info->columns[index].name[MAX_COLUMN_NAME_LENGTH - 1] = '\0';
        }
        
        // Convert SQLite type name to our enum
        table_info->columns[index].type = SQLITE_TYPE_TEXT; // Default
        if (col_type) {
            if (strstr(col_type, "INT")) {
                table_info->columns[index].type = SQLITE_TYPE_INTEGER;
            } else if (strstr(col_type, "REAL") || strstr(col_type, "FLOAT") || strstr(col_type, "DOUBLE")) {
                table_info->columns[index].type = SQLITE_TYPE_REAL;
            } else if (strstr(col_type, "BLOB")) {
                table_info->columns[index].type = SQLITE_TYPE_BLOB;
            }
        }
        
        table_info->columns[index].not_null = (not_null != 0);
        table_info->columns[index].primary_key = (primary_key != 0);
        table_info->columns[index].default_value = default_val ? strdup(default_val) : NULL;
        
        index++;
        free(row);
        row = sqlite_result_next_row(result);
    }
    
    sqlite_free_result(result);
    
    // Get row count
    table_info->row_count = sqlite_get_table_row_count(db, table_name);
    
    return table_info;
}

void sqlite_free_table_info(SqliteTableInfo* table_info) {
    if (table_info) {
        if (table_info->columns) {
            for (int i = 0; i < table_info->column_count; i++) {
                free(table_info->columns[i].default_value);
            }
            free(table_info->columns);
        }
        free(table_info);
    }
}

bool sqlite_table_exists(SqliteDatabase* db, const char* table_name) {
    if (!sqlite_is_database_open(db) || !table_name) {
        return false;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), 
            "SELECT name FROM sqlite_master WHERE type='table' AND name='%s'", 
            table_name);
    
    SqliteResult* result = sqlite_execute_query(db, sql);
    if (!result) {
        return false;
    }
    
    bool exists = (sqlite_result_first_row(result) != NULL);
    sqlite_free_result(result);
    
    return exists;
}

int64_t sqlite_get_table_row_count(SqliteDatabase* db, const char* table_name) {
    if (!sqlite_is_database_open(db) || !table_name) {
        return -1;
    }
    
    char sql[512];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM \"%s\"", table_name);
    
    SqliteResult* result = sqlite_execute_query(db, sql);
    if (!result) {
        return -1;
    }
    
    int64_t count = -1;
    SqliteRow* row = sqlite_result_first_row(result);
    if (row) {
        count = sqlite_row_get_int(row, 0, -1);
        free(row);
    }
    
    sqlite_free_result(result);
    return count;
}

// ============================================================================
// Utility Implementation
// ============================================================================

char* sqlite_escape_string(const char* input) {
    if (!input) {
        return NULL;
    }
    
    size_t len = strlen(input);
    size_t escaped_len = len * 2 + 1; // Worst case: every char needs escaping
    char* escaped = malloc(escaped_len);
    if (!escaped) {
        set_sqlite_error("Memory allocation failed for string escaping");
        return NULL;
    }
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\'') {
            escaped[j++] = '\'';
            escaped[j++] = '\'';
        } else {
            escaped[j++] = input[i];
        }
    }
    escaped[j] = '\0';
    
    // Resize to actual size
    char* result = realloc(escaped, j + 1);
    return result ? result : escaped;
}

int64_t sqlite_last_insert_rowid(SqliteDatabase* db) {
    if (!sqlite_is_database_open(db)) {
        return -1;
    }
    
    return sqlite3_last_insert_rowid(db->db);
}

int sqlite_changes(SqliteDatabase* db) {
    if (!sqlite_is_database_open(db)) {
        return -1;
    }
    
    return sqlite3_changes(db->db);
}

int sqlite_total_changes(SqliteDatabase* db) {
    if (!sqlite_is_database_open(db)) {
        return -1;
    }
    
    return sqlite3_total_changes(db->db);
}

bool sqlite_vacuum_database(SqliteDatabase* db) {
    if (!sqlite_is_database_open(db)) {
        set_sqlite_error("Database is not open");
        return false;
    }
    
    return sqlite_execute_sql(db, "VACUUM") >= 0;
}

bool sqlite_analyze_database(SqliteDatabase* db) {
    if (!sqlite_is_database_open(db)) {
        set_sqlite_error("Database is not open");
        return false;
    }
    
    return sqlite_execute_sql(db, "ANALYZE") >= 0;
}

bool sqlite_check_integrity(SqliteDatabase* db) {
    if (!sqlite_is_database_open(db)) {
        set_sqlite_error("Database is not open");
        return false;
    }
    
    SqliteResult* result = sqlite_execute_query(db, "PRAGMA integrity_check");
    if (!result) {
        return false;
    }
    
    bool is_ok = false;
    SqliteRow* row = sqlite_result_first_row(result);
    if (row) {
        const char* result_text = sqlite_row_get_text(row, 0);
        is_ok = (result_text && strcmp(result_text, "ok") == 0);
        free(row);
    }
    
    sqlite_free_result(result);
    return is_ok;
}

bool sqlite_backup_database(SqliteDatabase* source_db, const char* dest_filename) {
    if (!sqlite_is_database_open(source_db) || !dest_filename) {
        set_sqlite_error("Invalid parameters for database backup");
        return false;
    }
    
    sqlite3* dest_db;
    int result = sqlite3_open(dest_filename, &dest_db);
    if (result != SQLITE_OK) {
        set_sqlite_error("Failed to create backup database");
        return false;
    }
    
    sqlite3_backup* backup = sqlite3_backup_init(dest_db, "main", source_db->db, "main");
    if (!backup) {
        sqlite3_close(dest_db);
        set_sqlite_error("Failed to initialize backup");
        return false;
    }
    
    result = sqlite3_backup_step(backup, -1);
    bool success = (result == SQLITE_DONE);
    
    sqlite3_backup_finish(backup);
    sqlite3_close(dest_db);
    
    if (!success) {
        set_sqlite_error("Backup operation failed");
    }
    
    return success;
}

bool sqlite_restore_database(SqliteDatabase* dest_db, const char* source_filename) {
    if (!sqlite_is_database_open(dest_db) || !source_filename) {
        set_sqlite_error("Invalid parameters for database restore");
        return false;
    }
    
    sqlite3* source_db;
    int result = sqlite3_open_v2(source_filename, &source_db, SQLITE_OPEN_READONLY, NULL);
    if (result != SQLITE_OK) {
        set_sqlite_error("Failed to open source database");
        return false;
    }
    
    sqlite3_backup* backup = sqlite3_backup_init(dest_db->db, "main", source_db, "main");
    if (!backup) {
        sqlite3_close(source_db);
        set_sqlite_error("Failed to initialize restore");
        return false;
    }
    
    result = sqlite3_backup_step(backup, -1);
    bool success = (result == SQLITE_DONE);
    
    sqlite3_backup_finish(backup);
    sqlite3_close(source_db);
    
    if (!success) {
        set_sqlite_error("Restore operation failed");
    }
    
    return success;
}

// ============================================================================
// Error Handling Implementation
// ============================================================================

const char* sqlite_get_last_error(void) {
    return g_sqlite_error;
}

void sqlite_clear_error(void) {
    g_sqlite_error[0] = '\0';
}

const char* sqlite_get_version(void) {
    return sqlite3_libversion();
}

const char* sqlite_utils_get_version(void) {
    return SQLITE_UTILS_VERSION_STRING;
}

// ============================================================================
// Value Helper Implementation
// ============================================================================

SqliteValue* sqlite_value_create_int(int64_t value) {
    SqliteValue* val = malloc(sizeof(SqliteValue));
    if (!val) {
        set_sqlite_error("Memory allocation failed for integer value");
        return NULL;
    }
    
    val->type = SQLITE_TYPE_INTEGER;
    val->value.integer_val = value;
    
    return val;
}

SqliteValue* sqlite_value_create_real(double value) {
    SqliteValue* val = malloc(sizeof(SqliteValue));
    if (!val) {
        set_sqlite_error("Memory allocation failed for real value");
        return NULL;
    }
    
    val->type = SQLITE_TYPE_REAL;
    val->value.real_val = value;
    
    return val;
}

SqliteValue* sqlite_value_create_text(const char* value) {
    SqliteValue* val = malloc(sizeof(SqliteValue));
    if (!val) {
        set_sqlite_error("Memory allocation failed for text value");
        return NULL;
    }
    
    val->type = SQLITE_TYPE_TEXT;
    val->value.text_val = value ? strdup(value) : NULL;
    
    return val;
}

SqliteValue* sqlite_value_create_blob(const void* data, size_t size) {
    SqliteValue* val = malloc(sizeof(SqliteValue));
    if (!val) {
        set_sqlite_error("Memory allocation failed for blob value");
        return NULL;
    }
    
    val->type = SQLITE_TYPE_BLOB;
    
    if (data && size > 0) {
        val->value.blob_val.data = malloc(size);
        if (val->value.blob_val.data) {
            memcpy(val->value.blob_val.data, data, size);
            val->value.blob_val.size = size;
        } else {
            val->value.blob_val.data = NULL;
            val->value.blob_val.size = 0;
        }
    } else {
        val->value.blob_val.data = NULL;
        val->value.blob_val.size = 0;
    }
    
    return val;
}

SqliteValue* sqlite_value_create_null(void) {
    SqliteValue* val = malloc(sizeof(SqliteValue));
    if (!val) {
        set_sqlite_error("Memory allocation failed for null value");
        return NULL;
    }
    
    val->type = SQLITE_TYPE_NULL;
    
    return val;
}

void sqlite_value_free(SqliteValue* value) {
    if (value) {
        if (value->type == SQLITE_TYPE_TEXT && value->value.text_val) {
            free(value->value.text_val);
        } else if (value->type == SQLITE_TYPE_BLOB && value->value.blob_val.data) {
            free(value->value.blob_val.data);
        }
        free(value);
    }
}

char* sqlite_value_to_string(SqliteValue* value) {
    if (!value) {
        return strdup("NULL");
    }
    
    char* result = NULL;
    
    switch (value->type) {
        case SQLITE_TYPE_INTEGER: {
            result = malloc(32);
            if (result) {
                snprintf(result, 32, "%lld", (long long)value->value.integer_val);
            }
            break;
        }
        
        case SQLITE_TYPE_REAL: {
            result = malloc(32);
            if (result) {
                snprintf(result, 32, "%.15g", value->value.real_val);
            }
            break;
        }
        
        case SQLITE_TYPE_TEXT:
            result = value->value.text_val ? strdup(value->value.text_val) : strdup("");
            break;
            
        case SQLITE_TYPE_BLOB: {
            size_t hex_len = value->value.blob_val.size * 2 + 1;
            result = malloc(hex_len);
            if (result) {
                for (size_t i = 0; i < value->value.blob_val.size; i++) {
                    sprintf(result + i * 2, "%02x", 
                           ((unsigned char*)value->value.blob_val.data)[i]);
                }
            }
            break;
        }
        
        default:
            result = strdup("NULL");
            break;
    }
    
    return result ? result : strdup("NULL");
}
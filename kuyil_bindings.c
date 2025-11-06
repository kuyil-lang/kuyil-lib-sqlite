/* Kuyil bindings for SQLite - direct bindings without dlopen wrapper */
#include "../../src/ast.h"
#include "sqlite_utils.h"
#include <stdio.h>
#include <stdlib.h>

// Kuyil interface signature metadata
__attribute__((visibility("default")))
const char* kyl_interface_signature_text = 
    "sqlite openDatabase(path: string, flags: int32) -> int32\n"
    "sqlite closeDatabase(handle: int32) -> int32\n"
    "sqlite executeSql(handle: int32, sql: string) -> int32\n"
    "sqlite executeQuery(handle: int32, sql: string) -> int32\n"
    "result firstRow(handle: int32) -> int32\n"
    "result nextRow(handle: int32) -> int32\n"
    "row getInt(rowHandle: int32, colIndex: int32) -> int32\n"
    "row getText(rowHandle: int32, colIndex: int32) -> string\n"
    "row getReal(rowHandle: int32, colIndex: int32) -> float64\n"
    "sqlite freeResult(handle: int32) -> int32\n"
    "sqlite getLastError(handle: int32) -> string\n";

/* Pointer registry to convert between Kuyil numbers and C pointers */
#define MAX_POINTERS 1024
static void* g_pointer_registry[MAX_POINTERS];
static int g_next_handle = 1;

static int register_pointer(void* ptr) {
    if (g_next_handle >= MAX_POINTERS) {
        fprintf(stderr, "Pointer registry full!\n");
        return 0;
    }
    int handle = g_next_handle++;
    g_pointer_registry[handle] = ptr;
    return handle;
}

static void* get_pointer(int handle) {
    if (handle <= 0 || handle >= MAX_POINTERS) {
        return NULL;
    }
    return g_pointer_registry[handle];
}

static void unregister_pointer(int handle) {
    if (handle > 0 && handle < MAX_POINTERS) {
        g_pointer_registry[handle] = NULL;
    }
}

/* Kuyil wrapper: sqlite_open_database(filename, flags) -> database handle */
Value kyl_sqlite_open_database(int arg_count, Value* args) {
    if (arg_count < 2 || args[0].type != VALUE_STRING || args[1].type != VALUE_NUMBER) {
        fprintf(stderr, "sqlite_open_database requires (string filename, int flags)\n");
        Value result = {VALUE_NIL};
        return result;
    }
    
    const char* filename = args[0].as.string;
    int flags = (int)args[1].as.number;
    
    SqliteDatabase* db = sqlite_open_database(filename, flags);
    int handle = db ? register_pointer(db) : 0;
    
    Value result;
    result.type = VALUE_NUMBER;
    result.as.number = (double)handle;
    return result;
}

/* Kuyil wrapper: sqlite_close_database(db) */
Value kyl_sqlite_close_database(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_NUMBER) {
        Value result = {VALUE_NIL};
        return result;
    }
    
    int handle = (int)args[0].as.number;
    SqliteDatabase* db = (SqliteDatabase*)get_pointer(handle);
    if (db) {
        sqlite_close_database(db);
        unregister_pointer(handle);
    }
    
    Value result = {VALUE_NIL};
    return result;
}

/* Kuyil wrapper: sqlite_execute_sql(db, sql) -> success (1/0) */
Value kyl_sqlite_execute_sql(int arg_count, Value* args) {
    if (arg_count < 2 || args[0].type != VALUE_NUMBER || args[1].type != VALUE_STRING) {
        Value result = {VALUE_NUMBER};
        result.as.number = 0;
        return result;
    }
    
    int handle = (int)args[0].as.number;
    const char* sql = args[1].as.string;
    SqliteDatabase* db = (SqliteDatabase*)get_pointer(handle);
    
    int success = db ? sqlite_execute_sql(db, sql) : 0;
    
    Value result;
    result.type = VALUE_NUMBER;
    result.as.number = (double)success;
    return result;
}

/* Kuyil wrapper: sqlite_execute_query(db, sql) -> result handle */
Value kyl_sqlite_execute_query(int arg_count, Value* args) {
    if (arg_count < 2 || args[0].type != VALUE_NUMBER || args[1].type != VALUE_STRING) {
        Value result = {VALUE_NIL};
        return result;
    }
    
    int db_handle = (int)args[0].as.number;
    const char* sql = args[1].as.string;
    SqliteDatabase* db = (SqliteDatabase*)get_pointer(db_handle);
    
    SqliteResult* result_obj = db ? sqlite_execute_query(db, sql) : NULL;
    int result_handle = result_obj ? register_pointer(result_obj) : 0;
    
    Value result;
    result.type = VALUE_NUMBER;
    result.as.number = (double)result_handle;
    return result;
}

/* Kuyil wrapper: sqlite_result_first_row(result) -> row handle */
Value kyl_sqlite_result_first_row(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_NUMBER) {
        Value result = {VALUE_NIL};
        return result;
    }
    
    int result_handle = (int)args[0].as.number;
    SqliteResult* result_obj = (SqliteResult*)get_pointer(result_handle);
    
    SqliteRow* row = result_obj ? sqlite_result_first_row(result_obj) : NULL;
    
    /* Cast pointer directly to avoid registry exhaustion - rows are ephemeral */
    Value result;
    result.type = VALUE_NUMBER;
    result.as.number = (double)(uintptr_t)row;
    return result;
}

/* Kuyil wrapper: sqlite_result_next_row(result) -> row handle */
Value kyl_sqlite_result_next_row(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_NUMBER) {
        Value result = {VALUE_NIL};
        return result;
    }
    
    int result_handle = (int)args[0].as.number;
    SqliteResult* result_obj = (SqliteResult*)get_pointer(result_handle);
    
    SqliteRow* row = result_obj ? sqlite_result_next_row(result_obj) : NULL;
    
    /* Cast pointer directly to avoid registry exhaustion - rows are ephemeral */
    Value result;
    result.type = VALUE_NUMBER;
    result.as.number = (double)(uintptr_t)row;
    return result;
}

/* Kuyil wrapper: sqlite_row_get_int(row, column_index) -> integer */
Value kyl_sqlite_row_get_int(int arg_count, Value* args) {
    if (arg_count < 2 || args[0].type != VALUE_NUMBER || args[1].type != VALUE_NUMBER) {
        Value result = {VALUE_NIL};
        return result;
    }
    
    /* Cast pointer back from number - rows are not in registry */
    SqliteRow* row = (SqliteRow*)(uintptr_t)args[0].as.number;
    int column_index = (int)args[1].as.number;
    
    int64_t value = row ? sqlite_row_get_int(row, column_index, 0) : 0;
    
    Value result;
    result.type = VALUE_NUMBER;
    result.as.number = (double)value;
    return result;
}

/* Kuyil wrapper: sqlite_row_get_text(row, column_index) -> string */
Value kyl_sqlite_row_get_text(int arg_count, Value* args) {
    if (arg_count < 2 || args[0].type != VALUE_NUMBER || args[1].type != VALUE_NUMBER) {
        Value result = {VALUE_NIL};
        return result;
    }
    
    /* Cast pointer back from number - rows are not in registry */
    SqliteRow* row = (SqliteRow*)(uintptr_t)args[0].as.number;
    int column_index = (int)args[1].as.number;
    
    const char* text = row ? sqlite_row_get_text(row, column_index) : NULL;
    
    Value result;
    if (text) {
        result.type = VALUE_STRING;
        result.as.string = (char*)text;  // Note: sqlite_utils.h should document lifetime
    } else {
        result.type = VALUE_NIL;
    }
    return result;
}

/* Kuyil wrapper: sqlite_row_get_real(row, column_index) -> real number */
Value kyl_sqlite_row_get_real(int arg_count, Value* args) {
    if (arg_count < 2 || args[0].type != VALUE_NUMBER || args[1].type != VALUE_NUMBER) {
        Value result = {VALUE_NIL};
        return result;
    }
    
    /* Cast pointer back from number - rows are not in registry */
    SqliteRow* row = (SqliteRow*)(uintptr_t)args[0].as.number;
    int column_index = (int)args[1].as.number;
    
    double value = row ? sqlite_row_get_real(row, column_index, 0.0) : 0.0;
    
    Value result;
    result.type = VALUE_NUMBER;
    result.as.number = value;
    return result;
}

/* Kuyil wrapper: sqlite_free_result(result) */
Value kyl_sqlite_free_result(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_NUMBER) {
        Value result = {VALUE_NIL};
        return result;
    }
    
    int result_handle = (int)args[0].as.number;
    SqliteResult* result_obj = (SqliteResult*)get_pointer(result_handle);
    
    if (result_obj) {
        sqlite_free_result(result_obj);
        unregister_pointer(result_handle);
    }
    
    Value result = {VALUE_NIL};
    return result;
}

/* Kuyil wrapper: sqlite_get_last_error() -> error string */
Value kyl_sqlite_get_last_error(int arg_count, Value* args) {
    (void)arg_count;
    (void)args;
    
    const char* error = sqlite_get_last_error();
    
    Value result;
    if (error) {
        result.type = VALUE_STRING;
        result.as.string = (char*)error;
    } else {
        result.type = VALUE_NIL;
    }
    return result;
}

// LowerCamel aliases for cleaner interface
Value openDatabase(int arg_count, Value* args) { return kyl_sqlite_open_database(arg_count, args); }
Value closeDatabase(int arg_count, Value* args) { return kyl_sqlite_close_database(arg_count, args); }
Value executeSql(int arg_count, Value* args) { return kyl_sqlite_execute_sql(arg_count, args); }
Value executeQuery(int arg_count, Value* args) { return kyl_sqlite_execute_query(arg_count, args); }
Value firstRow(int arg_count, Value* args) { return kyl_sqlite_result_first_row(arg_count, args); }
Value nextRow(int arg_count, Value* args) { return kyl_sqlite_result_next_row(arg_count, args); }
Value getInt(int arg_count, Value* args) { return kyl_sqlite_row_get_int(arg_count, args); }
Value getText(int arg_count, Value* args) { return kyl_sqlite_row_get_text(arg_count, args); }
Value getReal(int arg_count, Value* args) { return kyl_sqlite_row_get_real(arg_count, args); }
Value freeResult(int arg_count, Value* args) { return kyl_sqlite_free_result(arg_count, args); }
Value getLastError(int arg_count, Value* args) { return kyl_sqlite_get_last_error(arg_count, args); }

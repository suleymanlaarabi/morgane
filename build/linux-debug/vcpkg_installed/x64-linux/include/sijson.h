#ifndef SIJSON_H
#define SIJSON_H

/* This generated file contains includes for project dependencies. */
#include <sijson/export.h>
#include "sireflect.h"
#include <stdbool.h>
#include <stddef.h>

/*
 * sijson is a tiny JSON serialization layer built on top of sireflect.
 *
 * Public usage is intentionally macro based:
 *
 *     SIJSON(Position, {
 *         float x;
 *         float y;
 *     });
 *
 *     char *json = sijson_to_json(Position, { .x = 1, .y = 2 });
 *     Position pos = sijson_from_json(Position, json);
 *
 * Strings returned by sijson_to_json are owned by the caller.
 * Values returned by sijson_from_json are copied out of an internal temporary
 * buffer before the expression ends. Heap-owned fields inside that value, such
 * as char *, must be released with sijson_free(type, &value).
 */

/* Internal reflected type handle symbol for a user type. */
#define sijson_handle(type) __sijson__##type##__handle

/*
 * Declare a JSON-reflectable type in a header.
 *
 * Pair this with SIJSON_DEFINE(type) in exactly one source file.
 */
#define SIJSON_DECLARE(type, ...)                                                                  \
    SIREFLECT_STRUCT(type, __VA_ARGS__);                                                           \
    extern sireflect_handle_t sijson_handle(type);

/* Define storage for a type declared with SIJSON_DECLARE. */
#define SIJSON_DEFINE(type) sireflect_handle_t sijson_handle(type) = 0;

/*
 * Declare and define a JSON-reflectable type in the current translation unit.
 *
 * This is the simplest form and is ideal for examples, tests, and small
 * programs.
 */
#define SIJSON(type, ...)                                                                          \
    SIJSON_DECLARE(type, __VA_ARGS__);                                                             \
    SIJSON_DEFINE(type);


/*
 * Opaque handle for arbitrary JSON values.
 *
 * A sijson_value_t can hold null, bool, number, string, array, or object.
 * Values are allocated in sijson's internal arena unless documented otherwise.
 * They stay valid until sijson_clean() or sijson_release().
 */
typedef struct sijson_value *sijson_value_t;

typedef enum sijson_type {
    SIJSON_NULL,
    SIJSON_BOOL,
    SIJSON_NUMBER,
    SIJSON_STRING,
    SIJSON_ARRAY,
    SIJSON_OBJECT,
} sijson_type_t;

/*
 * Parse/stringify dynamic JSON without a reflected C type.
 *
 * sijson_parse returns a value allocated in sijson's internal arena.
 * sijson_stringify returns a newly allocated JSON string owned by the
 * caller.
 */
SIJSON_API sijson_value_t sijson_parse(const char *json);
SIJSON_API char *sijson_stringify(sijson_value_t value);

/*
 * Reset or release the internal arena used by sijson_value_t values.
 * sijson_clean keeps allocated capacity for reuse.
 * sijson_release frees the arena storage.
 */
SIJSON_API void sijson_clean(void);
SIJSON_API void sijson_release(void);

/* Inspect a dynamic JSON value. */
SIJSON_API sijson_type_t sijson_type(sijson_value_t value);

/* Read scalar values. The value must have the matching sijson_type_t. */
SIJSON_API bool sijson_bool(sijson_value_t value);
SIJSON_API double sijson_number(sijson_value_t value);
SIJSON_API const char *sijson_string(sijson_value_t value);

/* Read arrays. */
SIJSON_API size_t sijson_array_len(sijson_value_t value);
SIJSON_API sijson_value_t sijson_array_get(sijson_value_t value, size_t index);

/* Read objects. */
SIJSON_API size_t sijson_object_len(sijson_value_t value);
SIJSON_API const char *sijson_object_key(sijson_value_t value, size_t index);
SIJSON_API sijson_value_t sijson_object_get(sijson_value_t value, const char *key);

/* Build dynamic JSON values in sijson's internal arena. */
SIJSON_API sijson_value_t sijson_make_null(void);
SIJSON_API sijson_value_t sijson_make_bool(bool value);
SIJSON_API sijson_value_t sijson_make_number(double value);
SIJSON_API sijson_value_t sijson_make_string(const char *value);
SIJSON_API sijson_value_t sijson_make_array(void);
SIJSON_API sijson_value_t sijson_make_object(void);

/* Mutate arrays and objects created as dynamic JSON values. */
SIJSON_API bool sijson_array_push(sijson_value_t array, sijson_value_t value);
SIJSON_API bool sijson_object_set(sijson_value_t object, const char *key, sijson_value_t value);

/*
 * Serialize a value written as a compound initializer.
 *
 * Example:
 *
 *     char *json = sijson_to_json(Position, { .x = 1, .y = 2 });
 *
 *     Position pos = { .x = 1, .y = 2 };
 *     char *json2 = sijson_to_json_ptr(Position, &pos);
 */
#define sijson_to_json(type, ...)                                                                  \
    sijson_to_json_impl(&sijson_handle(type), &sireflect_desc(type), &(type)__VA_ARGS__)

#define sijson_to_json_ptr(type, ptr)                                                              \
    sijson_to_json_impl(&sijson_handle(type), &sireflect_desc(type), (ptr))

/*
 * Low-level serialization entry point.
 *
 * Registers desc on first use through ref, then serializes ptr.
 * Returns a newly allocated JSON string owned by the caller.
 */
SIJSON_API char *
sijson_to_json_impl(sireflect_handle_t *ref, const sireflect_struct_desc_t *desc, const void *ptr);

/*
 * Deserialize JSON into a value of name.
 *
 * Example:
 *
 *     Position pos = sijson_from_json(Position, json);
 *
 * The returned value is copied from an internal temporary buffer. A later
 * sijson_from_json call may reuse that buffer, but previously copied structs
 * remain valid as long as they do not contain pointers into sijson memory.
 */
#define sijson_from_json(type, json)                                                               \
    *((type *)sijson_from_json_impl(&sijson_handle(type), &sireflect_desc(type), json))

/*
 * Low-level deserialization entry point.
 *
 * Registers desc on first use through ref, grows the internal temporary buffer
 * if needed, writes the parsed value into it, and returns that buffer.
 */
SIJSON_API void *sijson_from_json_impl(
    sireflect_handle_t *ref,
    const sireflect_struct_desc_t *desc,
    const char *json
);

/*
 * Free heap-owned fields inside a value produced by sijson_from_json.
 *
 * This does not free the struct pointer itself.
 */
#define sijson_free(type, ptr) sijson_free_impl(&sijson_handle(type), &sireflect_desc(type), (ptr))

SIJSON_API void sijson_free_impl(sireflect_handle_t *ref, const sireflect_struct_desc_t *desc, void *ptr);

/*
 * Returns the error message from the last failed operation.
 */
SIJSON_API const char *sijson_error(void);

#endif

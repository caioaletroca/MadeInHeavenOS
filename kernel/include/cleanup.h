#ifndef _CLEANUP_H
#define _CLEANUP_H

/*
 * Scope-based guards on top of GCC's cleanup attribute: the release runs
 * whenever the guard variable leaves scope (end of block, return, break, goto
 * out), like C#'s `using`.
 *
 * A guard named `name` is defined next to the resource it guards as:
 *   name##_guard_t                               saved state
 *   name##_guard_t name##_guard_init(args...)    acquire, return the state
 *   void name##_guard_exit(name##_guard_t *)     release
 *
 * Do not jump into the scope of a guard with goto, and keep explicit
 * acquire/release where the release is deliberately skipped or moved.
 */

/**
 * @brief Concatenate two tokens after expanding them (so __COUNTER__ becomes a number).
 */
#define CONCAT_(a, b) a##b
#define CONCAT(a, b) CONCAT_(a, b)

/**
 * @brief Declare guard variable number `id` of type `name##_guard_t`.
 */
#define guard_(name, id, ...)         \
    name##_guard_t CONCAT(guard_, id) \
        __attribute__((cleanup(name##_guard_exit), unused)) = name##_guard_init(__VA_ARGS__)

/**
 * @brief Acquire `name` now and release it at the end of the enclosing scope.
 *
 * Like C#'s `using var`: `guard(spinlock, &p->lock);`
 *
 * @param name Guard name (see the convention above).
 * @param ... Arguments for `name##_guard_init`.
 */
#define guard(name, ...) guard_(name, __COUNTER__, __VA_ARGS__)

/**
 * @brief Loop that runs its body once with guard number `id` held.
 *
 * `done` is declared as a pointer to the guard type so both fit one declaration.
 */
#define scoped_guard_(name, id, ...)                              \
    for (                                                         \
        guard_(name, id, __VA_ARGS__), *CONCAT(done_, id) = NULL; \
        !CONCAT(done_, id);                                       \
        CONCAT(done_, id) = (void *)1)

/**
 * @brief Acquire `name` for the following statement or block only.
 *
 * Like C#'s `lock (...) { }`: `scoped_guard(spinlock, &p->lock) { ... }`
 * The body runs inside a hidden `for`: `break` and `continue` leave the
 * guarded block, not an enclosing loop. The guard is released either way.
 *
 * @param name Guard name (see the convention above).
 * @param ... Arguments for `name##_guard_init`.
 */
#define scoped_guard(name, ...) scoped_guard_(name, __COUNTER__, __VA_ARGS__)

/**
 * @brief Call fn(&variable) when the variable leaves scope, however it leaves
 * (end of block, return). Like guard(), for a value that is not a lock:
 *   vfs_node_t *node release_with(vfs_node_put_ref) = NULL;
 *
 * @param fn Takes a pointer to the variable.
 */
#define release_with(fn) __attribute__((cleanup(fn)))

/**
 * @brief Run a call that returns -errno on failure; on a negative result, return
 * it from the enclosing function (Rust's `?`). Otherwise the macro's value is the
 * call's, so a 0/1 answer can be tested: if (!return_on_error(find(...))).
 *
 * Only in functions where every release is a guard or release_with: a hidden
 * return must not skip an unlock or a put.
 *
 * @param call An expression of type int (or long).
 */
#define return_on_error(call) \
    ({                        \
        int _result = (call); \
        if (_result < 0)      \
            return _result;   \
        _result;              \
    })

#endif // _CLEANUP_H

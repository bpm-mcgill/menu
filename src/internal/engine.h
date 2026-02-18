#ifndef ENGINE_H
#define ENGINE_H

// Logging
#define LOG_INFO(...)  do { printf("[INFO] " __VA_ARGS__); printf("\n"); } while(0)
#define LOG_WARN(...)  do { printf("\033[33m[WARN]\033[0m " __VA_ARGS__); printf("\n"); } while(0)
#define LOG_ERROR(...) do { fprintf(stderr, "\033[31m[ERROR]\033[0m " __VA_ARGS__); fprintf(stderr, "\n"); } while(0)

// Branch prediction helpers
#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

// Assert
#ifdef DEBUG_BUILD
    #include <stdio.h>
    #include <stdlib.h>
    
    #if defined(__GNUC__) || defined(__clang__)
        #define DEBUG_TRAP() __builtin_trap()
    #else
        #define DEBUG_TRAP() abort()
    #endif

    #define ENGINE_ASSERT(condition, message) \
        do { \
            if (!(condition)) { \
                fprintf(stderr, "\n[FATAL ASSERTION FAILED]\n"); \
                fprintf(stderr, "File: %s\nLine: %d\n", __FILE__, __LINE__); \
                fprintf(stderr, "Condition: %s\n", #condition); \
                fprintf(stderr, "Message: %s\n\n", message); \
                DEBUG_TRAP(); \
            } \
        } while(0)

#else
    #if defined(__GNUC__) || defined(__clang__)
        #define ENGINE_ASSERT(condition, message) \
            do { if (!(condition)) __builtin_unreachable(); } while(0)
    #else
        // Fallback: strip it entirely but prevent "unused variable" warnings
        #define ENGINE_ASSERT(condition, message) do { (void)sizeof(condition); } while(0)
    #endif
#endif

#endif // !ENGINE_H

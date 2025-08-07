#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

#include <QDebug>
#include <QString>
#include <stdexcept>

// ========================================
// DEBUG CONTROL - ENABLE/DISABLE ALL DEBUG OUTPUT
// ========================================
// Uncomment the line below to ENABLE debug output
#define ENABLE_DEBUG_WRAPPERS

// Comment out the line above to DISABLE all debug output
// This will make all debug macros into no-ops for production builds

#ifdef ENABLE_DEBUG_WRAPPERS
    #define DEBUG_ENABLED 1
#else
    #define DEBUG_ENABLED 0
#endif

// ========================================
// BASIC DEBUG MACROS
// ========================================
#if DEBUG_ENABLED
    #define DEBUG_FUNC() qDebug() << Q_FUNC_INFO << "- ENTRY"
    #define DEBUG_FUNC_EXIT() qDebug() << Q_FUNC_INFO << "- EXIT"
    #define DEBUG_LINE() qDebug() << Q_FUNC_INFO << "- Line" << __LINE__
    #define DEBUG_MSG(msg) qDebug() << Q_FUNC_INFO << "-" << msg
    #define DEBUG_VAR(var) qDebug() << Q_FUNC_INFO << "-" << #var << "=" << var
#else
    #define DEBUG_FUNC() do {} while(0)
    #define DEBUG_FUNC_EXIT() do {} while(0)
    #define DEBUG_LINE() do {} while(0)
    #define DEBUG_MSG(msg) do {} while(0)
    #define DEBUG_VAR(var) do {} while(0)
#endif

// ========================================
// ADVANCED DEBUG MACROS
// ========================================
#if DEBUG_ENABLED
    // Try-catch wrapper for debugging crashes
    #define DEBUG_TRY_BLOCK(code) \
        try { \
            qDebug() << Q_FUNC_INFO << "- Executing:" << #code; \
            code; \
            qDebug() << Q_FUNC_INFO << "- Success:" << #code; \
        } catch (const std::exception& e) { \
            qCritical() << Q_FUNC_INFO << "- Exception in" << #code << ":" << e.what(); \
            throw; \
        } catch (...) { \
            qCritical() << Q_FUNC_INFO << "- Unknown exception in" << #code; \
            throw; \
        }

    // Safe execution wrapper that doesn't crash on exceptions
    #define DEBUG_SAFE_EXECUTE(code) \
        try { \
            qDebug() << Q_FUNC_INFO << "- Safely executing:" << #code; \
            code; \
            qDebug() << Q_FUNC_INFO << "- Safe success:" << #code; \
        } catch (const std::exception& e) { \
            qCritical() << Q_FUNC_INFO << "- Safe exception in" << #code << ":" << e.what(); \
        } catch (...) { \
            qCritical() << Q_FUNC_INFO << "- Safe unknown exception in" << #code; \
        }
#else
    #define DEBUG_TRY_BLOCK(code) do { code; } while(0)
    #define DEBUG_SAFE_EXECUTE(code) do { code; } while(0)
#endif

// ========================================
// FUNCTION DEBUG WRAPPERS
// ========================================
#if DEBUG_ENABLED
    // Function wrapper for complete function debugging
    #define DEBUG_FUNCTION_WRAPPER(func_name) \
        class Debug##func_name##Wrapper { \
        public: \
            Debug##func_name##Wrapper() { \
                qDebug() << Q_FUNC_INFO << "- ENTERING:" << #func_name; \
            } \
            ~Debug##func_name##Wrapper() { \
                qDebug() << Q_FUNC_INFO << "- EXITING:" << #func_name; \
            } \
        }; \
        Debug##func_name##Wrapper debug_wrapper_##func_name;

    // Simplified function entry/exit wrapper
    #define DEBUG_WRAP_FUNCTION() \
        qDebug() << Q_FUNC_INFO << "- FUNCTION ENTRY"; \
        struct FunctionExitLogger { \
            const char* func_info; \
            FunctionExitLogger(const char* f) : func_info(f) {} \
            ~FunctionExitLogger() { qDebug() << func_info << "- FUNCTION EXIT"; } \
        } exit_logger(Q_FUNC_INFO);
#else
    #define DEBUG_FUNCTION_WRAPPER(func_name) do {} while(0)
    #define DEBUG_WRAP_FUNCTION() do {} while(0)
#endif

// ========================================
// OBJECT CREATION AND METHOD CALL WRAPPERS
// ========================================
#if DEBUG_ENABLED
    // Object creation wrapper
    #define DEBUG_CREATE_OBJECT(obj_type, obj_name, ...) \
        qDebug() << Q_FUNC_INFO << "- Creating" << #obj_type << #obj_name; \
        obj_type* obj_name = nullptr; \
        try { \
            obj_name = new obj_type(__VA_ARGS__); \
            qDebug() << Q_FUNC_INFO << "- Successfully created" << #obj_type << #obj_name; \
        } catch (const std::exception& e) { \
            qCritical() << Q_FUNC_INFO << "- Failed to create" << #obj_type << #obj_name << ":" << e.what(); \
            throw; \
        } catch (...) { \
            qCritical() << Q_FUNC_INFO << "- Failed to create" << #obj_type << #obj_name << ": unknown exception"; \
            throw; \
        }

    // Qt-specific object creation wrapper
    #define DEBUG_CREATE_QT_OBJECT(obj_type, obj_name, parent) \
        qDebug() << Q_FUNC_INFO << "- Creating Qt object" << #obj_type << #obj_name; \
        obj_type* obj_name = nullptr; \
        try { \
            obj_name = new obj_type(parent); \
            qDebug() << Q_FUNC_INFO << "- Successfully created Qt object" << #obj_type << #obj_name; \
        } catch (const std::exception& e) { \
            qCritical() << Q_FUNC_INFO << "- Failed to create Qt object" << #obj_type << #obj_name << ":" << e.what(); \
            throw; \
        } catch (...) { \
            qCritical() << Q_FUNC_INFO << "- Failed to create Qt object" << #obj_type << #obj_name << ": unknown exception"; \
            throw; \
        }

    // Method call wrapper
    #define DEBUG_CALL_METHOD(obj, method, ...) \
        try { \
            qDebug() << Q_FUNC_INFO << "- Calling" << #obj << "->" << #method; \
            obj->method(__VA_ARGS__); \
            qDebug() << Q_FUNC_INFO << "- Successfully called" << #obj << "->" << #method; \
        } catch (const std::exception& e) { \
            qCritical() << Q_FUNC_INFO << "- Failed calling" << #obj << "->" << #method << ":" << e.what(); \
            throw; \
        } catch (...) { \
            qCritical() << Q_FUNC_INFO << "- Failed calling" << #obj << "->" << #method << ": unknown exception"; \
            throw; \
        }
#else
    // When debug is disabled, these macros do the operation without logging
    #define DEBUG_CREATE_OBJECT(obj_type, obj_name, ...) \
        obj_type* obj_name = new obj_type(__VA_ARGS__);
    
    #define DEBUG_CREATE_QT_OBJECT(obj_type, obj_name, parent) \
        obj_type* obj_name = new obj_type(parent);
    
    #define DEBUG_CALL_METHOD(obj, method, ...) \
        obj->method(__VA_ARGS__);
#endif

// ========================================
// USAGE INSTRUCTIONS
// ========================================
/* 
TO ENABLE DEBUG OUTPUT:
    Uncomment: #define ENABLE_DEBUG_WRAPPERS (line 12)

TO DISABLE DEBUG OUTPUT:
    Comment out: // #define ENABLE_DEBUG_WRAPPERS (line 12)

AVAILABLE MACROS:
    DEBUG_FUNC() - Log function entry
    DEBUG_FUNC_EXIT() - Log function exit
    DEBUG_MSG(message) - Log a message
    DEBUG_VAR(variable) - Log variable name and value
    DEBUG_SAFE_EXECUTE(code) - Execute code with exception handling
    DEBUG_WRAP_FUNCTION() - Auto log function entry/exit
    DEBUG_CREATE_QT_OBJECT(Type, name, parent) - Create Qt object with logging

EXAMPLE USAGE:
    void MyClass::myFunction() {
        DEBUG_WRAP_FUNCTION();
        DEBUG_MSG("Starting operation");
        DEBUG_SAFE_EXECUTE(
            someRiskyOperation();
        );
    }
*/

#endif // DEBUG_UTILS_H
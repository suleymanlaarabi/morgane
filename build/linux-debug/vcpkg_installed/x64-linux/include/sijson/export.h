#pragma once

#if defined(sijson_STATIC)
#    define SIJSON_API
#elif defined(_WIN32)
#    if defined(sijson_EXPORTS)
#        define SIJSON_API __declspec(dllexport)
#    else
#        define SIJSON_API __declspec(dllimport)
#    endif
#elif defined(sijson_EXPORTS)
#    define SIJSON_API __attribute__((visibility("default")))
#else
#    define SIJSON_API
#endif

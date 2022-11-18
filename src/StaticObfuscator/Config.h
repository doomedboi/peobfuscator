#pragma once

#if defined(_MSC_VER)
    
    #ifndef COMPILER_MSVC
        #define COMPILER_MSVC 1
    #endif

    #define OBFUSCATOR_API

#elif defined (__GNUC__)
    #define COMPILER_GCC
    #define OBFUSCATOR_API
#else
    #error "Unsupported compiler!"
#endif
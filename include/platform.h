#pragma once

// Platform-specific includes and write abstraction
#ifdef _WIN32
    // Windows
    #include <io.h>
    #define sage_write(fd, buf, count) _write(fd, buf, (unsigned int)(count))

    #ifndef STDIN_FILENO
        #define STDIN_FILENO  0
        #define STDOUT_FILENO 1
        #define STDERR_FILENO 2
    #endif
#else
    // POSIX (Linux, macOS, BSD, etc.)
    #include <unistd.h>
    #define sage_write(fd, buf, count) write(fd, buf, count)
#endif

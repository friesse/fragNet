#ifndef LOGGER_H
#define LOGGER_H
#include <iostream>
#include <cstdarg>

#define RESET       "\x1B[0m"
#define BLACK       "\x1B[30m"              /* Black */
#define RED         "\x1B[31m"              /* Red */
#define GREEN       "\x1B[32m"              /* Green */
#define YELLOW      "\x1B[33m"              /* Yellow */
#define BLUE        "\x1B[34m"              /* Blue */
#define MAGENTA     "\x1B[35m"              /* Magenta */
#define CYAN        "\x1B[36m"              /* Cyan */
#define WHITE       "\x1B[37m"              /* White */
#define BOLDBLACK   "\x1B[1m\x1B[30m"       /* Bold Black */
#define BOLDRED     "\x1B[1m\x1B[31m"       /* Bold Red */
#define BOLDGREEN   "\x1B[1m\x1B[32m"       /* Bold Green */
#define BOLDYELLOW  "\x1B[1m\x1B[33m"       /* Bold Yellow */
#define BOLDBLUE    "\x1B[1m\x1B[34m"       /* Bold Blue */
#define BOLDMAGENTA "\x1B[1m\x1B[35m"       /* Bold Magenta */
#define BOLDCYAN    "\x1B[1m\x1B[36m"       /* Bold Cyan */
#define BOLDWHITE   "\x1B[1m\x1B[37m"       /* Bold White */

namespace logger {
    extern bool colors_disabled;
    void disable_colors();
    void mkdir_logs();
    const char* get_time_str();
    std::string get_date_str();
    std::string get_log_file_path();
    std::string get_error_file_path();
    
    void info(const char* format, ...);
    void warning(const char* format, ...);
    void error(const char* format, ...);
}

#endif
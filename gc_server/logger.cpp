#include "logger.hpp"
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <sys/stat.h>
#include <string>

namespace logger {
    bool colors_disabled = false;
    
    void disable_colors() {
        colors_disabled = true;
    }
    
    // helpers
    void mkdir_logs() {
        #ifdef _WIN32
        mkdir("logs");
        #else
        mkdir("logs", 0755);
        #endif
    }
    
    const char* get_time_str() {
        static char time_str[9];
        time_t now = time(nullptr);
        struct tm* tm_info = localtime(&now);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
        return time_str;
    }
    
    std::string get_date_str() {
        char date_str[11];
        time_t now = time(nullptr);
        struct tm* tm_info = localtime(&now);
        strftime(date_str, sizeof(date_str), "%d-%m-%Y", tm_info);
        return std::string(date_str);
    }
    
    std::string get_log_file_path() {
        return "logs/log_" + get_date_str() + "_gcserver.txt";
    }
    
    std::string get_error_file_path() {
        return "logs/error_" + get_date_str() + "_gcserver.txt";
    }
    


    void info(const char* format, ...) {
        va_list ap;
        char buffer[4096];
        va_start(ap, format);
        vsnprintf(buffer, sizeof(buffer), format, ap);
        va_end(ap);
        
        mkdir_logs();
        
        // terminal output
        if (colors_disabled) {
            printf("[GC] [%s] [Info] %s\n", get_time_str(), buffer);
        } else {
            printf(CYAN "[GC] [%s] [Info] %s" RESET "\n", get_time_str(), buffer);
        }
        
        // log_.txt
        FILE* log_file = fopen(get_log_file_path().c_str(), "a");
        if (log_file) {
            fprintf(log_file, "[GC] [%s] [Info] %s\n", get_time_str(), buffer);
            fclose(log_file);
        }
    }
    
    void warning(const char* format, ...) {
        va_list ap;
        char buffer[4096];
        va_start(ap, format);
        vsnprintf(buffer, sizeof(buffer), format, ap);
        va_end(ap);
        
        mkdir_logs();
        
        // terminal output
        if (colors_disabled) {
            printf("[GC] [%s] [Warning] %s\n", get_time_str(), buffer);
        } else {
            printf(YELLOW "[GC] [%s] [Warning] %s" RESET "\n", get_time_str(), buffer);
        }
        
        // log_.txt
        FILE* log_file = fopen(get_log_file_path().c_str(), "a");
        if (log_file) {
            fprintf(log_file, "[GC] [%s] [Warning] %s\n", get_time_str(), buffer);
            fclose(log_file);
        }
        
        // error_.txt
        FILE* error_file = fopen(get_error_file_path().c_str(), "a");
        if (error_file) {
            fprintf(error_file, "[GC] [%s] [Warning] %s\n", get_time_str(), buffer);
            fclose(error_file);
        }
    }
    
    void error(const char* format, ...) {
        va_list ap;
        char buffer[4096];
        va_start(ap, format);
        vsnprintf(buffer, sizeof(buffer), format, ap);
        va_end(ap);
        
        mkdir_logs();
        
        // terminal output
        if (colors_disabled) {
            printf("[GC] [%s] [Error] %s\n", get_time_str(), buffer);
        } else {
            printf(RED "[GC] [%s] [Error] %s" RESET "\n", get_time_str(), buffer);
        }
        
        // log_.txt
        FILE* log_file = fopen(get_log_file_path().c_str(), "a");
        if (log_file) {
            fprintf(log_file, "[GC] [%s] [Error] %s\n", get_time_str(), buffer);
            fclose(log_file);
        }
        
        // error_.txt
        FILE* error_file = fopen(get_error_file_path().c_str(), "a");
        if (error_file) {
            fprintf(error_file, "[GC] [%s] [Error] %s\n", get_time_str(), buffer);
            fclose(error_file);
        }
    }
}
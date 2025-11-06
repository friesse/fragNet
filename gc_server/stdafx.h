#pragma once

#include <assert.h>

#include <array>
#include <charconv>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace logger {
	void disable_colors();
	void info(const char* format, ...);
	void warning(const char* format, ...);
	void error(const char* format, ...);
}
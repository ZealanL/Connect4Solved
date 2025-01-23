#pragma once

#include <stdint.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <queue>
#include <deque>
#include <stack>
#include <cassert>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <list>
#include <functional>
#include <chrono>
#include <filesystem>
#include <random>
#include <mutex>
#include <bit>
#include <thread>
#include <cstring>
#include <array>
#include <bitset>
#include <numeric>

#ifdef _MSC_VER
// Disable annoying truncation warnings on MSVC
#pragma warning(disable: 4305 4244 4267)
#endif

#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)

#define CLAMP(val, min, max) MIN(MAX(val, min), max)

#define STR(s) ([&]{ std::stringstream __macroStream; __macroStream << s; return __macroStream.str(); }())

#define LOG(s) { std::cout << STR(s) << std::endl; }

#define LOG_BLANK() LOG("")

// Returns sign of number (1 if positive, -1 if negative, and 0 if 0)
#define SGN(val) ((val > 0) - (val < 0))

#define WARN(s) LOG("WARNING: " << s)

#define ERR_CLOSE(s) { \
	std::string _errorStr = STR(std::string(35, '-') << std::endl << "FATAL ERROR: " << s); \
	LOG(_errorStr); \
	throw std::runtime_error(_errorStr); \
	exit(EXIT_FAILURE); \
}

// Release-mode assertion
#define RASSERT(cond, msg) { if (!(cond)) { ERR_CLOSE("Condition failed: \"" #cond "\": " << msg); } } 

#ifndef NDEBUG 
#define ASSERT RASSERT
#else
#define ASSERT(cond, msg) {}
#endif

static_assert(sizeof(void*) == 8, "Only 64-bit compilation is supported due to the use of 64-bit bitboards");
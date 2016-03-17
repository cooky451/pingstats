#pragma once

#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <string>

template <std::size_t N, typename... Args>
std::string formatString(const char(&format)[N], Args&&... args)
{
	auto neededSize = std::snprintf(nullptr, 0, format, args...);

	if (neededSize > 0)
	{
		// Pretty sketchy since it's overwriting the null-character
		// (with another null-character), but I can't see this not working.
		// Making the string bigger would be a waste of space, and also report the wrong .size().
		std::string result(neededSize, char());
		if (static_cast<std::size_t>(std::snprintf(&result[0], result.size() + 1, format, args...)) == result.size())
		{
			return result;
		}
	}

	return std::string();
}

inline void printTimestamp(FILE* f, std::time_t stamp = std::time(nullptr))
{
	auto tm = *std::localtime(&stamp);

	fprintf(f, "%02d-%02d-%02d %02d:%02d:%02d",
		1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);
}

struct FileHandleCloser
{
	void operator () (std::FILE* handle) const
	{
		if (handle != nullptr)
			fclose(handle);
	}
};

typedef std::unique_ptr<std::FILE, FileHandleCloser> FileHandle;

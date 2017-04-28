#pragma once

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace utility // export
{
	namespace literals {}

	using seconds_f32 = std::chrono::duration<float, std::ratio<1, 1>>;
	using seconds_f64 = std::chrono::duration<double, std::ratio<1, 1>>;
	using milliseconds_f32 = std::chrono::duration<float, std::milli>;
	using milliseconds_f64 = std::chrono::duration<double, std::milli>;

	// This function doesn't handle all FP corner cases correctly, 
	// but it is about 20 times faster than std::(l)lround.

	template <typename To, typename From>
	constexpr auto fastround(From value)
	{
		static_assert(std::is_floating_point_v<From>);
		static_assert(std::is_integral_v<To>);

		if (value <= From{ 0.0 } && std::is_signed_v<To>)
		{
			return static_cast<To>(value - From{ 0.5 });
		}
		else
		{
			return static_cast<To>(value + From{ 0.5 });
		}
	}

	template <std::size_t N, typename... Args>
	std::string formatString(const char(&format)[N], Args&&... args)
	{
		const auto size = std::snprintf(nullptr, 0, format, args...);

		if (size < 0)
		{
			throw std::logic_error("Invalid format string.");
		}
		
		// legal since LWG 2475
		std::string result( static_cast<std::size_t>(size) , {});
		std::snprintf(result.data(), 1 + result.size(), format, args...);
		return result;
	}

	inline std::vector<std::string> parseWords(const std::string& source)
	{
		const auto skipSpaces = [](const char* s) { 
			for (; std::isspace(*s); ++s);
			return s;
		};

		const auto skipNonSpaces = [](const char* s) { 
			for (; *s != '\0' && !std::isspace(*s); ++s);
			return s;
		};

		std::vector<std::string> words;

		for (auto begin = source.c_str();; )
		{
			begin = skipSpaces(begin);

			const auto end = skipNonSpaces(begin);

			if (begin != end)
			{
				words.emplace_back(begin, end);
				begin = end;
			}
			else
			{
				break;
			}
		}

		return words;
	}

	inline std::string insertCarriageReturns(const std::string& str)
	{
		std::string ret(str.size() + std::count(str.begin(), str.end(), '\n'), char{});

		for (std::size_t i = 0, j = 0; i < str.size(); ++i, ++j)
		{
			if (str[i] == '\n')
			{
				ret[j++] = '\r';
			}

			ret[j] = str[i];
		}

		return ret;
	}

	// VS's implementation of ifstream is still broken
	// (much slower than fread), so fopen it is.

	struct FcloseType
	{
		void operator () (std::FILE* handle) const
		{
			if (handle != nullptr)
			{
				std::fclose(handle);
			}
		}
	};

	using FileHandle = std::unique_ptr<std::FILE, FcloseType>;

	namespace filesystem = std::experimental::filesystem;

	template <typename Buffer>
	Buffer readFileAs(filesystem::path filePath)
	{
		Buffer buffer;

		std::error_code ec;
		const auto size = filesystem::file_size(filePath, ec);

		if (!ec)
		{
			FileHandle file(std::fopen(filePath.u8string().c_str(), "rb"));

			if (file != nullptr)
			{
				buffer.resize(static_cast<std::size_t>(size));
				std::fread(buffer.data(), 1, buffer.size(), file.get());
			}
		}

		return buffer;
	}

	namespace literals
	{
		using namespace std::literals;

		constexpr auto operator "" _i8(unsigned long long int x)
		{
			return static_cast<std::int8_t>(x);
		}

		constexpr auto operator "" _u8(unsigned long long int x)
		{
			return static_cast<std::uint8_t>(x);
		}

		constexpr auto operator "" _i16(unsigned long long int x)
		{
			return static_cast<std::int16_t>(x);
		}

		constexpr auto operator "" _u16(unsigned long long int x)
		{
			return static_cast<std::uint16_t>(x);
		}

		constexpr auto operator "" _i32(unsigned long long int x)
		{
			return static_cast<std::int32_t>(x);
		}

		constexpr auto operator "" _u32(unsigned long long int x)
		{
			return static_cast<std::uint32_t>(x);
		}

		constexpr auto operator "" _i64(unsigned long long int x)
		{
			return static_cast<std::int64_t>(x);
		}

		constexpr auto operator "" _u64(unsigned long long int x)
		{
			return static_cast<std::uint64_t>(x);
		}

		constexpr auto operator "" _int(unsigned long long int x)
		{
			return static_cast<std::intptr_t>(x);
		}

		constexpr auto operator "" _unt(unsigned long long int x)
		{
			return static_cast<std::uintptr_t>(x);
		}

		constexpr auto operator "" _pdt(unsigned long long int x)
		{
			return static_cast<std::ptrdiff_t>(x);
		}

		constexpr auto operator "" _szt(unsigned long long int x)
		{
			return static_cast<std::size_t>(x);
		}

		constexpr auto operator "" _sf32(long double x)
		{
			return seconds_f32{ static_cast<float>(x) };
		}

		constexpr auto operator "" _sf64(long double x)
		{
			return seconds_f64{ static_cast<double>(x) };
		}

		constexpr auto operator "" _msf32(long double x)
		{
			return milliseconds_f32{ static_cast<float>(x) };
		}

		constexpr auto operator "" _msf64(long double x)
		{
			return milliseconds_f64{ static_cast<double>(x) };
		}
	}
}

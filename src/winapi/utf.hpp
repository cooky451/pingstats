#pragma once

#include "base.hpp"

#include <string>
#include <string_view>

namespace winapi // export
{
	void utf8(
		char* buffer, 
		std::size_t bufferSize,
		const wchar_t* source, 
		std::size_t sourceSize)
	{
		WideCharToMultiByte(
			CP_UTF8, 0,
			source, static_cast<int>(sourceSize),
			buffer, static_cast<int>(bufferSize),
			nullptr, nullptr);
	}

	std::string utf8(const wchar_t* source, std::size_t sourceSize)
	{
		const auto bufferSize{ WideCharToMultiByte(
			CP_UTF8, 0,
			source, static_cast<int>(sourceSize),
			nullptr, 0,
			nullptr, nullptr) };

		std::string buffer(bufferSize, {});

		utf8(buffer.data(), buffer.size(), source, sourceSize);

		return buffer;
	}

	std::string utf8(std::wstring_view source)
	{
		return utf8(source.data(), source.size());
	}

	void wstr(
		wchar_t* buffer, 
		std::size_t bufferSize, 
		const char* source, 
		std::size_t sourceSize)
	{
		MultiByteToWideChar(
			CP_UTF8, 0,
			source, static_cast<int>(sourceSize),
			buffer, static_cast<int>(bufferSize));
	}

	std::wstring wstr(const char* source, std::size_t sourceSize)
	{
		const auto bufferSize{ MultiByteToWideChar(
			CP_UTF8, 0,
			source, static_cast<int>(sourceSize),
			nullptr, 0) };

		std::wstring buffer(bufferSize, {});

		wstr(buffer.data(), buffer.size(), source, sourceSize);

		return buffer;
	}

	std::wstring wstr(std::string_view source)
	{
		return wstr(source.data(), source.size());
	}
}

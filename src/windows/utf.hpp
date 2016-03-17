#pragma once

#include <string>

#include "base.hpp"

std::string toUtf8(const wchar_t* sourceString, std::size_t sourceStringSize)
{
	auto destStringSize = WideCharToMultiByte(CP_UTF8, 0, sourceString, 
			static_cast<int>(sourceStringSize), nullptr, 0, nullptr, nullptr);

	std::string destString(destStringSize, char());

	WideCharToMultiByte(CP_UTF8, 0, sourceString, static_cast<int>(sourceStringSize), 
		&destString[0], destStringSize, nullptr, nullptr);

	return destString;
}

std::string toUtf8(const wchar_t* sourceString)
{
	return toUtf8(sourceString, lstrlenW(sourceString));
}

std::string toUtf8(const std::wstring& sourceString)
{
	return toUtf8(sourceString.c_str(), sourceString.size());
}

std::wstring toWideString(const char* sourceString, std::size_t sourceStringSize)
{
	auto destStringSize = MultiByteToWideChar(0, MB_PRECOMPOSED, sourceString, 
		static_cast<int>(sourceStringSize), nullptr, 0);

	std::wstring destString(destStringSize, wchar_t());

	MultiByteToWideChar(0, MB_PRECOMPOSED, sourceString, 
		static_cast<int>(sourceStringSize), &destString[0], destStringSize);

	return destString;
}

std::wstring toWideString(const char* sourceString)
{
	return toWideString(sourceString, lstrlenA(sourceString));
}

std::wstring toWideString(const std::string& sourceString)
{
	return toWideString(sourceString.c_str(), sourceString.size());
}


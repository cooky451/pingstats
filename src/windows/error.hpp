#pragma once

#include <system_error>

#include "base.hpp"
#include "utf.hpp"

const std::error_category& windowsCategory()
{
	class WindowsErrorCategory : public std::error_category
	{
		const char* name() const noexcept final override
		{
			return "WindowsErrorCategory";
		}

		std::string message(int errorCode) const final override
		{
			wchar_t* errorMessage;

			// Returns number of TCHARs, not bytes.
			const auto errorMessageSize = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM
				| FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
				errorCode, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
				reinterpret_cast<wchar_t*>(&errorMessage), 0, nullptr);

			auto errorString = toUtf8(errorMessage, errorMessageSize);

			return errorString;
		}
	};

	static const WindowsErrorCategory windowsErrorCategory;
	return windowsErrorCategory;
}

class WindowsError : public std::system_error
{
	const std::string _errorMessage;

public:
	WindowsError(const char* functionName, int errorCode = GetLastError())
		: std::system_error(errorCode, windowsCategory())
		, _errorMessage(std::string(functionName) + " failed (" 
			+ std::to_string(errorCode) + "): " + code().message())
	{}

	const char* what() const final override
	{
		return _errorMessage.c_str();
	}
};

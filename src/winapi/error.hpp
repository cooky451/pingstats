#pragma once

#include "base.hpp"
#include "utf.hpp"

#include <cctype>
#include <system_error>

namespace winapi // export
{
	class WindowsError : public std::system_error
	{
		std::string _message;

	public:
		WindowsError(const char* message)
			: WindowsError(GetLastError(), message)
		{}

		WindowsError(int ec, const char* message)
			: system_error(ec, std::system_category(), message)
			, _message(system_error::what())
		{
			// Fixing the currently broken system_error
			// implementation that appends "\r\n" to the message.
			for (std::size_t i{}; i < _message.size(); ++i)
			{
				if (!std::isprint(_message[i]))
				{
					_message.resize(i);
					break;
				}
			}
		}

		const char* what() const final override
		{
			return _message.c_str();
		}
	};
}

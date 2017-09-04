/* 
 * Copyright (c) 2016 - 2017 cooky451
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#pragma once

#include <cstdio>

#include <memory>
#include <filesystem>

namespace utility // export
{
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

		if (!ec && size % sizeof(decltype(buffer[0])) == 0)
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
}

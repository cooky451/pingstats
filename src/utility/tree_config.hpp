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

#include "utility.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace utility
{
	template <typename T, typename EnableIfDummy = void>
	struct ValueConverter
	{
		// Can always convert from T to std::string, 
		// but not necessarily from std::string to T.

		// static bool loadValue(T& var, const std::string& value);
		// static std::string storeValue(const T& value);
	};

	class TreeConfigNode
	{
		TreeConfigNode* _parent;
		std::string _name;
		std::vector<std::unique_ptr<TreeConfigNode>> _children;
		std::map<std::string, std::string> _values;

	public:
		TreeConfigNode(TreeConfigNode&&) = delete;

		TreeConfigNode(TreeConfigNode* parent, std::string name)
			: _parent(parent)
			, _name(std::move(name))
		{}

		auto parent()
		{
			return _parent;
		}

		auto parent() const
		{
			return _parent;
		}

		auto& name()
		{
			return _name;
		}

		auto& name() const
		{
			return _name;
		}

		auto hasValue(const std::string& key) const
		{
			return _values.find(key) != _values.end();
		}

		auto& values() const
		{
			return _values;
		}

		auto& children() const
		{
			return _children;
		}

		template <typename T>
		bool loadValue(const std::string& key, T& var)
		{
			auto found = _values.find(key);

			if (found != _values.end())
			{
				return ValueConverter<T>::loadValue(var, found->second);
			}

			return false;
		}

		bool storeValue(std::string key, std::string var)
		{
			return _values.insert({ std::move(key), std::move(var) }).second;
		}

		template <typename T>
		bool storeValue(std::string key, const T& var)
		{
			return storeValue(std::move(key), ValueConverter<T>::storeValue(var));
		}

		template <typename T>
		bool loadOrStore(const std::string& key, T& value)
		{
			if (!loadValue(key, value))
			{
				storeValue(key, value);
				return false;
			}

			return true;
		}

		template <typename T>
		T loadOrStoreIndirect(const std::string& key, T value)
		{
			loadOrStore(key, value);
			return value;
		}

		std::string loadOrStoreIndirect(const std::string& key, const char* value)
		{
			return loadOrStoreIndirect(key, std::string(value));
		}

		TreeConfigNode* findNode(const std::string& name)
		{
			for (auto& child : _children)
			{
				if (child->name() == name)
				{
					return child.get();
				}
			}

			return nullptr;
		}

		std::vector<TreeConfigNode*> findNodes(const std::string& name)
		{
			std::vector<TreeConfigNode*> nodes;

			for (auto& child : _children)
			{
				if (child->name() == name)
				{
					nodes.push_back(child.get());
				}
			}

			return nodes;
		}

		TreeConfigNode* appendNode(std::unique_ptr<TreeConfigNode>&& node)
		{
			_children.push_back(std::move(node));
			_children.back()->_parent = this;
			return _children.back().get();
		}

		TreeConfigNode* appendNode(std::string name)
		{
			_children.push_back(std::make_unique<TreeConfigNode>(this, std::move(name)));
			return _children.back().get();
		}

		TreeConfigNode* findOrAppendNode(const std::string& name)
		{
			auto node = findNode(name);

			if (node == nullptr)
			{
				node = appendNode(name);
			}

			return node;
		}
	};

	inline bool parseTreeConfigDirect(TreeConfigNode& node, const char*& s)
	{
		if (s == nullptr)
		{
			return false;
		}

		const auto skipSpaces = [](const char* s) {
			for (; std::isspace(*s); ++s); return s;
		};

		const auto skipSpacesReversed = [](const char* s, const char* begin) {
			while (s != begin && std::isspace(*--s)); return s;
		};

		const auto parseName = [=](const char*& s)
		{
			const auto begin = (s = skipSpaces(s));

			for (; std::isprint(*s); ++s)
			{
				if (*s == '=' || *s == '{' || *s == '}')
				{
					break;
				}
			}

			auto end = s;

			for (; begin != end && std::isspace(*(end - 1)); --end);

			return std::string(begin, end);
		};

		const auto parseValue = [=](const char*& s)
		{
			std::string value;
			bool escaped = false;

			for (s = skipSpaces(s); *s != '\0'; ++s)
			{
				if (*s == ';' && !escaped)
				{
					break;
				}
				else
				{
					escaped = escaped ? false : (*s == '\\');

					if (!escaped)
					{
						value += *s;
					}
				}
			}

			return value;
		};

		s = skipSpaces(s);

		if (*s != '{')
		{
			return false;
		}

		for (++s;; ++s)
		{
			auto name = parseName(s);

			if (*s == '=')
			{
				auto value = parseValue(++s);

				if (*s != ';')
				{
					return false;
				}

				node.storeValue(std::move(name), std::move(value));
			}
			else if (*s == '{')
			{
				auto child = node.appendNode(std::move(name));

				if (!parseTreeConfigDirect(*child, s))
				{
					return false;
				}
			}
			else if (*s == '}')
			{
				return name.size() == 0;
			}
			else
			{
				return false;
			}
		}
	}

	inline bool parseTreeConfig(TreeConfigNode& node, const char* s)
	{
		return parseTreeConfigDirect(node, s);
	}

	inline void serializeTreeConfigDirect(std::string& str, 
		const TreeConfigNode& node, std::size_t indentation)
	{
		std::string indent(indentation + 2, '\t');
		
		indent.front() = '\n';
		indent.pop_back();		

		str += indent;

		if (node.parent() != nullptr)
		{
			str += node.name();
			str += ' ';
		}

		str += '{';

		indent.push_back('\t');

		for (auto& p : node.values())
		{
			str += indent;
			str += p.first;
			str += " = ";

			for (auto c : p.second)
			{
				// escape every ; and \ 
				if (c == ';' || c == '\\')
				{
					str += '\\';
				}

				str += c;
			}

			str += ';';
		}

		if (node.values().size() > 0)
		{
			str += '\n';
		}

		for (auto& child : node.children())
		{
			serializeTreeConfigDirect(str, *child, indentation + 1);
		}

		indent.front() = '\t';
		indent.pop_back();
		indent.pop_back();

		str += indent;
		str += "}\n";
	}

	inline std::string serializeTreeConfig(const TreeConfigNode& node)
	{
		std::string str;
		serializeTreeConfigDirect(str, node, 0);
		return str;
	}

	template <typename T>
	struct ValueConverter<T, 
		typename std::enable_if<std::is_convertible<T, std::string>::value>::type>
	{
		static bool loadValue(T& var, const std::string& value)
		{
			var = value;
			return true;
		}

		static std::string storeValue(const T& value)
		{
			return value;
		}
	};

	template <typename T>
	struct ValueConverter<T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
	{
		static bool loadValue(T& var, const std::string& value)
		{
			std::size_t processed;
			T tmp;
			load(tmp, value, processed);

			if (processed == value.size())
			{
				var = tmp;
				return true;
			}

			return false;
		}

		static std::string storeValue(const T& value)
		{
			if (std::is_same<bool, T>::value)
			{
				return value ? "true" : "false";
			}

			return std::to_string(value);
		}

	private:
		static void load(bool& var, const std::string& value, std::size_t& processed)
		{
			if (value.size() == 4 &&
				std::tolower(value[0]) == 't' &&
				std::tolower(value[1]) == 'r' &&
				std::tolower(value[2]) == 'u' &&
				std::tolower(value[3]) == 'e')
			{
				var = true;
				processed = value.size();
			}
			else if (value.size() == 5 &&
				std::tolower(value[0]) == 'f' &&
				std::tolower(value[1]) == 'a' &&
				std::tolower(value[2]) == 'l' &&
				std::tolower(value[3]) == 's' &&
				std::tolower(value[4]) == 'e')
			{
				var = false;
				processed = value.size();
			}
			else
			{
				processed = 0;
			}
		}

		static void load(
			signed char& var, const std::string& value, std::size_t& processed)
		{
			var = static_cast<signed char>(std::stoi(value, &processed));
		}

		static void load(
			unsigned char& var, const std::string& value, std::size_t& processed)
		{
			var = static_cast<unsigned char>(std::stoul(value, &processed));
		}

		static void load(
			signed short& var, const std::string& value, std::size_t& processed)
		{
			var = static_cast<signed short>(std::stoi(value, &processed));
		}

		static void load(
			unsigned short& var, const std::string& value, std::size_t& processed)
		{
			var = static_cast<unsigned short>(std::stoul(value, &processed));
		}

		static void load(
			signed int& var, const std::string& value, std::size_t& processed)
		{
			var = std::stoi(value, &processed);
		}

		static void load(
			unsigned int& var, const std::string& value, std::size_t& processed)
		{
			var = static_cast<unsigned int>(std::stoul(value, &processed));
		}

		static void load(
			signed long& var, const std::string& value, std::size_t& processed)
		{
			var = std::stol(value, &processed);
		}

		static void load(
			unsigned long& var, const std::string& value, std::size_t& processed)
		{
			var = std::stoul(value, &processed);
		}

		static void load(
			signed long long& var, const std::string& value, std::size_t& processed)
		{
			var = std::stoll(value, &processed);
		}

		static void load(
			unsigned long long& var, const std::string& value, std::size_t& processed)
		{
			var = std::stoull(value, &processed);
		}

		static void load(
			double& var, const std::string& value, std::size_t& processed)
		{
			var = std::stod(value, &processed);
		}

		static void load(
			float& var, const std::string& value, std::size_t& processed)
		{
			var = std::stof(value, &processed);
		}
	};
}

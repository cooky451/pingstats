#pragma once

#include "utility.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

template <typename T, typename EnableIfDummy = void>
struct PropertyConverter
{
};

class PropertyNode
{
	struct Compare
	{
		bool operator () (
			const std::unique_ptr<PropertyNode>& lhs,
			const std::unique_ptr<PropertyNode>& rhs)
		{
			return lhs->name() < rhs->name();
		}
	};

	PropertyNode* _parent;
	const std::string _name;
	std::string _saveOnDestruct;
	std::vector<std::unique_ptr<PropertyNode>> _children;
	std::map<std::string, std::string> _values;

public:
	~PropertyNode()
	{
		triggerSave();
	}

	PropertyNode& operator = (PropertyNode&&) = delete;

	PropertyNode(PropertyNode* parent, std::string name)
		: _parent(parent)
		, _name(std::move(name))
	{}

	PropertyNode* parent()
	{
		return _parent;
	}

	const PropertyNode* parent() const
	{
		return _parent;
	}

	const std::string& name() const
	{
		return _name;
	}

	void triggerSave() const
	{
		if (parent() == nullptr && _saveOnDestruct.size() > 0)
		{
			std::ofstream file(_saveOnDestruct);
			save(file);
		}
	}

	bool hasValue(const std::string& key) const
	{
		return _values.find(key) != _values.end();
	}

	const decltype(_values)& values() const
	{
		return _values;
	}

	const decltype(_children)& nodes() const
	{
		return _children;
	}

	void setSaveOnDestruct(std::string filename)
	{
		_saveOnDestruct = filename;
	}

	template <typename T>
	bool loadValue(const std::string& key, T& var)
	{
		auto found = _values.find(key);

		if (found != _values.end())
		{
			return PropertyConverter<T>::loadValue(var, found->second);
		}

		return false;
	}

	template <typename T>
	bool storeValue(const std::string& key, const T& var)
	{
		auto str = PropertyConverter<T>::storeValue(var);
		str.swap(_values[key]);
		return str == ""; // Empty value strings don't count as overwritten.
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

	PropertyNode* findNode(const std::string& name)
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

	PropertyNode* appendNode(const std::string& name)
	{
		_children.push_back(std::make_unique<PropertyNode>(this, name));
		return _children.back().get();
	}

	PropertyNode* appendNode(std::unique_ptr<PropertyNode>&& node)
	{
		_children.push_back(std::move(node));
		return _children.back().get();
	}

	PropertyNode* findOrAppendNode(const std::string& name)
	{
		auto node = findNode(name);

		if (node == nullptr)
		{
			node = appendNode(name);
		}

		return node;
	}

	bool parse(const std::string& s)
	{
		return parse(s.c_str());
	}

	bool parse(const char* s)
	{
		return parseDirect(s);
	}

	bool parseDirect(const char*& begin)
	{
		const auto skipUntil = [](const char* s, char until) { while (*s != '\0' && *s != until) ++s; return s; };
		const auto skipSpaces = [](const char* s) { while (std::isspace(*s)) ++s; return s; };
		const auto skipIdents = [](const char* s) { 
			while (std::isalnum(*s) || *s == '.' || *s == '_' || *s == '-') ++s; return s; };

		while (begin != nullptr && *(begin = skipSpaces(begin)) != '\0' && *begin != '}')
		{
			auto nameBegin = begin;
			auto nameEnd = skipIdents(nameBegin);

			begin = skipSpaces(nameEnd);

			if (*begin == '=')
			{
				auto valueBegin = skipSpaces(begin + 1);
				auto valueEnd = skipUntil(valueBegin, ';');
				begin = valueEnd;

				if (*begin == ';')
				{
					++begin;
					_values[std::string(nameBegin, nameEnd)] = std::string(valueBegin, valueEnd);
					continue;
				}
			}

			if (*begin == '{')
			{
				auto child = std::make_unique<PropertyNode>(this, std::string(nameBegin, nameEnd));
				child->parseDirect(++begin);

				if (*begin == '}')
				{
					++begin;
					_children.push_back(std::move(child));
					continue;
				}
			}

			return false;
		}

		return true;
	}

	void save(std::ostream& file, std::uint16_t indentation = 0) const
	{
		const auto indent = [&](std::ostream& file) -> std::ostream&
		{
			for (std::uint16_t i = 0; i < indentation; ++i)
			{
				file << '\t';
			}

			return file;
		};

		if (this->parent() != nullptr)
		{
			indent(file) << _name << " {\n";
			indentation += 1;
		}

		for (auto& p : _values)
		{
			indent(file) << p.first << " = " << p.second << ";\n";
		}

		if (_children.size() > 0)
		{
			indent(file) << '\n';
		}

		for (auto& child : _children)
		{
			child->save(file, indentation);
		}

		if (this->parent() != nullptr)
		{
			indentation -= 1;
			indent(file) << "}\n";
			indent(file) << '\n';
		}
	}
};

template <typename T>
struct PropertyConverter<T, typename std::enable_if<std::is_convertible<T, std::string>::value>::type>
{
	// Can convert from T to std::string, but not necessarily from std::string to T.
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
struct PropertyConverter<T, typename std::enable_if<std::is_arithmetic<T>::value>::type>
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

	static void load(signed char& var, const std::string& value, std::size_t& processed)
	{
		var = static_cast<signed char>(std::stoi(value, &processed));
	}

	static void load(unsigned char& var, const std::string& value, std::size_t& processed)
	{
		var = static_cast<unsigned char>(std::stoul(value, &processed));
	}

	static void load(signed short& var, const std::string& value, std::size_t& processed)
	{
		var = static_cast<signed short>(std::stoi(value, &processed));
	}

	static void load(unsigned short& var, const std::string& value, std::size_t& processed)
	{
		var = static_cast<unsigned short>(std::stoul(value, &processed));
	}

	static void load(signed int& var, const std::string& value, std::size_t& processed)
	{
		var = std::stoi(value, &processed);
	}

	static void load(unsigned int& var, const std::string& value, std::size_t& processed)
	{
		var = static_cast<unsigned int>(std::stoul(value, &processed));
	}

	static void load(signed long& var, const std::string& value, std::size_t& processed)
	{
		var = std::stol(value, &processed);
	}

	static void load(unsigned long& var, const std::string& value, std::size_t& processed)
	{
		var = std::stoul(value, &processed);
	}

	static void load(signed long long& var, const std::string& value, std::size_t& processed)
	{
		var = std::stoll(value, &processed);
	}

	static void load(unsigned long long& var, const std::string& value, std::size_t& processed)
	{
		var = std::stoull(value, &processed);
	}

	static void load(double& var, const std::string& value, std::size_t& processed)
	{
		var = std::stod(value, &processed);
	}

	static void load(float& var, const std::string& value, std::size_t& processed)
	{
		var = std::stof(value, &processed);
	}
};

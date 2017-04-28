#pragma once

#include "utility/utility.hpp"
#include "winapi/utility.hpp"
#include "canvas_drawing.hpp"

#include <algorithm>
#include <vector>

namespace pingstats // export
{
	using namespace utility::literals;

	namespace ut = utility;
	namespace wa = winapi;

	class FontSpacing
	{
	public:
		LONG fontWidth{ 8 };
		LONG fontHeight{ 16 };
		LONG fontLineSpacing{ 16 };
	};

	class StringCache
	{
		static constexpr std::size_t CACHE_MAX_SIZE{ 512 };
		static constexpr std::size_t CACHE_CLEAR_SIZE{ 256 };

		class ColoredString
		{
		public:
			wa::MemoryCanvas canvas;
			std::string str;
			Color clearColor{ 0, 0, 0 };
			Color stringColor{ 0, 0, 0 };
			
			mutable uint32_t usageCounter = {};

			ColoredString() = default;

			ColoredString(std::string str, Color clearColor, Color stringColor)
				: str(std::move(str))
				, clearColor(clearColor)
				, stringColor(stringColor)
			{}

			bool operator == (const ColoredString& rhs) const
			{
				return str == rhs.str 
					&& clearColor.value == rhs.clearColor.value 
					&& stringColor.value == rhs.stringColor.value;
			}
		};

		std::vector<ColoredString> _cache;
		wa::DeviceContext _deviceContext;
		wa::FontPtr _font;
		LOGFONT _logFont;
		FontSpacing _spacing{};

	public:
		StringCache(const LOGFONT& logFont)
			: _deviceContext{ CreateCompatibleDC(nullptr) }
			, _font{ CreateFontIndirectW(&logFont) }
			, _logFont{ logFont }
		{
			_deviceContext.select(_font.get());

			TEXTMETRIC metric;

			if (GetTextMetricsW(_deviceContext.get(), &metric))
			{
				_spacing.fontWidth = metric.tmAveCharWidth;
				_spacing.fontHeight = metric.tmHeight;
				_spacing.fontLineSpacing = metric.tmHeight + metric.tmExternalLeading;
			}
		}

		auto& getLogicalFont() const
		{
			return _logFont;
		}

		auto& getFontSpacing() const
		{
			return _spacing;
		}

		void draw(
			wa::MemoryCanvas& canvas, 
			Color clearColor, 
			Color stringColor, 
			int32_t x, int32_t y,
			std::string s)
		{
			if (s.size() == 0)
			{
				return;
			}

			shrink();

			ColoredString cs(std::move(s), clearColor, stringColor);
			auto it = std::find(_cache.begin(), _cache.end(), cs);

			if (it == _cache.end())
			{
				const auto strSize = static_cast<int>(cs.str.size());

				cs.canvas = wa::MemoryCanvas{ 
					strSize * _spacing.fontWidth, _spacing.fontHeight };

				_deviceContext.select(cs.canvas.get());

				clearCanvas(cs.canvas, clearColor);

				SetBkMode(_deviceContext.get(), TRANSPARENT);
				SetTextColor(_deviceContext.get(), stringColor.toColorRef());

				TextOutA(_deviceContext.get(), 0, 0, cs.str.data(), strSize);

				_cache.push_back(std::move(cs));

				it = _cache.begin() + (_cache.size() - 1);
			}

			it->usageCounter += 1;

			const Rect rect{
				x, y, 
				x + static_cast<pxindex>(it->canvas.width()), 
				y + static_cast<pxindex>(it->canvas.height())
			};

			copyCanvasRect(canvas, it->canvas, rect, 0, 0);
		}

		void shrink()
		{
			if (_cache.size() >= CACHE_MAX_SIZE)
			{
				std::sort(_cache.begin(), _cache.end(), 
					[](const ColoredString& lhs, const ColoredString& rhs) {
						return lhs.usageCounter > rhs.usageCounter;
					}
				);

				_cache.resize(CACHE_CLEAR_SIZE);
			}
		}
	};

	StringCache& getStaticStringCache(const LOGFONT& logFont)
	{
		thread_local std::vector<StringCache> cache;

		for (size_t i = 0; i < cache.size(); ++i)
		{
			if (std::memcmp(
				&cache[i].getLogicalFont(), 
				&logFont, 
				sizeof logFont) == 0)
			{
				return cache[i];
			}
		}

		cache.emplace_back(logFont);

		return cache.back();
	}
}

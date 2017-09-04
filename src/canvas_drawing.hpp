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

#include "utility/utility.hpp"
#include "utility/tree_config.hpp"

#include "winapi/utility.hpp"

#include "utility.hpp"

namespace pingstats // export
{
	using namespace utility::literals;

	namespace ut = utility;
	namespace wa = winapi;

	using pxindex = LONG;

	struct Rect
	{
		pxindex left;
		pxindex top;
		pxindex right;
		pxindex bottom;

		constexpr auto width() const
		{
			return right - left;
		}

		constexpr auto height() const
		{
			return bottom - top;
		}
	};

	struct Color
	{
		std::uint32_t value;

		Color() = default;

		explicit constexpr Color(std::uint32_t value)
			: value{ value }
		{}

		constexpr Color(
			std::uint8_t r, 
			std::uint8_t g, 
			std::uint8_t b, 
			std::uint8_t a = { 0 })
			: value{ 
				(static_cast<std::uint32_t>(r) << 16) | 
				(static_cast<std::uint32_t>(g) << 8) |
				(static_cast<std::uint32_t>(b) << 0) |
				(static_cast<std::uint32_t>(a) << 24) }
		{}

		constexpr auto r() const
		{
			return static_cast<std::uint8_t>((value >> 16) & 0xFF);
		}

		constexpr auto g() const
		{
			return static_cast<std::uint8_t>((value >> 8) & 0xFF);
		}

		constexpr auto b() const
		{
			return static_cast<std::uint8_t>((value >> 0) & 0xFF);
		}

		constexpr auto a() const
		{
			return static_cast<std::uint8_t>((value >> 24) & 0xFF);
		}

		constexpr auto toColorRef() const
		{
			return RGB(r(), g(), b());
		}
	};

	struct Vertex
	{
		double x;
		double y;
		Color color;
	};

	constexpr Color mergeColors(Color c0, Color c1, double weight)
	{
		const auto w{ fastround<uintptr_t>(weight * 256.0) };
		const auto rw{ 256 - w };

		return Color{
			static_cast<uint8_t>(((c0.r() * rw) >> 8) + ((c1.r() * w) >> 8)),
			static_cast<uint8_t>(((c0.g() * rw) >> 8) + ((c1.g() * w) >> 8)),
			static_cast<uint8_t>(((c0.b() * rw) >> 8) + ((c1.b() * w) >> 8)) };
	}

	//constexpr Color mergeColors(Color c0, Color c1, double weight)
	//{
	//	const auto w{ weight };
	//	const auto rw{ 1.0 - w };

	//	return Color{
	//		static_cast<std::uint8_t>(c0.r() * rw + c1.r() * w),
	//		static_cast<std::uint8_t>(c0.g() * rw + c1.g() * w),
	//		static_cast<std::uint8_t>(c0.b() * rw + c1.b() * w) };
	//}

	void resizeCanvasPredictive(wa::MemoryCanvas& canvas, LONG width, LONG height)
	{
		const auto roundUp{ [](auto x) { 
			return static_cast<LONG>(~15 & (x + 16));
		} };

		if (canvas.width() == 0 ||
			canvas.height() == 0 ||
			canvas.width() > width * 5 / 4 ||
			canvas.height() > height * 5 / 4)
		{
			canvas = wa::MemoryCanvas{ roundUp(width), roundUp(height) };
		}
		else if (canvas.width() < width || canvas.height() < height)
		{
			width = width * 5 / 4;
			height = height * 5 / 4;

			canvas = wa::MemoryCanvas{ roundUp(width), roundUp(height) };
		}
	}

	void clearCanvas(wa::MemoryCanvas& canvas, Color color)
	{
		const auto ptr{ canvas.pixelPtr() };
		const auto size{ canvas.size() };

		// Generates rep stos, don't change to fill/fill_n.
		for (std::size_t i{}; i < size; ++i)
		{
			ptr[i] = color.value;
		}
	}

	void fillCanvasRect(wa::MemoryCanvas& canvas, const Rect& rect, Color color)
	{
		const auto ptr{ canvas.pixelPtr() };
		const auto width{ static_cast<std::size_t>(rect.width()) };
		const auto height{ static_cast<std::size_t>(rect.height()) };
		const auto x{ static_cast<std::size_t>(rect.left) };
		const auto y{ static_cast<std::size_t>(rect.top) };
		const auto w{ static_cast<std::size_t>(canvas.width()) };
		const auto h{ static_cast<std::size_t>(canvas.height()) };

		for (std::size_t j{}; j < height; ++j)
		{
			const auto line{ &ptr[x + j * w] };

			// Generates rep stos, don't change to fill/fill_n.
			for (std::size_t i{}; i < width; ++i)
			{
				line[i] = color.value;
			}
		}
	}

	void copyCanvasRect(wa::MemoryCanvas& dest, wa::MemoryCanvas& source,
		const Rect& destRect, std::size_t sourceX, std::size_t sourceY)
	{
		const auto sptr{ source.pixelPtr() };
		const auto dptr{ dest.pixelPtr() };
		const auto width{ static_cast<std::size_t>(destRect.width()) };
		const auto height{ static_cast<std::size_t>(destRect.height()) };
		const auto sx{ sourceX };
		const auto sy{ sourceY };
		const auto dx{ static_cast<std::size_t>(destRect.left) };
		const auto dy{ static_cast<std::size_t>(destRect.top) };
		const auto sw{ static_cast<std::size_t>(source.width()) };
		const auto dw{ static_cast<std::size_t>(dest.width()) };

		for (std::size_t j{}; j < height; ++j)
		{
			const auto dline{ &dptr[dx + (dy + j) * dw] };
			const auto sline{ &sptr[sx + (sy + j) * sw] };

			std::memcpy(dline, sline, 4 * width);
		}
	}

	void plot(wa::MemoryCanvas& canvas, const Rect& clip, 
		pxindex x, pxindex y, Color color, double weight)
	{
		if (x >= clip.left && 
			x < clip.right && 
			y >= clip.top &&
			y < clip.bottom)
		{
			canvas(x, y) = mergeColors(Color{ canvas(x, y) }, color, weight).value;
		}
	}

	void plot(wa::MemoryCanvas& canvas, 
		const Rect& clip, pxindex x, pxindex y, Color color)
	{
		if (x >= clip.left &&
			x < clip.right &&
			y >= clip.top &&
			y < clip.bottom)
		{
			canvas(x, y) = color.value;
		}
	}

	void drawHorizontalLine(wa::MemoryCanvas& canvas, 
		const Rect& clip, Color color, pxindex y, pxindex x0, pxindex x1)
	{
		if (y >= clip.top && y < clip.bottom)
		{
			if (x0 > x1)
			{
				std::swap(x0, x1);
			}

			x0 = std::clamp(x0, clip.left, clip.right - 1);
			x1 = std::clamp(x1, clip.left, clip.right - 1);

			const auto ptr{ canvas.pixelPtr() };
			const auto size{ canvas.size() };
			const auto width{ canvas.width() };
			const auto height{ canvas.height() };
			const auto start{ x0 + y * width };
			const auto end{ x1 + y * width };

			for (auto i{ start }; i <= end; ++i)
			{
				ptr[i] = color.value;
			}
		}
	}

	void drawVerticalLine(wa::MemoryCanvas& canvas, 
		const Rect& clip, Color color, pxindex x, pxindex y0, pxindex y1)
	{
		if (x >= clip.left && x < clip.right)
		{
			if (y0 > y1)
			{
				std::swap(y0, y1);
			}

			y0 = std::clamp(y0, clip.top, clip.bottom - 1);
			y1 = std::clamp(y1, clip.top, clip.bottom - 1);

			const auto ptr{ canvas.pixelPtr() };
			const auto size{ canvas.size() };
			const auto width{ canvas.width() };
			const auto height{ canvas.height() };
			const auto start{ x + y0 * width };
			const auto end{ x + y1 * width };

			for (auto j{ start }; j <= end; j += width)
			{
				ptr[j] = color.value;
			}
		}
	}

	template <unsigned THICKNESS>
	bool drawPrettyLine(
		wa::MemoryCanvas& canvas, 
		const Rect& clip,
		Color color, 
		double x0, 
		double y0, 
		double x1, 
		double y1,
		bool prevSteep, 
		bool isSteep, 
		bool nextSteep, 
		bool isFirst, 
		bool isLast)
	{
		const auto fracpart{ [](double x) {
			return x - static_cast<pxindex>(x);
		} };

		const auto drawEndpoint{ [=](
			MemoryCanvas& canvas,
			pxindex x,
			double ry,
			double xgap,
			Color color)
		{
			switch (THICKNESS)
			{
			default:
			{
				const auto y{ fastround<pxindex>(ry) };

				if (!isSteep)
				{
					plot(canvas, clip, x, y, color, xgap);
				}
				else
				{
					plot(canvas, clip, y, x, color, xgap);
				}
			}	break;

			case 1:
			{
				const auto y{ static_cast<pxindex>(ry) };
				const auto weight{ fracpart(ry) };

				if (!isSteep)
				{
					plot(canvas, clip, x, y + 0, color, xgap * (1.0 - weight));
					plot(canvas, clip, x, y + 1, color, xgap * weight);
				}
				else
				{
					plot(canvas, clip, y + 0, x, color, xgap * (1.0 - weight));
					plot(canvas, clip, y + 1, x, color, xgap * weight);
				}
			}	break;

			case 2:
			{
				const auto y{ fastround<pxindex>(ry) };
				const auto weight{ fracpart(ry + 0.5) };

				if (!isSteep)
				{
					plot(canvas, clip, x, y - 1, color, xgap * (1.0 - weight));
					plot(canvas, clip, x, y + 0, color, xgap);
					plot(canvas, clip, x, y + 1, color, xgap * weight);
				}
				else
				{
					plot(canvas, clip, y - 1, x, color, xgap * (1.0 - weight));
					plot(canvas, clip, y + 0, x, color, xgap);
					plot(canvas, clip, y + 1, x, color, xgap * weight);
				}
			}	break;
			}
		} };

		if (isSteep)
		{
			std::swap(x0, y0);
			std::swap(x1, y1);
		}

		const auto drawSwapped{ x0 > x1 };

		if (drawSwapped)
		{
			std::swap(x0, x1);
			std::swap(y0, y1);
		}

		bool isStartTransparent{ isFirst || prevSteep != isSteep };
		bool isEndTransparent{ isLast || nextSteep != isSteep };
		bool isStartSolid{ !isFirst && prevSteep == isSteep };
		bool isEndSolid{ false };

		if (drawSwapped)
		{
			std::swap(isStartTransparent, isEndTransparent);
			std::swap(isStartSolid, isEndSolid);
		}

		const auto gradient{ (y1 - y0) / (x1 - x0) };
		const auto xstart{ fastround<pxindex>(x0) };
		const auto ystart{ y0 + gradient * (xstart - x0) };
		const auto xend{ fastround<pxindex>(x1) };
		const auto yend{ y1 + gradient * (xend - x1) };

		if (isStartTransparent)
		{
			drawEndpoint(
				canvas, 
				xstart, 
				ystart, 
				1.0 - fracpart(x0 + 0.5), 
				color);
		}

		if (isEndTransparent)
		{
			drawEndpoint(canvas, xend, yend, fracpart(x1 + 0.5), color);
		}

		const auto itstart{ xstart + !isStartSolid };
		const auto itend{ xend + isEndSolid };
		auto ry{ ystart + !isStartSolid * gradient };

		if (!isSteep)
		{
			for (auto x{ itstart }; x < itend; ++x, ry += gradient)
			{
				switch (THICKNESS)
				{
				default:
				{
					plot(canvas, clip, x, fastround<pxindex>(ry), color, 1);
				}	break;

				case 1:
				{
					const auto y{ static_cast<pxindex>(ry) };
					const auto weight{ fracpart(ry) };
					plot(canvas, clip, x, y - 0, color, 1 - weight);
					plot(canvas, clip, x, y + 1, color, weight);
				}	break;

				case 2:
				{
					const auto y{ fastround<pxindex>(ry) };
					const auto weight{ fracpart(ry + 0.5) };
					plot(canvas, clip, x, y - 1, color, 1 - weight);
					plot(canvas, clip, x, y + 0, color, 1);
					plot(canvas, clip, x, y + 1, color, weight);
				}	break;
				}
			}
		}
		else
		{
			for (auto x{ itstart }; x < itend; ++x, ry += gradient)
			{
				switch (THICKNESS)
				{
				default:
				{
					plot(canvas, clip, fastround<pxindex>(ry), x, color, 1);
				}	break;

				case 1:
				{
					const auto y{ static_cast<pxindex>(ry) };
					const auto weight{ fracpart(ry) };
					plot(canvas, clip, y - 0, x, color, 1 - weight);
					plot(canvas, clip, y + 1, x, color, weight);
				}	break;

				case 2:
				{
					const auto y{ fastround<pxindex>(ry) };
					const auto weight{ fracpart(ry + 0.5) };
					plot(canvas, clip, y - 1, x, color, 1 - weight);
					plot(canvas, clip, y + 0, x, color, 1);
					plot(canvas, clip, y + 1, x, color, weight);
				}	break;
				}
			}
		}

		return isSteep;
	}

	template <unsigned THICKNESS>
	void drawPrettyLines(wa::MemoryCanvas& canvas, 
		const Rect& clip, const Vertex* vertices, std::size_t nVertices)
	{
		if (nVertices >= 2)
		{
			bool prevSteep{ false };

			for (std::size_t i{}; i + 1 < nVertices; ++i)
			{
				const auto x0{ vertices[i].x };
				const auto y0{ vertices[i].y };
				const auto x1{ vertices[i + 1].x };
				const auto y1{ vertices[i + 1].y };

				const auto isSteep{ std::abs(x1 - x0) < std::abs(y1 - y0) };
				const auto nextSteep{ i + 2 >= nVertices ? false :
					std::abs(vertices[i + 2].x - vertices[i + 1].x) < 
					std::abs(vertices[i + 2].y - vertices[i + 1].y) };

				drawPrettyLine<THICKNESS>(
					canvas, clip, vertices[i + 1].color, 
					x0, y0, x1, y1,
					prevSteep, isSteep, nextSteep,
					i == 0, i + 2 == nVertices);

				prevSteep = isSteep;
			}
		}
	}

	void drawSawLines(
		wa::MemoryCanvas& canvas, const Rect& clip, 
		const Vertex* vertices, std::size_t nVertices)
	{
		for (size_t i = 0; i + 1 < nVertices; ++i)
		{
			const auto x0{ fastround<pxindex>(vertices[i].x) };
			const auto y0{ fastround<pxindex>(vertices[i].y) };
			const auto c0{ vertices[i].color };
			const auto x1{ fastround<pxindex>(vertices[i + 1].x) };
			const auto y1{ fastround<pxindex>(vertices[i + 1].y) };
			const auto c1{ vertices[i + 1].color };

			drawHorizontalLine(canvas, clip, c0, y0, x0, x1);
			drawVerticalLine(canvas, clip, c1, x1, y0, y1);
		}
	}

	void drawPrettyLines(
		wa::MemoryCanvas& canvas, const Rect& clip, int thickness, 
		const Vertex* vertices, std::size_t nVertices)
	{
		switch (thickness)
		{
		default:
		{
			drawPrettyLines<0>(canvas, clip, vertices, nVertices);
		}	break;

		case -1:
		{
			drawSawLines(canvas, clip, vertices, nVertices);
		}	break;

		case 1:
		{
			drawPrettyLines<1>(canvas, clip, vertices, nVertices);
		}	break;

		case 2:
		{
			drawPrettyLines<2>(canvas, clip, vertices, nVertices);
		}	break;
		}
	}
}

namespace utility
{
	template <>
	struct ValueConverter<pingstats::Color, void>
	{
		static bool loadValue(pingstats::Color& var, const std::string& value)
		{
			unsigned r, g, b;

			if (std::sscanf(value.c_str(), "%u,%u,%u", &r, &g, &b) == 3)
			{
				var = pingstats::Color{
					static_cast<std::uint8_t>(r), 
					static_cast<std::uint8_t>(g),
					static_cast<std::uint8_t>(b) };

				return true;
			}

			return false;
		}

		static std::string storeValue(const pingstats::Color& value)
		{
			return formatString("%u, %u, %u", value.r(), value.g(), value.b());
		}
	};
}

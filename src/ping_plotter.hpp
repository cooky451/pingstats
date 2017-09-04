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
#include "winapi/utility.hpp"

#include "canvas_drawing.hpp"
#include "string_cache.hpp"
#include "ping_data.hpp"

#include <string>
#include <vector>

namespace pingstats // export
{
	using namespace utility::literals;

	namespace cr = std::chrono;
	namespace ut = utility;
	namespace wa = winapi;

	class PingPlotter
	{
		std::string _name{ "Name" };
		std::string _statusString;

		double _pixelsPerSecond{ 10.0 };
		double _secondsPerGridLine{ 5.0 };
		std::int32_t _plotThickness{ 2 };

		Color _clearColor = Color{ 4, 4, 20 };
		Color _textColor = Color{ 180, 180, 180 };
		Color _pingColor = Color{ 40, 140, 180 };
		Color _lossColor = Color{ 180, 140, 40 };
		Color _gridColor = Color{ 60, 50, 20 };
		Color _borderColor = Color{ 160, 140, 60 };
		Color _lineColor = Color{ 120, 120, 120 };
		Color _selectionColor = Color{ 160, 160, 160 };

		LOGFONT _logFont{};

		std::vector<Vertex> _pointBuffer;

		std::size_t _lastWidth{};
		std::size_t _lastHeight{};

		std::size_t _selection{};
		cr::steady_clock::time_point _selectionTime{};
		double _selectionTimeMs{};

	public:
		PingPlotter(ut::TreeConfigNode& config)
		{
			_name = config.name();

			auto& rendercfg{ *config.findOrAppendNode("render") };

			{
				auto& plotcfg{ *rendercfg.findOrAppendNode("plot") };

				plotcfg.loadOrStore("pixelsPerSecond", _pixelsPerSecond);
				plotcfg.loadOrStore("secondsPerGridLine", _secondsPerGridLine);
				plotcfg.loadOrStore("thickness", _plotThickness);
			}

			{
				auto& fontcfg{ *rendercfg.findOrAppendNode("font") };

				auto size{ 16 };
				auto name{ "consolas"s };

				fontcfg.loadOrStore("size", size);
				fontcfg.loadOrStore("name", name);

				_logFont.lfHeight = size;
				_logFont.lfWeight = FW_NORMAL;
				_logFont.lfCharSet = ANSI_CHARSET;
				_logFont.lfOutPrecision = OUT_TT_PRECIS;
				_logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
				_logFont.lfQuality = CLEARTYPE_QUALITY;
				_logFont.lfPitchAndFamily = FF_DONTCARE;

				wa::wstr(&_logFont.lfFaceName[0], 32, name.c_str(), name.size());
			}

			{
				auto& colors{ *rendercfg.findOrAppendNode("colors") };

				colors.loadOrStore("clear", _clearColor);
				colors.loadOrStore("text", _textColor);
				colors.loadOrStore("ping", _pingColor);
				colors.loadOrStore("loss", _lossColor);
				colors.loadOrStore("grid", _gridColor);
				colors.loadOrStore("border", _borderColor);
				colors.loadOrStore("line", _lineColor);
				colors.loadOrStore("selection", _selectionColor);
			}
		}

		auto& statusString() const
		{
			return _statusString;
		}

		auto pixelsPerSecond() const
		{
			return _pixelsPerSecond;
		}

		void redraw(
			wa::MemoryCanvas& canvas,
			const Rect& rect,
			const PingData& pingData,
			cr::steady_clock::time_point now,
			cr::steady_clock::time_point selectionOffset,
			bool drawSelectionLine,
			Color clearColor)
		{
			setStatusString(pingData);

			if (_clearColor.value != clearColor.value)
			{
				fillCanvasRect(canvas, rect, _clearColor);
			}

			auto& stringCache{ getStaticStringCache(_logFont) };

			const auto& fontSpacing{ stringCache.getFontSpacing() };
			const auto lineHeight{ fontSpacing.fontHeight };
			const auto infoHeight{ lineHeight * 3 + 8 };

			if (rect.height() > infoHeight)
			{
				const Rect plotRect{
					rect.left, 
					rect.top, 
					rect.right, 
					rect.bottom - infoHeight
				};

				drawGrid(canvas, plotRect, pingData);
				drawPlot(canvas, plotRect, pingData, now, selectionOffset, drawSelectionLine);
				drawInfo(canvas, rect, pingData, now, stringCache);
				drawBorder(canvas, rect);
			}
		}

	private:
		void setStatusString(const PingData& pingData)
		{
			if (pingData.lastResult() == nullptr)
			{
				_statusString = "No response yet.";
			}
			else
			{
				const auto& result{
					_selection < pingData.pingResults().size() ?
					pingData.pingResults()[_selection] :
					*pingData.lastResult() };

				if (result.statusCode != 0)
				{
					_statusString = ut::formatString("(%u, %u) %s",
						result.errorCode,
						result.statusCode,
						makeIpStatusString(result.statusCode).c_str());
				}
				else if (result.errorCode != 0)
				{
					_statusString = ut::formatString("(%u, %u) %s",
						result.errorCode,
						result.statusCode,
						std::system_category().
						message(result.errorCode).c_str());
				}
				else
				{
					_statusString = "(0, 0) Success";
				}
			}
		}

		void drawGrid(
			wa::MemoryCanvas& canvas, 
			const Rect& rect, 
			const PingData& pingData)
		{
			const auto xStep{ _secondsPerGridLine * _pixelsPerSecond };
			const auto xStart{ rect.right - 1 - xStep };

			for (auto x{ xStart }; x > rect.left; x -= xStep)
			{
				const auto ix{ fastround<pxindex>(x) };
				drawVerticalLine(canvas, rect, _gridColor, ix, rect.top, rect.bottom - 1);
			}

			const auto yStep{ pingData.gridSizeY() * pingData.pixelPerMs() };
			const auto yOffset{ pingData.pingOffsetMs() * pingData.pixelPerMs() };
			const auto yStart{ rect.bottom - 1 - yOffset };

			for (auto y{ yStart }; y > rect.top; y -= yStep)
			{
				const auto iy{ fastround<pxindex>(y) };
				drawHorizontalLine(canvas, rect, _gridColor, iy, rect.left, rect.right - 1);
			}
		}

		void drawBorder(wa::MemoryCanvas& canvas, const Rect& rect)
		{
			drawVerticalLine(canvas, rect, 
				_borderColor, rect.left, rect.top, rect.bottom - 1);
			drawVerticalLine(canvas, rect, 
				_borderColor, rect.right - 1, rect.top, rect.bottom - 1);
			drawHorizontalLine(canvas, rect, 
				_borderColor, rect.top, rect.left, rect.right - 1);
			drawHorizontalLine(canvas, rect, 
				_borderColor, rect.bottom - 1, rect.left, rect.right - 1);
		}

		void drawInfo(
			wa::MemoryCanvas& canvas,
			const Rect& rect, 
			const PingData& pingData, 
			cr::steady_clock::time_point now, 
			StringCache& stringCache)
		{
			const auto& fontSpacing = stringCache.getFontSpacing();
			const auto fsy = fontSpacing.fontLineSpacing;
			const auto fsx = fontSpacing.fontWidth;
			const auto row0 = rect.bottom - 8 - 3 * fsy;
			const auto row1 = rect.bottom - 8 - 2 * fsy;
			const auto row2 = rect.bottom - 8 - 1 * fsy;
			const auto col0 = rect.left + 8;
			const auto col1 = rect.right - 8 - 26 * fsx;
			const auto col2 = rect.right - 8 - 12 * fsx;

			const auto calcPrecision = [](auto x, auto a, auto b, auto c) { 
				return x >= 1000 ? 0 : x >= 100 ? a : x >= 10 ? b : c;
			};

			// Column 2
			stringCache.draw(canvas, _clearColor, _pingColor, col2, row0,
				ut::formatString("ping %4.*f ms", 
					calcPrecision(pingData.lastPing(), 0, 1, 2), 
					pingData.lastPing()));

			stringCache.draw(canvas, _clearColor, _lossColor, col2, row1,
				ut::formatString("jttr %4.*f ms", 
					calcPrecision(pingData.jitter(), 0, 1, 2), 
					pingData.jitter()));

			if (col0 + static_cast<LONG>(_statusString.size()) * fsx < col2)
			{
				stringCache.draw(canvas, _clearColor, _lossColor, col2, row2,
					ut::formatString("loss %4.*f %%", 
						calcPrecision(pingData.lossPercentage(), 0, 1, 2), 
						pingData.lossPercentage()));
			}

			// Column 1
			{
				const auto delta = now - _selectionTime;
				const auto min = cr::duration_cast<cr::minutes>(delta);
				const auto sec = cr::duration_cast< cr::seconds>(delta) - min;
				const auto minc = static_cast<int>(min.count());
				const auto secc = static_cast<int>(sec.count());

				const auto str = ut::formatString("[%.*f ms | -%02d:%02d]",
					calcPrecision(_selectionTimeMs, 0, 1, 2), 
					_selectionTimeMs, minc, secc);

				const auto x = 
					col1 - fsx * (static_cast<int>(str.size()) - 12);

				stringCache.draw(
					canvas, _clearColor, _textColor, x, row0, str);
			}

			stringCache.draw(canvas, _clearColor, _textColor, col1, row1,
				ut::formatString("mean %4.*f ms", 
					calcPrecision(pingData.meanPing(), 0, 1, 2),
					pingData.meanPing()));

			if (col0 + static_cast<LONG>(_statusString.size()) * fsx < col1)
			{
				stringCache.draw(canvas, _clearColor, _textColor, col1, row2,
					ut::formatString("grid %4.*f ms", 
						calcPrecision(pingData.gridSizeY(), 0, 1, 2), 
						pingData.gridSizeY()));
			}

			// Column 0
			stringCache.draw(canvas, _clearColor, _textColor, col0, row0, _name);

			stringCache.draw(canvas, _clearColor, _textColor, 
				col0, row1, pingData.lastResponder());

			stringCache.draw(canvas, _clearColor, _textColor, col0, row2, _statusString);
		}

		void drawPlot(
			wa::MemoryCanvas& canvas,
			const Rect& rect,
			const PingData& pingData,
			std::chrono::steady_clock::time_point now,
			cr::steady_clock::time_point selectionTime,
			bool drawSelectionLine)
		{
			const auto left = rect.left;
			const auto right = rect.right;
			const auto top = rect.top;
			const auto bot = rect.bottom;

			const auto& pingResults{ pingData.pingResults() };

			const auto abstime{ [](auto t) {
				return t < decltype(t){} ? -t : t;
			} };

			const auto calcX{ [&](auto sentTime) {
				return right -
					ut::seconds_f64{ now - sentTime }.count() * _pixelsPerSecond;
			} };

			const auto calcY{ [&](auto rtt) {
				const auto offMs{ pingData.pingOffsetMs()};
				const auto ms{ ut::milliseconds_f64{ rtt }.count() };
				return bot - (ms + offMs) * pingData.pixelPerMs();
			} };

			const auto getFirstContributingResult{ [&] {
				for (std::size_t i{}; i < pingResults.size(); ++i)
				{
					if (calcX(pingResults[i].sentTime) > left)
					{
						return i - (i > 0);
					}
				}

				return pingResults.size();
			} };

			const auto getFirstSuccessfulPingMs{ [&](std::size_t start) {
				for (auto i{ start }; i < pingResults.size(); ++i)
				{
					if (pingResults[i].statusCode == 0)
					{
						return ut::milliseconds_f64{ pingResults[i].latency }.count();
					}
				}

				return pingData.meanPing();
			} };

			auto startIndex{ getFirstContributingResult() };

			if (startIndex < pingResults.size())
			{
				_pointBuffer.clear();

				auto selectionDistance{ cr::nanoseconds::max() };
				auto _lastPingMs{ getFirstSuccessfulPingMs(startIndex) };

				for (auto i{ startIndex }; i < pingResults.size(); ++i)
				{
					const auto x{ calcX(pingResults[i].sentTime) };
					auto color{ _lossColor };

					if (pingResults[i].errorCode == 0 && 
						pingResults[i].statusCode == 0)
					{
						_lastPingMs = ut::milliseconds_f64{ pingResults[i].latency }.count();
						color = _pingColor;
					}

					_pointBuffer.push_back({ x, calcY(_lastPingMs), color });

					auto dist{ abstime(pingResults[i].sentTime - selectionTime) };

					if (dist < selectionDistance)
					{
						selectionDistance = dist;

						_selection = i;
						_selectionTime = pingResults[i].sentTime;
						_selectionTimeMs = _lastPingMs;
					}
				}

				drawPrettyLines(canvas, rect, _plotThickness, 
					_pointBuffer.data(), _pointBuffer.size());

				const auto lineX{ fastround<pxindex>(calcX(selectionTime)) };

				if (rect.left < lineX && rect.right > lineX)
				{
					if (_selection < pingResults.size())
						// Implies (startIndex < pingResults.size())
						// and that selection properties have been set.
					{
						const auto x{ calcX(_selectionTime) };
						const auto y{ calcY(_selectionTimeMs) };

						std::array<Vertex, 5> vertices {
							Vertex{ x - 8, y + 8, _selectionColor },
							Vertex{ x - 8, y - 8, _selectionColor },
							Vertex{ x + 8, y - 8, _selectionColor },
							Vertex{ x + 8, y + 8, _selectionColor },
							Vertex{ x - 8, y + 8, _selectionColor },
						};

						drawPrettyLines(canvas, rect, _plotThickness, 
							vertices.data(), vertices.size());
					}

					if (drawSelectionLine)
					{
						const pxindex dist{ 20 };
						const pxindex height{ 8 };

						for (pxindex y{ rect.bottom - dist }; y > rect.top; y -= dist)
						{
							drawVerticalLine(canvas, rect, _lineColor, lineX, y, y + height);
						}
					}
				}
				else
				{
					_selection = pingResults.size();
					_selectionTime = now;
					_selectionTimeMs = 0.0;
				}
			}
		}
	};
}

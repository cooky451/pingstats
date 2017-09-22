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

#include "resource.h"

#include "utility/utility.hpp"
#include "utility/read_file.hpp"
#include "utility/scoped_thread.hpp"
#include "utility/tree_config.hpp"
#include "utility/waitable_flag.hpp"

#include "winapi/utility.hpp"
#include "window_messages.hpp"
#include "ping_monitor.hpp"
#include "ping_data.hpp"
#include "ping_plotter.hpp"

#include <array>
#include <atomic>
#include <future>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace pingstats // export
{
	using namespace utility::literals;

	namespace cr = std::chrono;
	namespace ut = utility;
	namespace wa = winapi;

	struct HandleMessageResult
	{
		LRESULT result;
		bool forward;
		bool destroy;
		bool quit;
	};

	class Section
	{
	public:
		Rect rect{};
		PingData data;
		PingPlotter plotter;
		PingMonitor monitor;

		Section(Section&&) = delete;

		Section(ut::TreeConfigNode& config, HWND resultHandler, WPARAM resultTag)
			: data{ config }
			, plotter{ config }
			, monitor{ config, resultHandler, resultTag }
		{}
	};

	class MainWindow
	{
		static constexpr char CONFIG_FILEPATH[14]{ "pingstats.cfg" };

		HWND _windowHandle;
		std::unique_ptr<wa::NotifyIcon> _notifyIcon;
		UINT _taskbarCreatedMessage;

		wa::DeviceContext _deviceContext;
		wa::MemoryCanvas _backBuffer;

		std::vector<std::unique_ptr<Section>> _sections;

		int _sectionWidth{ 480 };
		int _sectionHeight{ 320 };
		int _rows{};
		int _columns{};
		int _lastWidth{};
		int _lastHeight{};

		Color _clearColor{ 4, 4, 20 };

		Section* _selectedSection{};
		cr::steady_clock::time_point _selectionStart;
		cr::steady_clock::time_point _selectionTime;
		bool _alwaysOnTop{ false };

		ut::WaitableFlag _stopFlag;
		ut::AutojoinThread _tickThread;

		std::vector<std::future<void>> _writeOperations;

	public:
		~MainWindow()
		{
			_stopFlag.set();

			for (auto& op : _writeOperations)
			{
				op.wait();
			}
		}

		MainWindow(HWND windowHandle)
			: _windowHandle{ windowHandle }
			, _taskbarCreatedMessage{ RegisterWindowMessageW(L"TaskbarCreated") }
			, _deviceContext{ CreateCompatibleDC(nullptr) }
		{
			auto configFile{ ut::readFileAs<std::string>(CONFIG_FILEPATH) };

			ut::TreeConfigNode config{ nullptr, "config" };

			if (configFile.size() > 0 && !parseTreeConfig(config, configFile.c_str()))
			{
				wa::showMessageBox("Warning", "Error while parsing config.");
			}

			config.loadOrStore("clearColor", _clearColor);

			auto& hosts{ *config.findOrAppendNode("hosts") };

			if (hosts.children().size() == 0)
			{
				hosts.appendNode("WAN (Internet)")
					->storeValue("target", "trace public4 8.8.8.8");
				hosts.appendNode("LAN")
					->storeValue("target", "trace private4 8.8.8.8");
			}

			for (auto& host : hosts.children())
			{
				if (host->loadOrStoreIndirect("enabled", true))
				{
					auto strpos{ "auto"s };
					host->loadOrStore("position", strpos);

					if (strpos == "auto")
					{
						_sections.push_back(std::make_unique<Section>(
							*host, _windowHandle, _sections.size()));
					}
					else
					{
						const auto i{ std::min(512ul, std::stoul(strpos)) };

						if (_sections.size() <= i)
						{
							_sections.resize(1 + i);
						}

						_sections[i] = std::make_unique<Section>(*host, _windowHandle, i);
					}
				}
			}

			if (_sections.size() == 0)
			{
				throw std::runtime_error("No active hosts.");
			}

			const auto size{ static_cast<int>(_sections.size()) };

			auto strrows{ "auto"s };
			auto strcols{ "auto"s };

			config.loadOrStore("windowRows", strrows);
			config.loadOrStore("windowColumns", strcols);

			_rows = strrows == "auto" ? 0 : std::stoul(strrows);
			_columns = strcols == "auto" ? 0 : std::stoul(strcols);

			if (_rows == 0 && _columns == 0)
			{
				_columns = size / 3 + (size % 3 > 0 ? 1 : 0);
				_rows = size / _columns + (size % _columns > 0 ? 1 : 0);
			}

			if (_rows == 0)
			{
				_rows = size / _columns + (size % _columns > 0 ? 1 : 0);
			}

			if (_columns == 0)
			{
				_columns = size / _rows + (size % _rows > 0 ? 1 : 0);
			}

			config.loadOrStore("sectionWidth", _sectionWidth);
			config.loadOrStore("sectionHeight", _sectionHeight);
			config.loadOrStore("alwaysOnTop", _alwaysOnTop);

			remakeNotifyIcon();
			resizeWindowToDefaultSize();
			setAlwaysOnTop(_alwaysOnTop);

			auto refreshRate{ 30 };
			config.loadOrStore("refreshRate", refreshRate);

			refreshRate = std::min(240, std::max(1, refreshRate));

			ut::FileHandle file{ std::fopen(CONFIG_FILEPATH, "wt") };

			if (file.get() != nullptr)
			{
				auto cfgstr{ serializeTreeConfig(config) };
				std::fwrite(cfgstr.data(), 1, cfgstr.size(), file.get());
			}

			_tickThread = std::thread([this, refreshRate]
			{
				const auto refreshInterval{ 
					cr::duration_cast<cr::nanoseconds>(1.0_sf64 / refreshRate) };

				for (auto time{ cr::steady_clock::now() }; 
					!_stopFlag.waitUntil(time); time += refreshInterval)
				{
					PostMessageW(_windowHandle, WM_REDRAW, 0, 0);
				}
			});
		}

		void remakeNotifyIcon()
		{
			// Needs to be deleted *before* creating the new one.
			_notifyIcon = nullptr; 
			_notifyIcon = std::make_unique<wa::NotifyIcon>(_windowHandle,
				WM_NOTIFICATIONICON, MAKEINTRESOURCEW(ICON_DEFAULT));
		}

		void setAlwaysOnTop(bool alwaysOnTop)
		{
			SetWindowPos(_windowHandle, 
				alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 
				0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		void resizeWindowToDefaultSize()
		{
			RECT windowRect;
			GetWindowRect(_windowHandle, &windowRect);

			const auto width{ windowRect.right - windowRect.left };
			const auto height{ windowRect.bottom - windowRect.top };
			const auto centerX{ windowRect.left + width / 2 };
			const auto centerY{ windowRect.top + height / 2 };
			const auto defaultWidth{ _columns * _sectionWidth };
			const auto defaultHeight{ _rows * _sectionHeight };
			const auto x{ std::max(0l, centerX - defaultWidth / 2) };
			const auto y{ std::max(0l, centerY - defaultHeight / 2) };

			SetWindowPos(_windowHandle, HWND_TOP, x, y, 
				defaultWidth, defaultHeight, SWP_SHOWWINDOW);

			calcSectionRects();
		}

		void calcSectionRects()
		{
			RECT rect;
			GetClientRect(_windowHandle, &rect);

			static constexpr LONG BORDER_WIDTH{ 8 };

			const auto clientWidth{ rect.right - rect.left };
			const auto clientHeight{ rect.bottom - rect.top };
			const auto borderPixelsX{ (1 + _columns) * BORDER_WIDTH };
			const auto borderPixelsY{ (1 + _rows) * BORDER_WIDTH };
			const auto sectWidth{ (clientWidth - borderPixelsX) / (1.0 * _columns) };
			const auto sectHeight{ (clientHeight - borderPixelsY) / ( 1.0 * _rows) };

			for (std::size_t i{}; i < _sections.size(); ++i)
			{
				if (_sections[i] != nullptr)
				{
					const auto row{ static_cast<std::int32_t>(i % _rows) };
					const auto col{ static_cast<std::int32_t>(i / _rows) };

					const auto left{ fastround<LONG>(
						col * sectWidth + (1 + col) * BORDER_WIDTH) };

					const auto top{ fastround<LONG>(
						row * sectHeight + (1 + row) * BORDER_WIDTH) };

					const auto right{ left + static_cast<LONG>(sectWidth) };
					const auto bottom{ top + static_cast<LONG>(sectHeight) };

					_sections[i]->rect = Rect{ left, top, right, bottom };
				}
			}

			resizeCanvasPredictive(_backBuffer, clientWidth, clientHeight);

			_deviceContext.select(_backBuffer.get());
		}

		Section* findSection(int32_t mouseX, int32_t mouseY)
		{
			for (auto& section : _sections)
			{
				if (section != nullptr &&
					section->rect.left < mouseX &&
					section->rect.right > mouseX &&
					section->rect.top < mouseY &&
					section->rect.bottom > mouseY)
				{
					return section.get();
				}
			}

			return nullptr;
		}

		void setSelectedTimeAndRedraw(int32_t x)
		{
			if (_selectedSection != nullptr)
			{
				const auto pixelsPerSecond{ _selectedSection->plotter.pixelsPerSecond() };

				const auto pixelOffset{ _selectedSection->rect.right - x };
				const auto offset{ pixelOffset / pixelsPerSecond };

				_selectionTime = _selectionStart - cr::duration_cast
					<cr::nanoseconds>(ut::seconds_f64{ offset });

				PostMessageW(_windowHandle, WM_REDRAW, 0, 0);
			}
		}

		void drawWindow(HWND hwnd)
		{
			wa::PaintLock paintLock{ hwnd };

			if (_backBuffer.width() > 8 && _backBuffer.height() > 8)
			{
				clearCanvas(_backBuffer, _clearColor);

				const bool drawSelectionLine{ _selectedSection != nullptr };

				const auto now{ 
					drawSelectionLine ? _selectionStart : cr::steady_clock::now() };

				for (auto& section : _sections)
				{
					if (section != nullptr)
					{
						section->plotter.redraw(
							_backBuffer, 
							section->rect, 
							section->data,
							now, 
							_selectionTime, 
							drawSelectionLine, 
							_clearColor);
					}
				}

				BitBlt(paintLock.deviceContext(), 0, 0,
					_backBuffer.width(), _backBuffer.height(),
					_deviceContext.get(), 0, 0, SRCCOPY);
			}
		}

		void asyncWriteLogToFile(const std::string& filename,
			const std::vector<IcmpEchoResult>& pingResults,
			const std::vector<IcmpEchoResult>& traceResults)
		{
			_writeOperations.push_back(
				std::async(std::launch::async, 
					[filename, traceResults, pingResults]() {
						wa::showMessageBox("Information", "Composing log string.");

						ut::FileHandle file{ std::fopen(filename.c_str(), "wb") };

						if (file.get() != nullptr)
						{
							const auto traceStr{ makeLogString(traceResults) };
							const auto pingStr{ makeLogString(pingResults) };

							std::fwrite(traceStr.data(), 1, traceStr.size(), file.get());
							std::fwrite(pingStr.data(), 1, pingStr.size(), file.get());

							wa::showMessageBox("Information", "Finished writing log file.");
						}
						else
						{
							wa::showMessageBox("Error", "Failed to open file.");
						}
					}));
		}

		HandleMessageResult handleMessage(
			HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
		{
			switch (message)
			{
			default:
			{
				if (message == _taskbarCreatedMessage)
				{
					remakeNotifyIcon();
				}
			} break;

			case WM_CLOSE:
			{}	return{ 0, false, true, true };

			case WM_REDRAW:
			{
				RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
			}	return{ 0 };

			case WM_TRACE_RESULT:
			{
				_sections[wparam]->data.insertTraceResult(
					*reinterpret_cast<IcmpEchoResult*>(lparam));
			}	return{ 0 };

			case WM_PING_RESULT:
			{
				_sections[wparam]->data.insertPingResult(
					*reinterpret_cast<IcmpEchoResult*>(lparam));
			}	return{ 0 };

			case WM_CRITICAL_PING_MONITOR_ERROR:
			{
				if (lparam != 0)
				{
					auto e = reinterpret_cast<std::exception*>(lparam);
					wa::showMessageBox("Error", e->what());
				}
				else
				{
					wa::showMessageBox("Error", "Unknown error.");
				}
			}	return { 0 };

			case WM_PAINT:
			{
				drawWindow(hwnd);
			}	return{ 0 };

			case WM_ERASEBKGND:
			{}	return{ 1 }; // Tells Windows not to erase the background

			case WM_SIZE:
			{
				calcSectionRects();
				PostMessageW(hwnd, WM_REDRAW, 0, 0);
			}	return{ 0 };

			case WM_LBUTTONDOWN:
			{
				const auto x{ GET_X_LPARAM(lparam) };
				const auto y{ GET_Y_LPARAM(lparam) };

				_selectionStart = cr::steady_clock::now();
				_selectedSection = findSection(x, y);
				setSelectedTimeAndRedraw(x);
			}	return{ 0 };

			case WM_LBUTTONUP:
			{
				_selectedSection = nullptr;
				PostMessageW(hwnd, WM_REDRAW, 0, 0);
			}	return{ 0 };

			case WM_MOUSEMOVE:
			{
				setSelectedTimeAndRedraw(GET_X_LPARAM(lparam));
			}	return{ 0 };

			case WM_CONTEXTMENU:
			{
				const auto menu{ LoadMenuW(
					GetModuleHandleW(nullptr),
					MAKEINTRESOURCEW(CONTEXT_MENU)) };

				const auto popup{ GetSubMenu(menu, 0) };
				const auto flags{ TPM_TOPALIGN | TPM_LEFTALIGN | TPM_RETURNCMD };

				const POINT pos{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
				POINT cpt{ pos };
				ScreenToClient(hwnd, &cpt);

				const auto selection{ findSection(cpt.x, cpt.y) };

				switch (TrackPopupMenu(popup, flags, pos.x, pos.y, 0, hwnd, nullptr))
				{
				default:
				{}	break;

				case CONTEXT_MENU_RESET_SIZE:
				{
					resizeWindowToDefaultSize();
				}	break;

				case CONTEXT_MENU_COPY_HOSTNAME:
				{
					if (selection != nullptr)
					{
						wa::copyToClipboard(selection->data.lastResponder(), hwnd);
					}
				}	break;

				case CONTEXT_MENU_COPY_STATUS:
				{
					if (selection != nullptr)
					{
						wa::copyToClipboard(selection->plotter.statusString(), hwnd);
					}
				}	break;

				case CONTEXT_MENU_COPY_ROUTE:
				{
					if (selection != nullptr)
					{
						wa::copyToClipboard(
							makeLogString(selection->data.traceResults()), hwnd);
					}
				}	break;

				case CONTEXT_MENU_SAVE_LOG:
				{
					if (selection != nullptr)
					{
						constexpr DWORD FN_SIZE{ 512 };
						wchar_t filename[FN_SIZE] = L"pingstats-log.txt";

						OPENFILENAMEW saveFile = { sizeof saveFile };
						saveFile.hwndOwner = hwnd;
						saveFile.lpstrFilter = L".txt\0*.txt\0";
						saveFile.lpstrFile = filename;
						saveFile.nMaxFile = FN_SIZE;
						saveFile.Flags = OFN_LONGNAMES 
							| OFN_OVERWRITEPROMPT 
							| OFN_PATHMUSTEXIST;

						if (GetSaveFileNameW(&saveFile))
						{
							asyncWriteLogToFile(wa::utf8(filename), 
								selection->data.traceResults(), 
								selection->data.pingResults());
						}
					}
				}	break;

				case CONTEXT_MENU_ALWAYS_ON_TOP:
				{
					setAlwaysOnTop((_alwaysOnTop = !_alwaysOnTop));
				}	break;
				}
			}	return{ 0 };

			case WM_SYSCOMMAND:
			{
				if (wparam == SC_MINIMIZE)
				{
					ShowWindow(hwnd, SW_HIDE);
					return{ 0 };
				}
			}	break;

			case WM_NOTIFICATIONICON:
			{
				if (lparam == WM_RBUTTONUP)
				{
					POINT pos;
					GetCursorPos(&pos);

					const auto menu{ LoadMenuW(
						GetModuleHandleW(nullptr),
						MAKEINTRESOURCEW(TRAY_MENU)) };

					const auto popup{ GetSubMenu(menu, 0) };

					const auto flags{ TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON };

					// Required for correct menu behavior!
					SetForegroundWindow(hwnd);

					TrackPopupMenu(popup, flags, pos.x, pos.y, 0, hwnd, nullptr);

					// May be required for correct menu behavior?
					PostMessageW(hwnd, WM_NULL, 0, 0); 
				}
				else if (lparam == WM_LBUTTONDBLCLK)
				{
					if (IsWindowVisible(hwnd))
					{
						ShowWindow(hwnd, SW_HIDE);
					}
					else
					{
						ShowWindow(hwnd, SW_SHOW);
						SetForegroundWindow(hwnd);
					}

					return{ 0 };
				}
			}	break;

			case WM_COMMAND:
			{
				switch (LOWORD(wparam))
				{
				default:
				{}	break;

				case TRAY_MENU_SHOW:
				{
					ShowWindow(hwnd, SW_SHOW);
					SetForegroundWindow(hwnd);
				}	return{ 0 };

				case TRAY_MENU_CLOSE:
				{
					PostMessageW(hwnd, WM_CLOSE, 0, 0);
				}	return{ 0 };
				}
			}	break;
			}

			return{ 0, true };
		}
	};
}

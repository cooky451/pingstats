#pragma once

#include "graph_printer.hpp"
#include "pingstats_config.h"
#include "resource.h"

#include <cmath>
#include <algorithm>
#include <atomic>
#include <deque>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

class MainWindow
{
	NotifyIcon _notifyIcon;
	std::unique_ptr<PingstatsConfig> _psConfig;
	GraphPrinter _graphPrinter;
	std::chrono::steady_clock::time_point _lastForcedRedraw;

	bool _topmost = false;

public:
	MainWindow(HWND windowHandle)
		: _notifyIcon(windowHandle, WM_NOTIFICATIONICON, MAKEINTRESOURCEW(ICON_DEFAULT))
		, _psConfig(std::make_unique<PingstatsConfig>("pingstats.cfg"))
		, _graphPrinter(windowHandle, _psConfig->get())
	{
		(*_psConfig)->loadOrStore("Window.Topmost", _topmost);

		_psConfig.reset(); // Config no longer required, so save it to disk
	}

	void resizeWindowToDefaultSize(HWND hwnd)
	{
		// Resizes window without changing it's center point (unless it's off the screen)
		RECT windowRect;
		GetWindowRect(hwnd, &windowRect);
		const auto width = windowRect.right - windowRect.left;
		const auto height = windowRect.bottom - windowRect.top;
		const auto centerX = windowRect.left + width / 2;
		const auto centerY = windowRect.top + height / 2;
		const auto defaultRect = _graphPrinter.defaultWindowRect();
		const auto defaultWidth = defaultRect.right - defaultRect.left;
		const auto defaultHeight = defaultRect.bottom - defaultRect.top;
		const auto x = std::max(0l, centerX - defaultWidth / 2);
		const auto y = std::max(0l, centerY - defaultHeight / 2);
		SetWindowPos(hwnd, HWND_TOP, x, y, defaultWidth, defaultHeight, SWP_SHOWWINDOW);
		_graphPrinter.resetBackBufferSize();
	}

	void setTopmostIfConfigured(HWND hwnd)
	{
		if (_topmost)
		{
			SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_TOPMOST);
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
	}

	void drawWindow(HWND hwnd)
	{
		RECT clientRect;
		GetClientRect(hwnd, &clientRect);
		const auto clientWidth = std::uint16_t(clientRect.right - clientRect.left);
		const auto clientHeight = std::uint16_t(clientRect.bottom - clientRect.top);

		PaintLock paintLock(hwnd);

		// Locks the front buffer mutex and selects the front buffer.
		auto frontBufferLock = _graphPrinter.frontBufferLock();
		BitBlt(paintLock.deviceContext(), 0, 0, clientWidth, clientHeight, frontBufferLock.deviceContext(), 0, 0, SRCCOPY);
	}

	void resizeNotify()
	{
		using namespace std::chrono;

		if (steady_clock::now() >= _lastForcedRedraw + milliseconds(15))
		{
			_lastForcedRedraw = steady_clock::now();
			_graphPrinter.forceRedraw();
		}
	}
};

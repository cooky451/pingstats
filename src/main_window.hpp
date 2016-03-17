#pragma once

#include "graph_printer.hpp"
#include "resource.h"

#include <cmath>
#include <algorithm>
#include <atomic>
#include <deque>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

class MainWindow
{
	NotifyIcon _notifyIcon;
	GraphPrinter _graphPrinter;
	std::chrono::steady_clock::time_point _lastForcedRedraw;

public:
	MainWindow(HWND windowHandle)
		: _notifyIcon(windowHandle, WM_NOTIFICATIONICON, MAKEINTRESOURCEW(ICON_DEFAULT))
		, _graphPrinter(windowHandle)
	{}

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

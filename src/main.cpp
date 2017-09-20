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

#include "canvas_drawing.hpp"
#include "utility/utility.hpp"
#include "winapi/utility.hpp"
#include "main_window.hpp"

#include <memory>

#pragma comment(lib, "Winmm.lib") // timeBeginPeriod

using namespace std;
using namespace utility;
using namespace winapi;
using namespace pingstats;

namespace
{
	class TimePeriod
	{
		UINT _period;

	public:
		~TimePeriod()
		{
			timeEndPeriod(_period);
		}

		TimePeriod(UINT period)
			: _period{ period }
		{
			timeBeginPeriod(_period);
		}

		TimePeriod(TimePeriod&&) = delete;
	};

	LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) try
	{
		auto window{ reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA)) };

		switch (message)
		{
		default:
		{} break;

		case WM_CREATE:
		{
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, 
				reinterpret_cast<LONG_PTR>(new MainWindow(hwnd)));
		}	return 0;
		}

		if (window != nullptr)
		{
			const auto result{ window->handleMessage(hwnd, message, wparam, lparam) };

			if (result.destroy)
			{
				DestroyWindow(hwnd);

				delete window;

				if (result.quit)
				{
					PostQuitMessage(0);
				}
			}

			if (!result.forward)
			{
				return result.result;
			}
		}

		return DefWindowProcW(hwnd, message, wparam, lparam);
	}
	catch (std::exception& e)
	{
		SendMessageW(hwnd, WM_CLOSE, 0, 0);
		showMessageBox("Error", e.what());
		return 0;
	}
	catch (...)
	{
		SendMessageW(hwnd, WM_CLOSE, 0, 0);
		showMessageBox("Error", "Unknown error.");
		return 0;
	}
}

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, LPWSTR, int show) try
{
	static constexpr wchar_t SINGLE_INSTANCE_EVENT_NAME[]{ L"oNnAOn73JzWwWoCN" };

	auto singleInstanceEvent{ HandlePtr{ OpenEventW(
		EVENT_ALL_ACCESS, false, SINGLE_INSTANCE_EVENT_NAME) } };

	if (singleInstanceEvent != nullptr)
	{
		const auto decision{ showMessageBox("Warning", 
			"There is already an instance "
			"of pingstats running. Start anyway?", MB_OKCANCEL) };

		if (decision != IDOK)
		{
			return 0;
		}
	}
	else
	{
		singleInstanceEvent.reset(CreateEventW(
			nullptr, false, false, SINGLE_INSTANCE_EVENT_NAME));
	}

	// For debugging.
	//AllocConsole();
	//freopen("CONOUT$", "w", stdout);

	// Increases sleep/wait resolution
	TimePeriod timePeriod{ 1 };

	// Prevents Windows memory leak https://support.microsoft.com/en-us/kb/2384321
	IcmpFileHandle icmpDummy{ IcmpCreateFile() };

	static constexpr wchar_t WND_CLASSNAME[]{ L"MainWindowClass" };
	static constexpr wchar_t WND_TITLE[]{ L"pingstats v2.0.2" };

	WNDCLASSEXW windowClassEx{};
	windowClassEx.cbClsExtra = 0;
	windowClassEx.cbSize = sizeof windowClassEx;
	windowClassEx.cbWndExtra = 0;
	windowClassEx.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
	windowClassEx.hCursor = LoadCursorW(nullptr, IDC_ARROW);
	windowClassEx.hIcon = LoadIconW(hinstance, MAKEINTRESOURCEW(ICON_DEFAULT));
	windowClassEx.hIconSm = LoadIconW(hinstance, MAKEINTRESOURCEW(ICON_DEFAULT));
	windowClassEx.hInstance = hinstance;
	windowClassEx.lpfnWndProc = windowProc;
	windowClassEx.lpszClassName = WND_CLASSNAME;
	windowClassEx.lpszMenuName = nullptr;
	windowClassEx.style = CS_HREDRAW | CS_VREDRAW;

	if (!RegisterClassExW(&windowClassEx))
	{
		throw WindowsError{ "RegisterClassEx()" };
	}

	const auto hwnd{ CreateWindowExW(
		0, WND_CLASSNAME, WND_TITLE, 
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 
		640, 640, 
		nullptr, nullptr, 
		hinstance, nullptr) };

	if (hwnd == nullptr)
	{
		throw WindowsError{ "CreateWindowEx()" };
	}

	ShowWindow(hwnd, show);
	UpdateWindow(hwnd);

	MSG message;

	while (GetMessageW(&message, nullptr, 0, 0) > 0)
	{
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}

	return static_cast<int>(message.wParam);
}
catch (std::exception& e)
{
	showMessageBox("Error", e.what());
	return 0;
}
catch (...)
{
	showMessageBox("Error", "Unknown error.");
	return 0;
}

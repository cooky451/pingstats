/* 
 * 
 */

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

#include "windows/utility.hpp"
#include "main_window.hpp"

#include <memory>

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
#if defined _WIN64
	try // Exceptions get propagated on 32-bit executables.
#endif
	{
		auto mainWindow = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

		switch (message)
		{
		default:
		{} break;

		case WM_CREATE:
		{
			mainWindow = new MainWindow(hwnd);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(mainWindow));

			mainWindow->resizeWindowToDefaultSize(hwnd);
		}	return 0;

		case WM_CLOSE:
		{
			ShowWindow(hwnd, SW_HIDE);
			delete mainWindow;
			DestroyWindow(hwnd);
			PostQuitMessage(0);
		}	return 0;

		case WM_PAINT:
		{
			mainWindow->drawWindow(hwnd);
		}	return 0;

		case WM_ERASEBKGND:
		{}	return 1;

		case WM_SIZE:
		{
			mainWindow->resizeNotify();
		}	return 0;

		case WM_LBUTTONDBLCLK:
		{
			mainWindow->resizeWindowToDefaultSize(hwnd);
		}	return 0;

		case WM_SYSCOMMAND:
		{
			switch (wparam)
			{
			default:
			{}	break;

			case SC_MINIMIZE:
			{
				ShowWindow(hwnd, SW_HIDE);
			} return 0;
			}
		}	break;

		case WM_NOTIFICATIONICON:
		{
			if (wparam == 0)
			{
				switch (lparam)
				{
				default:
				{}	break;

				case WM_RBUTTONDOWN:
				{
					POINT p;
					GetCursorPos(&p);

					auto hmenu = LoadMenuW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(TRAY_MENU));
					auto htrack = GetSubMenu(hmenu, 0);

					switch (TrackPopupMenu(htrack, TPM_RIGHTALIGN | TPM_RETURNCMD, p.x, p.y, 0, hwnd, nullptr))
					{
					default:
						break;

					case TRAY_MENU_SHOW:
						ShowWindow(hwnd, SW_SHOW);
						return 0;

					case TRAY_MENU_CLOSE:
						PostMessageW(hwnd, WM_CLOSE, 0, 0);
						return 0;
					}
				}	break;

				case WM_LBUTTONDBLCLK:
				{
					if (IsWindowVisible(hwnd))
					{
						ShowWindow(hwnd, SW_HIDE);
					}
					else
					{
						PostMessageW(hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
						ShowWindow(hwnd, SW_SHOW);
					}
				}	return 0;
				}
			}
		}	break;
		}
	}
#if defined _WIN64
	catch (std::exception& e)
	{
		showMessageBox("Fatal Error", e.what());
		PostMessageW(hwnd, WM_CLOSE, 0, 0);
	}
	catch (...)
	{
		showMessageBox("Fatal Error", "Fatal Error");
		PostMessageW(hwnd, WM_CLOSE, 0, 0);
	}

#endif

	return DefWindowProcW(hwnd, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE hinstance, HINSTANCE, LPWSTR, int show)
{
	try
	{
		auto singleInstanceEventName = L"oNnAOn73JzWwWoCN";
		auto singleInstanceEvent = HandlePtr(OpenEventW(EVENT_ALL_ACCESS, false, singleInstanceEventName));

		if (singleInstanceEvent != nullptr)
		{
			auto decision = showMessageBox("Warning", "There is already an instance "
				"of pingstats running. Start anyway?", nullptr, MB_OKCANCEL);

			if (decision != IDOK)
			{
				return 0;
			}
		}
		else
		{
			singleInstanceEvent.reset(CreateEventW(nullptr, false, false, singleInstanceEventName));
		}

		const auto windowClassName = L"MainWindowClass";
		const auto windowTitle = L"Ping Monitor";

		BrushPtr backgroundBrush(CreateSolidBrush(GraphPrinter::DARK_BLUE));

		WNDCLASSEXW windowClassEx = {};
		windowClassEx.cbClsExtra = 0;
		windowClassEx.cbSize = sizeof windowClassEx;
		windowClassEx.cbWndExtra = 0;
		windowClassEx.hbrBackground = backgroundBrush.get();
		windowClassEx.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		windowClassEx.hIcon = LoadIconW(hinstance, MAKEINTRESOURCEW(ICON_DEFAULT));
		windowClassEx.hIconSm = LoadIconW(hinstance, MAKEINTRESOURCEW(ICON_DEFAULT));
		windowClassEx.hInstance = hinstance;
		windowClassEx.lpfnWndProc = windowProc;
		windowClassEx.lpszClassName = windowClassName;
		windowClassEx.lpszMenuName = nullptr;
		windowClassEx.style = CS_OWNDC | CS_DBLCLKS;

		if (!RegisterClassExW(&windowClassEx))
		{
			throw WindowsError("RegisterClassEx()");
		}

		auto hwnd = CreateWindowExW(0, windowClassName, windowTitle, WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, 600, 300, nullptr, nullptr, hinstance, nullptr);

		if (hwnd == nullptr)
		{
			throw WindowsError("CreateWindowEx()");
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
		showMessageBox("Fatal Error", e.what());
	}
	catch (...)
	{
		showMessageBox("Fatal Error", "Fatal Error");
	}

	return 0;
}

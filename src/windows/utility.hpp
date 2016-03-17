#pragma once

#include "base.hpp"
#include "utf.hpp"
#include "error.hpp"

#include <algorithm>
#include <atomic>
#include <memory>

struct GlobalMemoryDeleter
{
	void operator () (HGLOBAL globalMemory)
	{
		GlobalFree(globalMemory);
	}
};

struct HandleCloser
{
	void operator () (HANDLE handle)
	{
		CloseHandle(handle);
	}
};

struct GDIObjectDeleter
{
	void operator () (HGDIOBJ object)
	{
		DeleteObject(object);
	}
};

typedef std::unique_ptr<std::remove_pointer<HGLOBAL>::type, GlobalMemoryDeleter> GlobalMemoryPtr;
typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, HandleCloser> HandlePtr;
typedef std::unique_ptr<std::remove_pointer<HBRUSH>::type, GDIObjectDeleter> BrushPtr;
typedef std::unique_ptr<std::remove_pointer<HFONT>::type, GDIObjectDeleter> FontPtr;
typedef std::unique_ptr<std::remove_pointer<HDC>::type, GDIObjectDeleter> DeviceContextPtr;
typedef std::unique_ptr<std::remove_pointer<HBITMAP>::type, GDIObjectDeleter> BitmapPtr;
typedef std::unique_ptr<std::remove_pointer<HPEN>::type, GDIObjectDeleter> PenPtr;

class GlobalMemoryLock
{
	HGLOBAL _globalMemory;
	void* _pointer;

public:
	~GlobalMemoryLock()
	{
		GlobalUnlock(_globalMemory);
	}

	GlobalMemoryLock(GlobalMemoryLock&&) = delete;

	GlobalMemoryLock(HGLOBAL globalMemory)
		: _globalMemory(globalMemory)
		, _pointer(GlobalLock(globalMemory))
	{
		if (_pointer == nullptr)
		{
			throw WindowsError("GlobalLock");
		}
	}

	void* get()
	{
		return _pointer;
	}

	const void* get() const
	{
		return _pointer;
	}
};

class PaintLock
{
	HWND _windowHandle;
	PAINTSTRUCT _paintStruct;
	HDC _deviceContext;

public:
	~PaintLock()
	{
		EndPaint(_windowHandle, &_paintStruct);
	}

	PaintLock(PaintLock&&) = delete;

	PaintLock(HWND windowHandle)
		: _windowHandle(windowHandle)
		, _deviceContext(BeginPaint(_windowHandle, &_paintStruct))
	{}

	HDC deviceContext()
	{
		return _deviceContext;
	}
};

class DeviceContext
{
	DeviceContextPtr _deviceContext;
	HGDIOBJ _oldBitmap = nullptr;
	HGDIOBJ _oldBrush = nullptr;
	HGDIOBJ _oldFont = nullptr;
	HGDIOBJ _oldPen = nullptr;
	HGDIOBJ _oldRegion = nullptr;

public:
	~DeviceContext()
	{
		if (_deviceContext != nullptr)
		{
			if (_oldBitmap != nullptr)
			{
				SelectObject(_deviceContext.get(), _oldBitmap);
			}
			
			if (_oldBrush != nullptr)
			{
				SelectObject(_deviceContext.get(), _oldBrush);
			}

			if (_oldFont != nullptr)
			{
				SelectObject(_deviceContext.get(), _oldFont);
			}

			if (_oldPen != nullptr)
			{
				SelectObject(_deviceContext.get(), _oldPen);
			}

			if (_oldRegion != nullptr)
			{
				SelectObject(_deviceContext.get(), _oldRegion);
			}
		}
	}

	DeviceContext(DeviceContext&&) = delete;

	DeviceContext(HDC deviceContext)
		: _deviceContext(deviceContext)
	{}

	HDC get()
	{
		return _deviceContext.get();
	}

	void select(HBITMAP newBitmap)
	{
		auto oldBitmap = SelectObject(_deviceContext.get(), newBitmap);

		if (_oldBitmap == nullptr)
		{
			_oldBitmap = oldBitmap;
		}
	}

	void select(HBRUSH newBrush)
	{
		auto oldBrush = SelectObject(_deviceContext.get(), newBrush);

		if (_oldBrush == nullptr)
		{
			_oldBrush = oldBrush;
		}
	}

	void select(HFONT newFont)
	{
		auto oldFont = SelectObject(_deviceContext.get(), newFont);

		if (_oldFont == nullptr)
		{
			_oldFont = oldFont;
		}
	}

	void select(HPEN newPen)
	{
		auto oldPen = SelectObject(_deviceContext.get(), newPen);

		if (_oldPen == nullptr)
		{
			_oldPen = oldPen;
		}
	}

	void select(HRGN newRegion)
	{
		auto oldRegion = SelectObject(_deviceContext.get(), newRegion);

		if (_oldRegion == nullptr)
		{
			_oldRegion = oldRegion;
		}
	}
};

class MemoryCanvas
{
	HDC _compatibleContext;
	DeviceContext _deviceContext;
	BitmapPtr _buffer;
	std::uint32_t* _memoryPtr;
	std::uint16_t _bufferWidth;
	std::uint16_t _bufferHeight;

public:
	MemoryCanvas(HDC compatibleContext, std::uint16_t bufferWidth, std::uint16_t bufferHeight)
		: _compatibleContext(compatibleContext)
		, _deviceContext(CreateCompatibleDC(_compatibleContext))
	{
		reset(bufferWidth, bufferHeight);
	}

	std::uint16_t width() const
	{
		return _bufferWidth;
	}

	std::uint16_t height() const
	{
		return _bufferHeight;
	}

	HDC deviceContext()
	{
		return _deviceContext.get();
	}

	std::uint32_t* memoryPtr()
	{
		return _memoryPtr;
	}

	void reset(std::uint16_t bufferWidth, std::uint16_t bufferHeight)
	{
		_bufferWidth = bufferWidth;
		_bufferHeight = bufferHeight;

		BITMAPINFO bitmapInfo = { BITMAPINFOHEADER{ sizeof bitmapInfo.bmiHeader,
			_bufferWidth, _bufferHeight, 1, 32, BI_RGB },{} };

		_buffer.reset(CreateDIBSection(_compatibleContext, &bitmapInfo,
			DIB_RGB_COLORS, reinterpret_cast<void**>(&_memoryPtr), nullptr, 0));

		_deviceContext.select(_buffer.get());
	}

	template <typename T>
	void select(T gdiObject)
	{
		_deviceContext.select(gdiObject);
	}
};

class NotifyIcon
{
	NOTIFYICONDATAW _notifyIconData = {};

public:
	~NotifyIcon()
	{
		Shell_NotifyIconW(NIM_DELETE, &_notifyIconData);
	}

	NotifyIcon(NotifyIcon&&) = delete;

	NotifyIcon(HWND hwnd, UINT callbackMessage, const wchar_t* iconName)
	{
		_notifyIconData.cbSize = sizeof _notifyIconData;
		_notifyIconData.uID = 0;
		_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		_notifyIconData.hIcon = LoadIconW(GetModuleHandleW(nullptr), iconName);
		_notifyIconData.uCallbackMessage = callbackMessage;
		_notifyIconData.uVersion = NOTIFYICON_VERSION_4;
		_notifyIconData.hWnd = hwnd;
		Shell_NotifyIconW(NIM_ADD, &_notifyIconData);
	}

	void setText(const std::string& text)
	{
		static const auto maxLength = std::uint32_t(sizeof(_notifyIconData.szTip) - 1) / 2;
		auto wideText = toWideString(text);
		auto textLength = std::min(maxLength, std::uint32_t(wideText.size()));

		std::memcpy(_notifyIconData.szTip, wideText.c_str(), textLength * 2);
		_notifyIconData.szTip[textLength] = 0;

		Shell_NotifyIconW(NIM_MODIFY, &_notifyIconData);
	}
};

class ClipboardLock
{
	static std::uint32_t& locks()
	{
		thread_local std::uint32_t nLocks = 0;
		return nLocks;
	}

public:
	~ClipboardLock()
	{
		if (locks() > 0 && --locks() == 0)
		{
			CloseClipboard();
		}
	}

	ClipboardLock(ClipboardLock&&) = delete;

	ClipboardLock()
	{
		if (locks()++ == 0)
		{
			locks() = OpenClipboard(nullptr) ? 1 : 0;
		}
	}

	explicit operator bool() const
	{
		return locks() > 0;
	}
};

inline int showMessageBox(const char* title, const char* text, HWND owner = nullptr, UINT type = MB_OK)
{
	return MessageBoxW(owner, toWideString(text).c_str(), toWideString(title).c_str(), type);
}

inline std::string getWindowText(HWND hwnd)
{
	wchar_t windowText[0x1000];
	auto windowTextSize = GetWindowTextW(hwnd, windowText, sizeof windowText / sizeof *windowText);
	return toUtf8(windowText, windowTextSize);
}

inline void setWindowText(HWND hwnd, const std::string& windowText)
{
	SetWindowTextW(hwnd, toWideString(windowText).c_str());
}

inline void copyToClipboard(const std::string& clipboardString)
{
	auto wideString = toWideString(clipboardString);
	auto wideStringBytes = (wideString.size() + 1) * sizeof wideString[0];
	auto globalMemory = GlobalMemoryPtr(GlobalAlloc(GMEM_MOVEABLE, wideStringBytes));

	if (globalMemory == nullptr)
	{
		throw WindowsError("GlobalAlloc");
	}

	{
		GlobalMemoryLock memoryLock(globalMemory.get());
		std::memcpy(memoryLock.get(), &wideString[0], wideStringBytes);
	}

	ClipboardLock clipboardLock;
	if (clipboardLock)
	{
		EmptyClipboard();
		SetClipboardData(CF_UNICODETEXT, globalMemory.release());
	}
}

inline std::string copyFromClipboard()
{
	ClipboardLock clipboardLock;
	if (clipboardLock)
	{
		auto clipboardData = GetClipboardData(CF_UNICODETEXT);
		GlobalMemoryLock memoryLock(clipboardData);
		std::string clipboardString = toUtf8(static_cast<wchar_t*>(memoryLock.get()));
		return clipboardString;
	}

	return "";
}

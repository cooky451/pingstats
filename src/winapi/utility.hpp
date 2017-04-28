#pragma once

#include "base.hpp"
#include "utf.hpp"
#include "error.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>

namespace winapi // export
{
	struct GlobalFreeType
	{
		void operator () (HGLOBAL globalMemory)
		{
			GlobalFree(globalMemory);
		}
	};

	struct CloseHandleType
	{
		void operator () (HANDLE handle)
		{
			CloseHandle(handle);
		}
	};

	struct DeleteObjectType
	{
		void operator () (HGDIOBJ object)
		{
			DeleteObject(object);
		}
	};

	using GlobalMemoryPtr = 
		std::unique_ptr<std::remove_pointer<HGLOBAL>::type, GlobalFreeType>;
	using HandlePtr = 
		std::unique_ptr<std::remove_pointer<HANDLE>::type, CloseHandleType>;
	using BrushPtr = 
		std::unique_ptr<std::remove_pointer<HBRUSH>::type, DeleteObjectType>;
	using FontPtr = 
		std::unique_ptr<std::remove_pointer<HFONT>::type, DeleteObjectType>;
	using DeviceContextPtr = 
		std::unique_ptr<std::remove_pointer<HDC>::type, DeleteObjectType>;
	using BitmapPtr = 
		std::unique_ptr<std::remove_pointer<HBITMAP>::type, DeleteObjectType>;
	using PenPtr = 
		std::unique_ptr<std::remove_pointer<HPEN>::type, DeleteObjectType>;

	class GlobalMemoryLock
	{
		HGLOBAL _globalMemory{};
		void* _pointer{};

	public:
		~GlobalMemoryLock()
		{
			GlobalUnlock(_globalMemory);
		}

		GlobalMemoryLock(GlobalMemoryLock&&) = delete;

		GlobalMemoryLock(HGLOBAL globalMemory)
			: _globalMemory{ globalMemory }
			, _pointer{ GlobalLock(globalMemory) }
		{
			if (_pointer == nullptr)
			{
				throw WindowsError{ "GlobalLock()" };
			}
		}

		auto get()
		{
			return _pointer;
		}

		auto get() const
		{
			return _pointer;
		}
	};

	class PaintLock
	{
		HWND _windowHandle{};
		HDC _deviceContext{};
		PAINTSTRUCT _paintStruct;

	public:
		~PaintLock()
		{
			EndPaint(_windowHandle, &_paintStruct);
		}

		PaintLock(PaintLock&&) = delete;

		PaintLock(HWND windowHandle)
			: _windowHandle{ windowHandle }
			, _deviceContext{ BeginPaint(_windowHandle, &_paintStruct) }
		{}

		HDC deviceContext()
		{
			return _deviceContext;
		}
	};

	class DeviceContext
	{
		enum ObjectIndex
		{
			BITMAP = 0,
			BRUSH,
			FONT,
			PEN,
			REGION,
			INVALID,
		};

		DeviceContextPtr _deviceContext;
		std::array<HGDIOBJ, ObjectIndex::INVALID> _oldObjects{};

	public:
		~DeviceContext()
		{
			selectDefaults();
		}

		DeviceContext(DeviceContext&& other) noexcept
			: DeviceContext{}
		{
			*this = std::move(other);
		}

		constexpr DeviceContext() = default;

		constexpr explicit DeviceContext(HDC deviceContext)
			: _deviceContext{ deviceContext }
		{}

		DeviceContext& operator = (DeviceContext&& other)
		{
			selectDefaults();
			_deviceContext = std::move(other._deviceContext);
			_oldObjects = other._oldObjects;
			other._oldObjects = {};
			return *this;
		}

		HDC get()
		{
			return _deviceContext.get();
		}

		void selectDefaults()
		{
			if (_deviceContext != nullptr)
			{
				for (auto& oldObject : _oldObjects)
				{
					if (oldObject != nullptr)
					{
						SelectObject(_deviceContext.get(), oldObject);
					}
				}
			}
		}

		void select(HBITMAP newBitmap)
		{
			auto oldBitmap = SelectObject(_deviceContext.get(), newBitmap);

			if (_oldObjects[ObjectIndex::BITMAP] == nullptr)
			{
				_oldObjects[ObjectIndex::BITMAP] = oldBitmap;
			}
		}

		void select(HBRUSH newBrush)
		{
			auto oldBrush = SelectObject(_deviceContext.get(), newBrush);

			if (_oldObjects[ObjectIndex::BRUSH] == nullptr)
			{
				_oldObjects[ObjectIndex::BRUSH] = oldBrush;
			}
		}

		void select(HFONT newFont)
		{
			auto oldFont = SelectObject(_deviceContext.get(), newFont);

			if (_oldObjects[ObjectIndex::FONT] == nullptr)
			{
				_oldObjects[ObjectIndex::FONT] = oldFont;
			}
		}

		void select(HPEN newPen)
		{
			auto oldPen = SelectObject(_deviceContext.get(), newPen);

			if (_oldObjects[ObjectIndex::PEN] == nullptr)
			{
				_oldObjects[ObjectIndex::PEN] = oldPen;
			}
		}

		void select(HRGN newRegion)
		{
			auto oldRegion = SelectObject(_deviceContext.get(), newRegion);

			if (_oldObjects[ObjectIndex::REGION] == nullptr)
			{
				_oldObjects[ObjectIndex::REGION] = oldRegion;
			}
		}
	};

	class MemoryCanvas
	{
		BitmapPtr _bitmap;
		std::uint32_t* _pixelPtr{};
		LONG _width{};
		LONG _height{};

	public:
		MemoryCanvas(MemoryCanvas&& other)
			: MemoryCanvas{}
		{
			*this = std::move(other);
		}

		constexpr MemoryCanvas() = default;

		MemoryCanvas(LONG width, LONG height, bool isTopDown = true)
			: _width{ width }
			, _height{ height }
		{
			if (_width != 0 && _height != 0)
			{
				const auto heightHack{ isTopDown ? -_height : _height };

				BITMAPINFO bitmapInfo{ BITMAPINFOHEADER{ 
					sizeof bitmapInfo.bmiHeader, 
					_width, heightHack, 1, 32, BI_RGB }, {} };

				_bitmap = BitmapPtr{ CreateDIBSection(
					nullptr, &bitmapInfo, DIB_RGB_COLORS, 
					reinterpret_cast<void**>(&_pixelPtr), nullptr, 0) };

				if (_bitmap == nullptr)
				{
					throw WindowsError("CreateDIBSection()");
				}
			}
		}

		MemoryCanvas& operator = (MemoryCanvas&& other)
		{
			_bitmap = std::move(other._bitmap);
			_pixelPtr = other._pixelPtr;
			_width = other._width;
			_height = other._height;
			other._pixelPtr = {};
			other._width = {};
			other._height = {};
			return *this;
		}

		auto get()
		{
			return _bitmap.get();
		}

		auto width() const
		{
			return _width;
		}

		auto height() const
		{
			return _height;
		}

		auto size() const
		{
			return width() * static_cast<std::size_t>(height());
		}

		auto& operator () (std::size_t x, std::size_t y) const
		{
			return _pixelPtr[x + y * width()];
		}

		auto& operator () (std::size_t x, std::size_t y)
		{
			return _pixelPtr[x + y * width()];
		}

		auto pixelPtr()
		{
			return _pixelPtr;
		}
	};

	class NotifyIcon
	{
		NOTIFYICONDATAW _notifyIconData{};

	public:
		~NotifyIcon()
		{
			Shell_NotifyIconW(NIM_DELETE, &_notifyIconData);
		}

		NotifyIcon(NotifyIcon&&) = delete;

		NotifyIcon(HWND hwnd, UINT callbackMessage, const wchar_t* iconName, UINT id = {})
		{
			_notifyIconData.cbSize = sizeof _notifyIconData;
			_notifyIconData.uID = id;
			_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
			_notifyIconData.hIcon = LoadIconW(GetModuleHandleW(nullptr), iconName);
			_notifyIconData.uCallbackMessage = callbackMessage;
			_notifyIconData.uVersion = NOTIFYICON_VERSION_4;
			_notifyIconData.hWnd = hwnd;

			Shell_NotifyIconW(NIM_ADD, &_notifyIconData);
		}

		void setText(std::string_view text)
		{
			static constexpr auto bufferSize{
				sizeof _notifyIconData.szTip / sizeof _notifyIconData.szTip[0] };

			wstr(_notifyIconData.szTip, bufferSize, text.data(), text.size());

			Shell_NotifyIconW(NIM_MODIFY, &_notifyIconData);
		}
	};

	class ClipboardLock
	{
		bool _isOpen;

	public:
		~ClipboardLock()
		{
			if (_isOpen)
			{
				CloseClipboard();
			}
		}

		ClipboardLock(ClipboardLock&&) = delete;

		ClipboardLock(HWND newOwner)
			: _isOpen{ !!OpenClipboard(newOwner) }
		{}

		explicit operator bool() const
		{
			return _isOpen;
		}
	};

	inline std::string getCurrentModuleFileName()
	{
		static constexpr DWORD bufferSize{ 0x200 };

		wchar_t buffer[bufferSize];

		const auto size{ GetModuleFileNameW(nullptr, buffer, bufferSize) };

		return utf8(buffer, size);
	}

	inline int showMessageBox(
		std::string_view title, 
		std::string_view text,
		UINT type = { MB_OK }, 
		HWND owner = {})
	{
		const auto wtitle{ wstr(title.data(), title.size()) };
		const auto wtext{ wstr(text.data(), text.size()) };
		return MessageBoxW(owner, wtext.c_str(), wtitle.c_str(), type);
	}

	inline std::string getWindowText(HWND hwnd)
	{
		std::wstring buffer;

		const auto textSize{ GetWindowTextLengthW(hwnd) };

		if (textSize > 0)
		{
			buffer.resize(static_cast<std::size_t>(textSize));

			// legal since LWG 2475
			GetWindowTextW(hwnd, buffer.data(), textSize + 1);
		}

		return utf8(buffer);
	}

	inline void setWindowText(HWND hwnd, std::string_view text)
	{
		SetWindowTextW(hwnd, wstr(text.data(), text.size()).c_str());
	}

	inline bool copyToClipboard(std::string_view str, HWND newOwner = {})
	{
		const auto widestr{ wstr(str.data(), str.size()) };
		const auto nBytes{ (widestr.size() + 1) * sizeof widestr[0] };

		GlobalMemoryPtr globalMemory{ GlobalAlloc(GMEM_MOVEABLE, nBytes) };

		if (globalMemory == nullptr)
		{
			throw WindowsError("GlobalAlloc()");
		}

		{
			GlobalMemoryLock lock{ globalMemory.get() };

			std::memcpy(lock.get(), widestr.c_str(), nBytes);
		}

		ClipboardLock clipboardLock{ newOwner };

		if (clipboardLock)
		{
			EmptyClipboard();
			SetClipboardData(CF_UNICODETEXT, globalMemory.release());

			return true;
		}

		return false;
	}

	inline std::string copyFromClipboard(HWND newOwner = {})
	{
		ClipboardLock clipboardLock{ newOwner };

		if (clipboardLock)
		{
			const auto hglobal{ GetClipboardData(CF_UNICODETEXT) };

			if (hglobal != nullptr)
			{
				GlobalMemoryLock lock{ hglobal };
				return utf8(static_cast<wchar_t*>(lock.get()));
			}
		}

		return std::string{};
	}

	void centerWindowOnScreen(HWND hwnd)
	{
		RECT rc;
		GetWindowRect(hwnd, &rc);

		const auto width{ rc.right - rc.left };
		const auto height{ rc.bottom - rc.top };

		const auto x{ (GetSystemMetrics(SM_CXSCREEN) - width) / 2 };
		const auto y{ (GetSystemMetrics(SM_CYSCREEN) - height) / 2 };

		SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE);
	}
}

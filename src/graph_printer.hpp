#pragma once

#include <vector>

#include "utility/utility.hpp"
#include "utility/waitable_flag.hpp"
#include "utility/scoped_thread.hpp"
#include "utility/property_node.hpp"
#include "windows/utility.hpp"
#include "ping_monitor.hpp"

class GraphPrinter
{
public:
	static const auto DARK_BLUE = RGB(3, 3, 15);
	static const auto GREEN = RGB(63, 255, 63);
	static const auto BLUE = RGB(63, 63, 255);
	static const auto RED = RGB(255, 63, 63);
	static const auto YELLOW = RGB(255, 255, 63);
	static const auto WHITE = RGB(255, 255, 255);
	static const std::uint32_t BACKGROUND_COLOR = 0x0003030F;
	static const std::uint16_t ANTI_ALIASING = 2;

	class FrontBufferLock
	{
		std::unique_lock<std::mutex> _mutexLock;
		HDC _deviceContext;

	public:
		FrontBufferLock(std::mutex& bufferMutex, HDC deviceContext)
			: _mutexLock(bufferMutex)
			, _deviceContext(deviceContext)
		{}

		HDC deviceContext()
		{
			return _deviceContext;
		}
	};

	class PingMonitorProperties
	{
	public:
		std::unique_ptr<PingMonitor> pingMonitor;

		std::string address = "auto4";
		std::uint32_t delayMs = 500;
		std::uint32_t timeoutMs = 2000;
		bool async = false;

		std::string logfilename;
		bool drawMeanGraph = false;
		bool drawJitterGraph = true;
		bool drawLossGraph = true;

		double pixelPerSecond = 4.0;

		double maxGraphPing = 20.0;
		double maxPing = 0.0;

		void makeMonitor()
		{
			FileHandle file;

			if (logfilename.size() > 0)
			{
				file = FileHandle(std::fopen(logfilename.c_str(), "at"));
			}

			pingMonitor = std::make_unique<PingMonitor>(address, delayMs, timeoutMs, async, std::move(file));
		}
	};

private:
	HWND _windowHandle;
	HDC _windowDeviceContext;

	std::uint64_t _frameCounter = 0;
	const double _bottomBorder = 64.0;

	std::uint16_t _penThickness = ANTI_ALIASING;
	std::uint16_t _drawDelayMs = 500;
	bool _alwaysRedraw = false;

	FontPtr _consolasFont;
	BrushPtr _backgroundBrush;
	PenPtr _greenPen;
	PenPtr _bluePen;
	PenPtr _redPen;
	PenPtr _yellowPen;
	PenPtr _whitePen;

	std::unique_ptr<MemoryCanvas> _frontBuffer;
	std::unique_ptr<MemoryCanvas> _backBuffer;
	std::unique_ptr<MemoryCanvas> _highResBackBuffer;

	std::vector<PingMonitorProperties> _pingMonitors;

	WaitableFlag _cancelSleepFlag = false;
	std::atomic_bool _forceRedrawFlag = false;
	std::atomic_bool _stopThreadFlag = false;
	std::atomic_bool _resetBackBufferSizeFlag = false;

	RECT _defaultWindowRect;

	mutable std::mutex _frontBufferMutex;
	autojoin_thread _backgroundThread;

public:
	~GraphPrinter()
	{
		_stopThreadFlag = true;
		cancelSleep();
	}

	GraphPrinter(HWND windowHandle)
		: _windowHandle(windowHandle)
		, _windowDeviceContext(GetDC(windowHandle))
		, _frontBuffer(std::make_unique<MemoryCanvas>(_windowDeviceContext, 0, 0))
		, _backBuffer(std::make_unique<MemoryCanvas>(_windowDeviceContext, 0, 0))
		, _highResBackBuffer(std::make_unique<MemoryCanvas>(_windowDeviceContext, 0, 0))
	{
		auto config = PropertyNode::makeAndLinkWithFile("ipstats.cfg");

		config->loadOrStore("Graph.PenThickness", _penThickness);
		config->loadOrStore("Timing.DrawDelay", _drawDelayMs);
		config->loadOrStore("Timing.AlwaysRedraw", _alwaysRedraw);

		auto hosts = config->findOrAppendNode("hosts");

		if (hosts->nodes().size() == 0)
		{
			hosts->appendNode("default");
		}

		for (auto& host : hosts->nodes())
		{
			_pingMonitors.emplace_back();
			host->loadOrStore("Address", _pingMonitors.back().address);
			host->loadOrStore("Logfile", _pingMonitors.back().logfilename);
			host->loadOrStore("Timing.Delay", _pingMonitors.back().delayMs);
			host->loadOrStore("Timing.Timeout", _pingMonitors.back().timeoutMs);
			host->loadOrStore("Timing.Async", _pingMonitors.back().async);
			host->loadOrStore("Graph.DrawMeanGraph", _pingMonitors.back().drawMeanGraph);
			host->loadOrStore("Graph.DrawJitterGraph", _pingMonitors.back().drawJitterGraph);
			host->loadOrStore("Graph.DrawLossGraph", _pingMonitors.back().drawLossGraph);
			host->loadOrStore("Graph.PixelPerSecond", _pingMonitors.back().pixelPerSecond);
		}

		RECT windowRect = { 0, 0, 540, static_cast<LONG>(240 * _pingMonitors.size()) };
		AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);
		_defaultWindowRect = windowRect;

		_backgroundThread = autojoin_thread([this] { backgroundThreadMain(); });
	}

	GraphPrinter(const GraphPrinter&) = delete;
	GraphPrinter(GraphPrinter&&) = delete;
	GraphPrinter& operator = (const GraphPrinter&) = delete;
	GraphPrinter& operator = (GraphPrinter&&) = delete;

	void forceRedraw()
	{
		_forceRedrawFlag = true;
		cancelSleep();
	}

	void cancelSleep()
	{
		_cancelSleepFlag.set();
	}

	FrontBufferLock frontBufferLock()
	{
		return FrontBufferLock(_frontBufferMutex, _frontBuffer->deviceContext());
	}

	RECT defaultWindowRect()
	{
		return _defaultWindowRect;
	}

	void resetBackBufferSize()
	{
		_resetBackBufferSizeFlag = true;
	}

private:
	void backgroundThreadMain()
	{
		using namespace std::chrono;		

		_consolasFont.reset(CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
			OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FF_DONTCARE, L"Consolas"));

		_backgroundBrush.reset(CreateSolidBrush(DARK_BLUE));

		_greenPen.reset(CreatePen(PS_SOLID, _penThickness, GREEN));
		_bluePen.reset(CreatePen(PS_SOLID, _penThickness, BLUE));
		_redPen.reset(CreatePen(PS_SOLID, _penThickness, RED));
		_yellowPen.reset(CreatePen(PS_SOLID, _penThickness, YELLOW));
		_whitePen.reset(CreatePen(PS_SOLID, _penThickness, WHITE));

		for (auto& pingMonitorProperties : _pingMonitors)
		{
			pingMonitorProperties.makeMonitor();
		}

		const auto drawDelay = milliseconds(_drawDelayMs);
		auto nextDraw = steady_clock::now();

		while (!_stopThreadFlag)
		{
			auto nextTick = steady_clock::now() + milliseconds(4000);

			for (auto& pingMonitorProperties : _pingMonitors)
			{
				nextTick = std::min(nextTick, pingMonitorProperties.pingMonitor->tick([this] { cancelSleep(); }));
			}

			if (_forceRedrawFlag || steady_clock::now() >= nextDraw)
			{
				_forceRedrawFlag = _alwaysRedraw;

				if (IsWindowVisible(_windowHandle))
				{
					++_frameCounter;
					drawAndSwapBuffers();
					InvalidateRect(_windowHandle, nullptr, true);
				}

				while (steady_clock::now() >= nextDraw && drawDelay > seconds(0))
				{
					nextDraw += drawDelay;
				}
			}

			if (_cancelSleepFlag.waitUntil(std::min(nextTick, nextDraw)))
			{	// Sleep was interrupted.
				_cancelSleepFlag.reset();
			}
		}
	}

	void drawAndSwapBuffers()
	{
		RECT clientRect;
		GetClientRect(_windowHandle, &clientRect);
		const auto clientWidth = std::uint16_t(clientRect.right - clientRect.left);
		const auto clientHeight = std::uint16_t(clientRect.bottom - clientRect.top);

		if (clientWidth >= 16 && clientHeight >= 16)
		{
			if (_resetBackBufferSizeFlag || 
				clientWidth > _backBuffer->width() || 
				clientHeight > _backBuffer->height() ||
				clientWidth < _backBuffer->width() / 2 || 
				clientHeight < _backBuffer->height() / 2)
			{
				_resetBackBufferSizeFlag = false;
				_backBuffer->reset(clientWidth + 16, clientHeight + 16);
			}

			if (_highResBackBuffer->width() != _backBuffer->width() * ANTI_ALIASING || 
				_highResBackBuffer->height() != _backBuffer->height() * ANTI_ALIASING)
			{
				_highResBackBuffer->reset(_backBuffer->width() * ANTI_ALIASING, _backBuffer->height() * ANTI_ALIASING);
			}

			const std::uint16_t highResWidth = clientWidth * ANTI_ALIASING;
			const std::uint16_t highResHeight = clientHeight * ANTI_ALIASING;
			
			for (std::size_t i = _pingMonitors.size(); i-- > 0; )
			{	// Draw graphs on high resolution buffer
				const auto partialHeight = static_cast<std::uint16_t>(highResHeight / _pingMonitors.size());
				const auto drawOffset = static_cast<std::uint16_t>(i * partialHeight);
				const auto endPtr = _highResBackBuffer->memoryPtr() + _highResBackBuffer->width() * _highResBackBuffer->height();
				const auto startPtr = endPtr - _highResBackBuffer->width() * (drawOffset + partialHeight);
				const auto nElementsToFill = std::size_t(_highResBackBuffer->width() * partialHeight);
				
				// std::fill_n generates rep stos, which is perfect and twice as fast as movaps.
				// Don't change to std::fill, std::fill generates neither rep stos nor movaps/movups.
				std::fill_n(startPtr, nElementsToFill, BACKGROUND_COLOR); // Clear background
				drawGraph(_pingMonitors[i], 0, drawOffset, highResWidth, partialHeight);
			}

			// Clear background that isn't visible yet, so it has the right color immediately in case the window gets resized.
			const std::size_t nElementsToFill = _highResBackBuffer->width() * (_highResBackBuffer->height() - highResHeight);
			std::fill_n(_highResBackBuffer->memoryPtr(), nElementsToFill, BACKGROUND_COLOR);

			// Copy high resolution buffer to back buffer
			scaleBitmap2(_backBuffer->memoryPtr(), _highResBackBuffer->memoryPtr(), _backBuffer->width(), _backBuffer->height());
			
			for (std::size_t i = 0; i < _pingMonitors.size(); ++i)
			{	// Draw info
				const auto partialHeight = static_cast<std::uint16_t>(clientHeight / _pingMonitors.size());
				const auto drawOffset = static_cast<std::uint16_t>(i * partialHeight);

				drawInfo(_pingMonitors[i], 0, drawOffset, clientWidth, partialHeight);
			}

			{	// Flush drawing operations, then swap back and front buffers
				GdiFlush();
				std::lock_guard<std::mutex> frontBufferLock(_frontBufferMutex);
				std::swap(_frontBuffer, _backBuffer);
			}
		}
	}

	void drawGraph(PingMonitorProperties& pingMonitorProperties,
		std::uint16_t pixelOffsetX, std::uint16_t pixelOffsetY,
		std::uint16_t baseWidth, std::uint16_t baseHeight)
	{
		auto& maxGraphPing = pingMonitorProperties.maxGraphPing;
		auto pingSamples = pingMonitorProperties.pingMonitor->pingSamples();

		if (pingSamples.size() > 0)
		{
			const auto& lastSample = pingSamples.back();
			const auto graphHeight = baseHeight - _bottomBorder * ANTI_ALIASING;

			pingMonitorProperties.maxPing = 0.0;
			maxGraphPing = graphHeight <= 0 ? 0.0 : 
				lastSample.roundtripTimeMean * 2 + graphHeight * 0.04 * std::sqrt(0.5 * lastSample.roundtripTimeMean);
			
			const auto calcY = [&](double value, double highestPoint)
			{	// Helper routine to generate graph coordinates
				return std::lround(pixelOffsetY + graphHeight - (value / (highestPoint)) * graphHeight);
			};

			std::vector<POINT> roundtripTimePoints;
			std::vector<POINT> roundtripTimeMeanPoints;
			std::vector<POINT> roundtripTimeJitterPoints;
			std::vector<POINT> lossPercentagePoints;
			std::vector<POINT> lossLines;
			std::vector<DWORD> lossLineSizes;

			for (std::size_t i = 0; i < pingSamples.size(); ++i)
			{
				using namespace std::chrono;
				auto ns = duration_cast<nanoseconds>(lastSample.sentTime - pingSamples[i].sentTime);
				auto h = 0.000000001 * ns.count() * pingMonitorProperties.pixelPerSecond * ANTI_ALIASING;
				auto x = std::lround(pixelOffsetX + baseWidth - h);

				if (x >= 0)
				{
					auto roundtripTimeY = calcY(pingSamples[i].roundtripTime, maxGraphPing);
					auto roundtripTimeMeanY = calcY(pingSamples[i].roundtripTimeMean, maxGraphPing);
					auto roundtripTimeJitterY = calcY(pingSamples[i].roundtripTimeJitter, maxGraphPing);
					auto lossPercentageY = calcY(pingSamples[i].lossPercentage, 100.0);

					roundtripTimeMeanPoints.push_back(POINT{ x, roundtripTimeMeanY });
					roundtripTimeJitterPoints.push_back(POINT{ x, roundtripTimeJitterY });
					lossPercentagePoints.push_back(POINT{ x, lossPercentageY });

					if (pingSamples[i].errorCode == 0)
					{
						pingMonitorProperties.maxPing = std::max(pingSamples[i].roundtripTime, pingMonitorProperties.maxPing);
						roundtripTimePoints.push_back(POINT{ x, roundtripTimeY });
					}
					else
					{
						if (roundtripTimePoints.size() == 0)
						{
							roundtripTimePoints.push_back(POINT{ 0, calcY(maxGraphPing / 2.0, maxGraphPing) });
						}

						lossLines.push_back(roundtripTimePoints.back());
						lossLines.push_back(POINT{ x, lossLines.back().y });
						roundtripTimePoints.push_back(lossLines.back());
						lossLineSizes.push_back(2);
					}
				}
			}

			if (pingMonitorProperties.drawLossGraph)
			{
				_highResBackBuffer->select(_redPen.get());
				Polyline(_highResBackBuffer->deviceContext(), lossPercentagePoints.data(),
					static_cast<int>(lossPercentagePoints.size()));
			}

			if (pingMonitorProperties.drawJitterGraph)
			{
				_highResBackBuffer->select(_yellowPen.get());
				Polyline(_highResBackBuffer->deviceContext(), roundtripTimeJitterPoints.data(),
					static_cast<int>(roundtripTimeJitterPoints.size()));
			}

			if (pingMonitorProperties.drawMeanGraph)
			{
				_highResBackBuffer->select(_bluePen.get());
				Polyline(_highResBackBuffer->deviceContext(), roundtripTimeMeanPoints.data(),
					static_cast<int>(roundtripTimeMeanPoints.size()));
			}

			_highResBackBuffer->select(_greenPen.get());
			Polyline(_highResBackBuffer->deviceContext(), roundtripTimePoints.data(),
				static_cast<int>(roundtripTimePoints.size()));

			_highResBackBuffer->select(_redPen.get());
			PolyPolyline(_highResBackBuffer->deviceContext(), lossLines.data(), lossLineSizes.data(),
				static_cast<DWORD>(lossLineSizes.size()));
		}
	}

	void drawInfo(const PingMonitorProperties& pingMonitorProperties,
		std::uint16_t pixelOffsetX, std::uint16_t pixelOffsetY,
		std::uint16_t drawWidth, std::uint16_t drawHeight)
	{
		const auto& statusString = pingMonitorProperties.pingMonitor->statusString();

		auto sample = PingSample{};

		if (pingMonitorProperties.pingMonitor->pingSamples().size() > 0)
		{
			sample = pingMonitorProperties.pingMonitor->pingSamples().back();
		}

		_backBuffer->select(_consolasFont.get());

		SetBkColor(_backBuffer->deviceContext(), DARK_BLUE);

		const auto drawText = [&](int x, int y, COLORREF color, const std::string& text)
		{
			SetTextColor(_backBuffer->deviceContext(), color);
			TextOutA(_backBuffer->deviceContext(), x, y, text.c_str(), static_cast<int>(text.size()));
		};
		
		// Draw frame counter
		//drawText(0, 0, WHITE, std::to_string(_frameCounter));

		const auto baseWidth = drawWidth + pixelOffsetX;
		const auto baseHeight = drawHeight + pixelOffsetY;

		// Draw status string
		drawText(pixelOffsetX + 8, baseHeight - 24, WHITE, statusString);

		// Draw current ping
		drawText(baseWidth - 120, baseHeight - 56, GREEN,
			formatString("Ping %.2f ms", sample.roundtripTime));

		// Draw mean ping
		drawText(baseWidth - 120, baseHeight - 40, pingMonitorProperties.drawMeanGraph ? BLUE : WHITE,
			formatString("Mean %.2f ms", sample.roundtripTimeMean));

		// If status string isn't too long
		if (static_cast<std::int64_t>(statusString.size() * 8 + 8) < baseWidth - 120)
		{	// Draw max ping
			drawText(baseWidth - 120, baseHeight - 24, WHITE,
				formatString("Max  %.2f ms", pingMonitorProperties.maxPing));
		}

		// Draw jitter
		drawText(baseWidth - 256, baseHeight - 56, YELLOW,
			formatString("Jitter %.2f ms", sample.roundtripTimeJitter));

		// Draw loss percentage
		drawText(baseWidth - 256, baseHeight - 40, RED,
			formatString("Loss   %.2f %%", sample.lossPercentage));

		// If status string isn't too long
		if (static_cast<std::int64_t>(statusString.size() * 8 + 8) < baseWidth - 256)
		{	// Draw maximum visible graph height
			drawText(baseWidth - 256, baseHeight - 24, WHITE,
				formatString("Graph  %.0f ms", pingMonitorProperties.maxGraphPing));
		}
	}

	static void scaleBitmap2(std::uint32_t* target, std::uint32_t* source, std::size_t width, std::size_t height)
	{
		const std::size_t scaledWidth = width << 1;
		const std::size_t scaledHeight = height << 1;
		const std::size_t scaledSize = scaledHeight * scaledWidth;
		const std::size_t scaledStep = scaledWidth << 1;

		for (std::size_t iTimesWidth = 0; iTimesWidth < scaledSize; iTimesWidth += scaledStep)
		{
			for (std::size_t j = 0; j < scaledWidth; j += 2)
			{
				std::uint_fast32_t p0 = source[iTimesWidth + 0 + j + 0];
				std::uint_fast32_t p1 = source[iTimesWidth + 0 + j + 1];
				std::uint_fast32_t p2 = source[iTimesWidth + 1 + j + 0];
				std::uint_fast32_t p3 = source[iTimesWidth + 1 + j + 1];

				std::uint_fast16_t p0_r = (p0 >> 16) & 0xFF;
				std::uint_fast16_t p0_g = (p0 >> 8) & 0xFF;
				std::uint_fast16_t p0_b = (p0 >> 0) & 0xFF;

				std::uint_fast16_t p1_r = (p1 >> 16) & 0xFF;
				std::uint_fast16_t p1_g = (p1 >> 8) & 0xFF;
				std::uint_fast16_t p1_b = (p1 >> 0) & 0xFF;

				std::uint_fast16_t p2_r = (p2 >> 16) & 0xFF;
				std::uint_fast16_t p2_g = (p2 >> 8) & 0xFF;
				std::uint_fast16_t p2_b = (p2 >> 0) & 0xFF;

				std::uint_fast16_t p3_r = (p3 >> 16) & 0xFF;
				std::uint_fast16_t p3_g = (p3 >> 8) & 0xFF;
				std::uint_fast16_t p3_b = (p3 >> 0) & 0xFF;

				std::uint_fast32_t r = (p0_r + p1_r + p2_r + p3_r) >> 2;
				std::uint_fast32_t g = (p0_g + p1_g + p2_g + p3_g) >> 2;
				std::uint_fast32_t b = (p0_b + p1_b + p2_b + p3_b) >> 2;

				target[(iTimesWidth >> 2) + (j >> 1)] = (r << 16) | (g << 8) | (b << 0);
			}
		}
	}
};

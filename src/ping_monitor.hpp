#pragma once

#include "utility/utility.hpp"
#include "winapi/utility.hpp"
#include "icmp.hpp"
#include "window_messages.hpp"

#include <functional>
#include <random>
#include <string>

namespace pingstats // export
{
	using namespace utility::literals;

	namespace cr = std::chrono;
	namespace ut = utility;
	namespace wa = winapi;

	class PingMonitor
	{
		std::string _targetname = "trace public4 8.8.8.8";
		std::string _sourcename = "auto";

		IpEndPoint _target;
		IpEndPoint _source;

		std::uint32_t _pingIntervalMs = 500;
		std::uint32_t _pingTimeoutMs = 2000;

		HWND _resultHandler;
		WPARAM _resultTag;

		HANDLE _stopEvent;
		ut::AutojoinThread _thread;

	public:
		~PingMonitor()
		{
			SetEvent(_stopEvent);
		}

		PingMonitor(ut::TreeConfigNode& config, HWND resultHandler, WPARAM resultTag)
			: _resultHandler(resultHandler)
			, _resultTag(resultTag)
			, _stopEvent(CreateEventW(nullptr, true, false, nullptr))
		{
			config.loadOrStore("target", _targetname);
			config.loadOrStore("source", _sourcename);
			config.loadOrStore("pingIntervalMs", _pingIntervalMs);
			config.loadOrStore("pingTimeoutMs", _pingTimeoutMs);

			_thread = std::thread([this] { run(); });
		}

	private:
		void run()
		{
			//std::random_device rng;
			//std::uniform_int_distribution<> dist(50, 500);
			//std::this_thread::sleep_for(milliseconds(dist(rng)));

			if (!setSourceAndTarget())
			{
				return;
			}

			constexpr DWORD MAX_OBJECTS = std::min(64, MAXIMUM_WAIT_OBJECTS);

			std::array<std::unique_ptr<IcmpEchoContext>, MAX_OBJECTS> contexts;
			std::array<HANDLE, MAX_OBJECTS> events = { _stopEvent };

			const auto pingInterval = cr::nanoseconds{ cr::milliseconds{_pingIntervalMs} };

			auto nextPingTime = cr::steady_clock::now();

			for (;;)
			{
				DWORD nEvents = 1;

				for (; contexts[nEvents] != nullptr && nEvents < MAX_OBJECTS; ++nEvents)
				{
					events[nEvents] = contexts[nEvents]->event.get();
				}

				const auto now = cr::steady_clock::now();

				if (nextPingTime <= now + 1ms)
				{
					if (nEvents < MAX_OBJECTS)
					{
						const auto i = nEvents++;

						contexts[i] = asyncSendIcmpEcho(
							_target, _source, _pingTimeoutMs, 255);

						events[i] = contexts[i]->event.get();

						if (contexts[i]->errorCode == ERROR_IO_PENDING)
						{
							contexts[i]->errorCode = 0;
						}
						else
						{
							--nEvents;

							const auto replyTime{ cr::steady_clock::now() };
							sendResult(makeIcmpPingResult(*contexts[i], replyTime));

							contexts[i] = nullptr;
						}
					}

					do {
						nextPingTime += pingInterval;
					} while (nextPingTime < now);
				}

				// assert(nextPingTime >= now && "Impossible!");
				const auto waitTimeMs = ut::fastround<DWORD>(
					ut::milliseconds_f64{ nextPingTime - now }.count());

				const auto reason = WaitForMultipleObjects(
					nEvents, &events[0], false, waitTimeMs);

				const auto replyTime = cr::steady_clock::now();

				if (reason >= WAIT_OBJECT_0 && 
					reason < WAIT_OBJECT_0 + MAX_OBJECTS)
				{
					const auto index = reason - WAIT_OBJECT_0;

					// Waiting suspended by external stop event.
					if (index == 0)
					{
						return;
					}

					sendResult(makeIcmpPingResult(*contexts[index], replyTime));

					contexts[index] = nullptr;
				}
			}
		}

		bool setSourceAndTarget()
		{
			_source = _sourcename == "auto" ? IpEndPoint() :
				IpEndPoint::fromHostname(_sourcename.c_str());

			const auto words = ut::parseWords(_targetname);

			if (words.size() == 3 && words[0] == "trace")
			{
				const auto traceTarget{ IpEndPoint::fromHostname(words[2].c_str()) };
				const auto traceType{
					words[1] == "public4" ?
					TraceType::FIRST_PUBLIC :
					words[1] == "private4" ?
					TraceType::LAST_PRIVATE :
					TraceType::FULL_TRACE
				};

				if (!traceRoute(
					_target,
					traceType,
					_stopEvent,
					traceTarget,
					_source,
					_pingTimeoutMs,
					_resultHandler,
					_resultTag))
				{
					return false;
				}
			}
			else
			{
				_target = IpEndPoint::fromHostname(_targetname.c_str());
			}

			return true;
		}

		void sendResult(const IcmpEchoResult& result)
		{
			SendMessageW(_resultHandler, WM_PING_RESULT, 
				_resultTag, reinterpret_cast<LPARAM>(&result));
		}
	};
}

#pragma once

#include "utility/utility.hpp"
#include "utility/stopwatch.hpp"

#include <vector>
#include <future>

#include <Iphlpapi.h>
#include <Icmpapi.h>

class PingSample
{
public:
	std::int64_t errorCode;
	std::chrono::steady_clock::time_point sentTime;
	double roundtripTime;
	double roundtripTimeMean;
	double roundtripTimeJitter;
	double lossPercentage;
};

class PingMonitor
{
	struct IcmpHandleCloser
	{
		void operator () (HANDLE icmpfile)
		{
			IcmpCloseHandle(icmpfile);
		}
	};

	typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, IcmpHandleCloser> IcmpFileHandle;

	class IpEndPoint
	{
		IPAddr _ipv4Addr;

	public:
		explicit IpEndPoint(IPAddr ipv4Addr = 0)
			: _ipv4Addr(ipv4Addr)
		{}

		bool operator == (const IpEndPoint& rhs) const
		{
			return _ipv4Addr == rhs._ipv4Addr;
		}

		IPAddr addr4() const
		{
			return _ipv4Addr;
		}

		bool isPublicAddress()
		{
			if ((_ipv4Addr & htonl(0xFF000000)) == htonl(0x0A000000))
				return false;

			if ((_ipv4Addr & htonl(0xFFF00000)) == htonl(0xAC100000))
				return false;

			if ((_ipv4Addr & htonl(0xFFFF0000)) == htonl(0xC0A80000))
				return false;

			return true;
		}

		std::string name() const
		{
			return inet_ntoa(*reinterpret_cast<const in_addr*>(&_ipv4Addr));
		}
	};

	class IcmpPingResult
	{
	public:
		std::int64_t errorCode;
		std::chrono::steady_clock::time_point sentTime;
		std::chrono::nanoseconds roundtripTime;
		IpEndPoint responder;
		std::string errorString;
	};

	typedef std::chrono::steady_clock::time_point (PingMonitor::* TickFunction) (std::function<void()>);

	IpEndPoint _traceTargetServer = IpEndPoint(inet_addr("8.8.8.8"));
	IpEndPoint _host = IpEndPoint(0);
	std::uint8_t _hops = 0;

	std::chrono::milliseconds _delay;
	std::chrono::milliseconds _timeout;
	bool _async;

	std::chrono::steady_clock::time_point _lastPingReceived = std::chrono::steady_clock::now() - _delay;
	std::chrono::steady_clock::time_point _lastPingSent = std::chrono::steady_clock::now() - _delay;

	TickFunction _tick;

	std::vector<std::future<IcmpPingResult>> _pendingRequests;

	std::vector<PingSample> _samples;

	std::string _statusString = "Initializing...";

	FileHandle _log;

public:
	PingMonitor(const std::string& hostname, std::uint32_t delayMs, 
		std::uint32_t timeoutMs, bool async, FileHandle&& logfile)
		: _delay(delayMs)
		, _timeout(timeoutMs)
		, _async(async)
		, _log(std::move(logfile))
	{
		log(" [===] Initiating ping to \"%s\" with delay = %u, timeout = %u, async = %u.",
			hostname.c_str(), delayMs, timeoutMs, async);

		if (hostname == "auto4")
		{
			_tick = &PingMonitor::findFirstPublicNode;
		}
		else if (hostname == "auto6")
		{	// Not supported yet
			_tick = &PingMonitor::findFirstPublicNode;
		}
		else
		{
			_host = IpEndPoint(inet_addr(hostname.c_str()));
			_tick = &PingMonitor::findNumberOfHops;
		}
	}

	const std::string& statusString() const
	{
		return _statusString;
	}

	const std::vector<PingSample>& pingSamples() const
	{
		return _samples;
	}

	// Returns when tick() should be called the next time.
	std::chrono::steady_clock::time_point tick(std::function<void()> notify)
	{
		return (this->*_tick)(notify);
	}

private:
	std::chrono::steady_clock::time_point findFirstPublicNode(std::function<void()> notify)
	{
		using namespace std::chrono;

		if (_pendingRequests.size() > 0 && _pendingRequests.back().valid())
		{
			auto pingResult = _pendingRequests.back().get();
			_pendingRequests.pop_back();

			if (pingResult.responder.isPublicAddress())
			{
				_host = pingResult.responder;
				_tick = &PingMonitor::ping;
				log(" [==+] Found first public node = %s with hops = %u.", _host.name().c_str(), _hops);
				return tick(notify);
			}
			else
			{
				log(" [=++] Searching for first public node, answer from = %s with hops = %u.", 
					pingResult.responder.name().c_str(), _hops);
			}

			++_hops;
		}

		if (_pendingRequests.size() == 0)
		{
			_pendingRequests.push_back(asyncIcmpPing(_traceTargetServer, 
				static_cast<DWORD>(_timeout.count()), _hops, notify));
		}

		return steady_clock::now() + milliseconds(1000);
	}

	std::chrono::steady_clock::time_point findNumberOfHops(std::function<void()> notify)
	{
		using namespace std::chrono;

		if (_pendingRequests.size() > 0 && _pendingRequests.back().valid())
		{
			auto pingResult = _pendingRequests.back().get();
			_pendingRequests.pop_back();

			if (pingResult.responder == _host)
			{
				_host = pingResult.responder;
				_tick = &PingMonitor::ping;
				log(" [==+] Found number of hops = %u to node = %s.", _hops, _host.name().c_str());
				return tick(notify);
			}
			else
			{
				log(" [=++] Searching for number of hops, answer from = %s with hops = %u.", 
					pingResult.responder.name().c_str(), _hops);
			}

			++_hops;
		}

		if (_pendingRequests.size() == 0)
		{
			_pendingRequests.push_back(asyncIcmpPing(_traceTargetServer, 
				static_cast<DWORD>(_timeout.count()), _hops, notify));
		}

		return steady_clock::now() + milliseconds(1000);
	}

	std::chrono::steady_clock::time_point ping(std::function<void()> notify)
	{
		using namespace std::chrono;

		while (_pendingRequests.size() > 0 && _pendingRequests.back().valid())
		{
			auto pingResult = _pendingRequests.back().get();
			_pendingRequests.pop_back();
			_lastPingReceived = steady_clock::now();

			processPingResult(pingResult);
		}

		if (_async)
		{
			if (steady_clock::now() >= _lastPingSent + _delay)
			{
				_pendingRequests.push_back(asyncIcmpPing(_host, static_cast<DWORD>(_timeout.count()), _hops, notify));
				_lastPingSent = steady_clock::now();
			}

			return _lastPingSent + _delay;
		}
		else
		{
			if (_pendingRequests.size() == 0 && steady_clock::now() >= _lastPingReceived + _delay)
			{
				_pendingRequests.push_back(asyncIcmpPing(_host, static_cast<DWORD>(_timeout.count()), _hops, notify));
				_lastPingSent = steady_clock::now();
			}

			return _pendingRequests.size() == 0 ? _lastPingReceived + _delay : steady_clock::now() + milliseconds(1000);
		}		
	}

	void processPingResult(IcmpPingResult pingResult)
	{
		_statusString = pingResult.errorCode != 0 ? 
			std::move(pingResult.errorString) : 
			formatString("Pinging %s over %u %s", pingResult.responder.name().c_str(), _hops, _hops == 1 ? "hop" : "hops");

		_samples.push_back(PingSample{pingResult.errorCode, pingResult.sentTime});

		// Nanoseconds (int64) to milliseconds (double)
		_samples.back().roundtripTime = pingResult.roundtripTime.count() * 0.000001;

		{ // Calculate average round trip time
			std::uint32_t nSuccessfulPings = 0;
			double sum = 0.0;

			forReverseRange(_samples.size(), 240, [&] (std::size_t i)
			{
				if (_samples[i].errorCode == 0)
				{
					++nSuccessfulPings;
					sum += _samples[i].roundtripTime;
				}
			});

			_samples.back().roundtripTimeMean = nSuccessfulPings > 0 ? sum / nSuccessfulPings : -1.0;
		}

		{ // Calculate jitter
			std::uint32_t nSuccessfulPings = 0;
			double localMean = 0.0;

			forReverseRange(_samples.size(), 30, [&](std::size_t i)
			{
				if (_samples[i].errorCode == 0)
				{
					++nSuccessfulPings;
					localMean += _samples[i].roundtripTime;
				}
			});

			if (nSuccessfulPings > 0)
			{
				localMean /= nSuccessfulPings;

				double squaredDistances = 0.0;

				forReverseRange(_samples.size(), 30, [&](std::size_t i)
				{
					if (_samples[i].errorCode == 0)
					{
						double distance = _samples[i].roundtripTime - localMean;
						squaredDistances += distance * distance;
					}
				});

				_samples.back().roundtripTimeJitter = std::sqrt(squaredDistances / nSuccessfulPings);
			}
		}

		{ // Calculate loss
			std::size_t nSamples = 0;
			std::size_t nLostPackets = 0;

			forReverseRange(_samples.size(), 30, [&](std::size_t i)
			{
				++nSamples;
				nLostPackets += _samples[i].errorCode != 0;
			});

			// Check if 0
			_samples.back().lossPercentage = 100.0 * nLostPackets / nSamples;
		}

		if (_samples.size() >= 4096)
		{
			_samples = decltype(_samples)(_samples.begin() + 2048, _samples.end());
		}

		log(" [=++] Latency = %f ms, Mean = %f ms, Jitter = %f ms, Loss = %f%%. (%s)", 
			_samples.back().roundtripTime, _samples.back().roundtripTimeMean, 
			_samples.back().roundtripTimeJitter, _samples.back().lossPercentage, 
			pingResult.errorCode == 0 ? "+" : _statusString.c_str());
	}

	template <std::size_t N, typename... Args>
	void log(const char(&format)[N], Args&&... args)
	{
		if (_log != nullptr)
		{
			printTimestamp(_log.get());
			fprintf(_log.get(), format, args...);
			fprintf(_log.get(), "\n");
			fflush(_log.get());
		}
	}

	static std::string ipErrorString(const char* functionName, DWORD errorCode = GetLastError())
	{
		wchar_t buffer[0x1000];
		DWORD size = sizeof buffer * sizeof *buffer;

		GetIpErrorString(errorCode, buffer, &size);

		return std::string(functionName) + " failed (" + std::to_string(errorCode) + "): " + toUtf8(buffer);
	}

	static std::future<IcmpPingResult> asyncIcmpPing(IpEndPoint host, DWORD timeout, UCHAR ttl, std::function<void()> notify)
	{
		return std::async(std::launch::async, [=]() -> IcmpPingResult {
			using namespace std::chrono;

			auto host4 = host.addr4();
			IcmpPingResult result = {};

			IcmpFileHandle icmpFile(IcmpCreateFile());

			ICMP_ECHO_REPLY reply = {};
			IP_OPTION_INFORMATION options = {};
			options.Ttl = ttl;

			result.sentTime = steady_clock::now();
			auto ret = IcmpSendEcho(icmpFile.get(), host4, nullptr, 0, &options, &reply, sizeof reply, timeout);

			// Subtracting 0.2 ms because of call overhead, while not allowing values below 0.01 ms.
			result.roundtripTime = std::max(nanoseconds(1000), steady_clock::now() - result.sentTime - microseconds(200));
			result.responder = IpEndPoint(reply.Address);

			if (ret == 0)
			{ // Error
				result.errorCode = GetLastError();
				result.errorString = ipErrorString("IcmpSendEcho");
			}

			notify();

			return result;
		});
	}

	template <typename F>
	static void forReverseRange(std::size_t exclusiveStart, std::size_t nElements, F&& body)
	{
		const auto inclusiveEnd = std::max(exclusiveStart, nElements) - nElements;
		for (std::size_t i = exclusiveStart; i-- > inclusiveEnd; )
		{
			body(i);
		}
	}
};

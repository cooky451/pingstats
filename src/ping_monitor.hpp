#pragma once

#include "utility/utility.hpp"
#include "utility/stopwatch.hpp"

#include <algorithm>
#include <future>
#include <vector>

#include <Iphlpapi.h>
#include <Icmpapi.h>

class PingSample
{
public:
	std::uint32_t errorCode;
	std::chrono::steady_clock::time_point sentTime;
	double roundtripTime;
	double roundtripTimeMean;
	double roundtripTimeJitter;
	double lossPercentage;
};

bool operator < (const PingSample& lhs, const PingSample& rhs)
{
	return lhs.sentTime < rhs.sentTime;
}

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
		std::uint32_t errorCode;
		std::uint32_t statusCode;
		std::chrono::steady_clock::time_point sentTime;
		std::chrono::nanoseconds roundtripTime;
		IpEndPoint responder;
		std::string statusString;
	};

	typedef std::chrono::steady_clock::time_point (PingMonitor::* TickFunction) (std::function<void()>);

	IpEndPoint _traceTargetServer = IpEndPoint(inet_addr("8.8.8.8"));
	IpEndPoint _host = IpEndPoint(0);
	std::uint8_t _hops = 0;
	std::uint8_t _tries = 0;

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

		if (_pendingRequests.size() > 0 &&
			_pendingRequests.back().wait_for(seconds(0)) == std::future_status::ready)
		{
			auto pingResult = _pendingRequests.back().get();
			_pendingRequests.pop_back();

			if (pingResult.errorCode == 0)
			{
				_tries = 0;

				if (pingResult.statusCode == 0)
				{
					_statusString = formatString("Found target node %s with hops = %u.",
						pingResult.responder.name().c_str(), _hops);

					log(" [==+] %s", _statusString.c_str());

					_host = pingResult.responder;
					_tick = &PingMonitor::ping;
					
					return tick(notify);
				}
				else if (pingResult.responder.isPublicAddress()
					&& pingResult.statusCode == IP_TTL_EXPIRED_TRANSIT)
				{
					_statusString = formatString("Found candidate %s with hops = %u.", 
						pingResult.responder.name().c_str(), _hops);

					log(" [=++] %s", _statusString.c_str());

					_pendingRequests.push_back(asyncIcmpPing(pingResult.responder,
						static_cast<DWORD>(_timeout.count()), _hops, notify));
				}
				else
				{
					_statusString = formatString("Node %s with hops = %u not suitable. (%s)",
						pingResult.responder.name().c_str(), _hops, pingResult.responder.isPublicAddress() ? 
						pingResult.statusString.c_str() : "Not public.");

					log(" [=++] %s", _statusString.c_str());

					++_hops;
				}
			}
			else
			{
				_statusString = formatString("%s with hops = %u.", pingResult.statusString.c_str(), _hops);
				log(" [=++] %s", _statusString.c_str());

				if (++_tries == 4)
				{
					_tries = 0;
					++_hops;
				}
			}
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

		if (_pendingRequests.size() > 0 && 
			_pendingRequests.back().wait_for(seconds(0)) == std::future_status::ready)
		{
			auto pingResult = _pendingRequests.back().get();
			_pendingRequests.pop_back();

			if (pingResult.errorCode == 0)
			{
				_tries = 0;

				if (pingResult.responder == _host && pingResult.statusCode == 0)
				{
					_statusString = formatString("Response from target node with hops = %u.", _hops);
					log(" [==+] %s", _statusString.c_str());

					_host = pingResult.responder;
					_tick = &PingMonitor::ping;

					return tick(notify);
				}
				else
				{
					_statusString = formatString("Response from %s with hops = %u: %s",
						pingResult.responder.name().c_str(), _hops, pingResult.statusString.c_str());

					log(" [=++] %s", _statusString.c_str());

					_hops += pingResult.statusCode == IP_TTL_EXPIRED_TRANSIT;
				}
			}
			else
			{
				_statusString = pingResult.statusString;
				log(" [=++] %s", _statusString.c_str());

				if (++_tries == 4)
				{
					_tries = 0;
					++_hops;
				}
			}
		}

		if (_pendingRequests.size() == 0)
		{
			_pendingRequests.push_back(asyncIcmpPing(_host,
				static_cast<DWORD>(_timeout.count()), _hops, notify));
		}

		return steady_clock::now() + milliseconds(500);
	}

	std::chrono::steady_clock::time_point ping(std::function<void()> notify)
	{
		using namespace std::chrono;

		while (_pendingRequests.size() > 0 
			&& _pendingRequests.back().wait_for(seconds(0)) == std::future_status::ready)
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
		_statusString = pingResult.errorCode != 0 || pingResult.statusCode != 0 ? std::move(pingResult.statusString) :
			formatString("Pinging %s over %u %s.", pingResult.responder.name().c_str(), _hops, _hops == 1 ? "hop" : "hops");

		auto& sample = *pushSampleSorted(PingSample{ pingResult.errorCode, pingResult.sentTime });

		// Nanoseconds (int64) to milliseconds (double)
		sample.roundtripTime = pingResult.roundtripTime.count() * 0.000001;

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

			sample.roundtripTimeMean = nSuccessfulPings > 0 ? sum / nSuccessfulPings : -1.0;
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

				sample.roundtripTimeJitter = std::sqrt(squaredDistances / nSuccessfulPings);
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
			sample.lossPercentage = 100.0 * nLostPackets / nSamples;
		}

		if (_samples.size() >= 4096)
		{
			_samples = decltype(_samples)(_samples.begin() + 2048, _samples.end());
		}

		log(" [=++] Latency = %f ms, Mean = %f ms, Jitter = %f ms, Loss = %f%%. %s", 
			_samples.back().roundtripTime, _samples.back().roundtripTimeMean, 
			_samples.back().roundtripTimeJitter, _samples.back().lossPercentage, 
			pingResult.errorCode == 0 && pingResult.statusCode == 0 ? "" : pingResult.statusString.c_str());
	}

	decltype(_samples)::iterator pushSampleSorted(PingSample sample)
	{
		auto insertionPoint = std::upper_bound(_samples.begin(), _samples.end(), sample, 
			[](const PingSample& lhs, const PingSample& rhs)
		{
			return lhs.sentTime < rhs.sentTime;
		});

		return _samples.insert(insertionPoint, std::move(sample));
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

	static std::string ipStatusString(DWORD errorCode)
	{
		wchar_t buffer[0x1000];
		DWORD size = sizeof buffer * sizeof *buffer;

		GetIpErrorString(errorCode, buffer, &size);

		return toUtf8(buffer) + '(' + std::to_string(errorCode) + ')';
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
			IcmpSendEcho(icmpFile.get(), host4, nullptr, 0, &options, &reply, sizeof reply, timeout);
			auto receiveTime = steady_clock::now();
			result.errorCode = GetLastError();
			result.statusCode = reply.Status;
			result.statusString = ipStatusString(result.errorCode == 0 ? reply.Status : result.errorCode);
			result.responder = IpEndPoint(reply.Address);
			result.roundtripTime = std::max(nanoseconds(1000), receiveTime - result.sentTime - microseconds(200));
			// Subtracting 0.2 ms because of call overhead, while not allowing values below 0.01 ms.

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

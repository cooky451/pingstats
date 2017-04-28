#pragma once

#include "utility/utility.hpp"
#include "ping_monitor.hpp"

#include <string>
#include <vector>

namespace pingstats // export
{
	using namespace utility::literals;

	namespace cr = std::chrono;
	namespace ut = utility;

	class PingData
	{
		std::vector<IcmpEchoResult> _traceResults;
		std::vector<IcmpEchoResult> _pingResults;
		const IcmpEchoResult* _lastResult = {};

		std::size_t _historySize = { 2 * 3600 };

		std::string _lastResponder;

		double _meanWeight = { 80.0 };
		double _jitterWeight = { 40.0 };
		double _lossWeight = { 40.0 };

		double _lastPing = {};
		double _meanPing = {};
		double _maxPing = {};
		double _squaredJitter = {};
		double _jitter = {};
		double _loss = {};
		double _lossPercentage = {};
		double _pixelPerMs = { 1.0 };
		double _pingOffsetMs = {};
		double _gridSizeY = { 50.0 };

	public:
		PingData(ut::TreeConfigNode& config)
		{
			auto& statscfg = *config.findOrAppendNode("stats");

			statscfg.loadOrStore("historySize", _historySize);
			statscfg.loadOrStore("averagePingWeight", _meanWeight);
			statscfg.loadOrStore("averageJitterWeight", _jitterWeight);
			statscfg.loadOrStore("averageLossWeight", _lossWeight);
		}

		auto lastResult() const
		{
			return _lastResult;
		}

		auto& traceResults() const
		{
			return _traceResults;
		}

		auto& pingResults() const
		{
			return _pingResults;
		}

		auto& lastResponder() const
		{
			return _lastResponder;
		}

		auto lastPing() const
		{
			return _lastPing;
		}

		auto meanPing() const
		{
			return _meanPing;
		}

		auto jitter() const
		{
			return _jitter;
		}

		auto lossPercentage() const
		{
			return _lossPercentage;
		}

		auto gridSizeY() const
		{
			return _gridSizeY;
		}

		auto pixelPerMs() const
		{
			return _pixelPerMs;
		}

		auto pingOffsetMs() const
		{
			return _pingOffsetMs;
		}

		void insertPingResult(const IcmpEchoResult& echoResult)
		{
			_lastResponder = echoResult.responder.name();

			// Usually near the end.
			auto insertionPoint = std::upper_bound(
				_pingResults.begin(), 
				_pingResults.end(), 
				echoResult, 
				[](const auto& lhs, const auto& rhs) {
					return lhs.sentTime < rhs.sentTime;
				}
			);

			const auto& result = 
				*_pingResults.insert(insertionPoint, echoResult);

			const auto isLost = 
				result.errorCode != 0 || result.statusCode != 0;

			const auto lw = 
				std::max(1.0 / _lossWeight, 1.0 / _pingResults.size());

			_loss = _loss * (1.0 - lw) + isLost * lw;
			_lossPercentage = 100.0 * _loss;

			if (!isLost)
			{
				calculateStats(result);
			}

			if (_pingResults.size() >= _historySize * 2)
			{
				std::copy(_pingResults.end() - _historySize,
					_pingResults.end(), _pingResults.begin());

				_pingResults.resize(_historySize);
			}

			_lastResult = &_pingResults.back();
		}

		void insertTraceResult(const IcmpEchoResult& traceResult)
		{
			_lastResponder = traceResult.responder.name();

			_traceResults.push_back(std::move(traceResult));

			if (_traceResults.size() >= _historySize * 2)
			{
				std::copy(_traceResults.end() - _historySize,
					_traceResults.end(), _traceResults.begin());

				_traceResults.resize(_historySize);
			}

			_lastResult = &_traceResults.back();
		}

	private:
		void calculateStats(const IcmpEchoResult& result)
		{
			_lastPing = ut::milliseconds_f64(result.latency).count();
			_maxPing = std::max(_maxPing, _lastPing);

			const auto mw = 
				std::max(1.0 / _meanWeight, 1.0 / _pingResults.size());

			_meanPing = (1.0 - mw) * _meanPing + mw * _lastPing;

			const auto jw = 
				std::max(1.0 / _jitterWeight, 1.0 / _pingResults.size());

			const auto sd = 
				(_meanPing - _lastPing) * (_meanPing - _lastPing);

			_squaredJitter = (1.0 - jw) * _squaredJitter + jw * sd;
			_jitter = std::sqrt(_squaredJitter);

			constexpr auto HEIGHT = 200.0;

			const auto optimalPixelPerMs = 
				HEIGHT / ((_meanPing) * 2.0 + _jitter);

			const auto pixelPerMsDiff =
				std::abs(optimalPixelPerMs - _pixelPerMs);

			if (pixelPerMsDiff / optimalPixelPerMs > 0.2)
			{
				if (optimalPixelPerMs >= HEIGHT / 5.0)
				{
					_pixelPerMs = HEIGHT / 5.0;
					_pingOffsetMs = 0.0;
					_gridSizeY = 1.0;
				}
				else
				{
					_pixelPerMs = std::min(HEIGHT / 20.0, optimalPixelPerMs);
					_pingOffsetMs = 0.0;

					const auto assumedHeight = HEIGHT / _pixelPerMs;

					_gridSizeY =
						assumedHeight >= 300.0 ?
						100.0 : assumedHeight >= 150.0 ?
						50.0 : assumedHeight >= 75.0 ?
						25.0 : assumedHeight >= 30.0 ?
						10.0 : 5.0;
				}
			}
		}
	};

	std::time_t makeTimeStamp(cr::steady_clock::time_point tp)
	{
		const auto dur = cr::duration_cast<
			cr::system_clock::duration>(tp - cr::steady_clock::now());

		return cr::system_clock::to_time_t(cr::system_clock::now() + dur);
	}

	std::string makeTimestampString(cr::steady_clock::time_point tp)
	{
		auto stamp = makeTimeStamp(tp);

		std::tm tm;
		localtime_s(&tm, &stamp);

		return ut::formatString("%02d-%02d-%02d %02d:%02d:%02d",
			1900 + tm.tm_year, 1 + tm.tm_mon, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

	std::string makeLogString(const std::vector<IcmpEchoResult>& results)
	{
		std::string str;

		for (auto& result : results)
		{
			str += ut::formatString(
				"[%s] Error %5u | Status %5u | Responder %15s"
				" | Latency %7.2f ms | SysLatency %4d ms\r\n",
				makeTimestampString(result.sentTime).c_str(),
				result.errorCode, result.statusCode,
				result.responder.name().c_str(),
				ut::milliseconds_f64(result.latency).count(), 
				result.sysLatency);
		}

		return str;
	}
}

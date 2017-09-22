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

#pragma once

#include "utility/utility.hpp"
#include "winapi/utility.hpp"

#include "window_messages.hpp"

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <Ws2tcpip.h>
#include <Iphlpapi.h>
#include <Icmpapi.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")

namespace pingstats // export
{
	using namespace utility::literals;

	namespace cr = std::chrono;
	namespace ut = utility;
	namespace wa = winapi;

#if defined _WIN64
	using IcmpEchoReplyType = ICMP_ECHO_REPLY32;
	using IpOptionInformationType = IP_OPTION_INFORMATION32;
#else
	using IcmpEchoReplyType = ICMP_ECHO_REPLY;
	using IpOptionInformationType = IP_OPTION_INFORMATION;
#endif

	struct IcmpCloseHandleType
	{
		void operator () (HANDLE icmpfile)
		{
			IcmpCloseHandle(icmpfile);
		}
	};

	using IcmpFileHandle = std::unique_ptr<
		std::remove_pointer<HANDLE>::type, IcmpCloseHandleType>;

	class IpEndPoint
	{
		IPAddr _ipv4Addr;

	public:
		constexpr IpEndPoint()
			: IpEndPoint{ INADDR_ANY }
		{}

		constexpr explicit IpEndPoint(IPAddr ipv4Addr)
			: _ipv4Addr{ ipv4Addr }
		{}

		constexpr bool operator == (const IpEndPoint& rhs) const
		{
			return _ipv4Addr == rhs._ipv4Addr;
		}

		constexpr bool operator != (const IpEndPoint& rhs) const
		{
			return _ipv4Addr != rhs._ipv4Addr;
		}

		constexpr auto addr4() const
		{
			return _ipv4Addr;
		}

		bool isPublicAddress() const
		{
			return 
				!((_ipv4Addr & htonl(0xFF000000)) == htonl(0x0A000000)) && 
				!((_ipv4Addr & htonl(0xFFF00000)) == htonl(0xAC100000)) && 
				!((_ipv4Addr & htonl(0xFFFF0000)) == htonl(0xC0A80000));
		}

		std::string name() const
		{
			char buffer[512];

			if (inet_ntop(AF_INET, &_ipv4Addr, buffer, sizeof buffer) == nullptr)
			{
				throw std::runtime_error("Generating name for IP failed.");
			}

			return buffer;
		}

		static IpEndPoint fromHostname(const char* targetname)
		{
			struct AddrInfo
			{
				addrinfo* addr{};

				~AddrInfo()
				{
					freeaddrinfo(addr);
				}
			};

			AddrInfo info;
			
			int ec{ getaddrinfo(targetname, nullptr, nullptr, &info.addr) };

			if (!ec)
			{
				for (auto addr{ info.addr }; addr != nullptr; addr = addr->ai_next)
				{
					if (addr->ai_family == AF_INET)
					{
						auto addr_in{ reinterpret_cast<sockaddr_in*>(addr->ai_addr) };
						return IpEndPoint{ addr_in->sin_addr.s_addr };
					}
				}
			}

			throw std::runtime_error(
				"Unable to resolve hostname \""s + targetname + 
				"\".\r\nCode: " + std::to_string(ec));
		}
	};

	class IcmpEchoResult
	{
	public:
		cr::steady_clock::time_point sentTime;
		cr::nanoseconds latency;
		std::uint32_t errorCode;
		std::uint32_t statusCode;
		IpEndPoint responder;
		std::uint32_t sysLatency;
	};

	class IcmpEchoContext
	{
	public:
		// buffer and event have to be defined before file, 
		// so the file gets released before they get.

		alignas(8) std::array<char, 96> buffer{};
		wa::HandlePtr event{ wa::HandlePtr{ CreateEventW(nullptr, false, false, nullptr) } };

		IcmpFileHandle file{ IcmpCreateFile() };
		cr::steady_clock::time_point sentTime{};
		DWORD timoutMs{};
		DWORD errorCode{};
	};

	enum class TraceType
	{
		FULL_TRACE,
		FIRST_PUBLIC,
		LAST_PRIVATE,
	};

	std::string makeIpStatusString(DWORD errorCode)
	{
		wchar_t buffer[0x1000];
		DWORD size{ sizeof buffer * sizeof *buffer };

		GetIpErrorString(errorCode, buffer, &size);

		return wa::utf8(buffer);
	}

	IcmpEchoResult makeIcmpPingResult(
		const IcmpEchoContext& context,
		cr::steady_clock::time_point replyTime)
	{
		const cr::milliseconds timeout{ context.timoutMs };

		IcmpEchoResult result{};

		result.sentTime = context.sentTime;
		result.latency = replyTime - result.sentTime;
		result.errorCode = context.errorCode;

		alignas(8) auto buffer{ context.buffer };

		if (result.latency < timeout && 
			IcmpParseReplies(buffer.data(), static_cast<DWORD>(buffer.size())) >= 1)
		{
			IcmpEchoReplyType reply;
			std::memcpy(&reply, buffer.data(), sizeof reply);

			result.statusCode = reply.Status;
			result.responder = IpEndPoint{ reply.Address };
			result.sysLatency = reply.RoundTripTime;
		}
		else
		{
			result.statusCode = IP_REQ_TIMED_OUT;
		}

		return result;
	}

	std::unique_ptr<IcmpEchoContext> asyncSendIcmpEcho(
		IpEndPoint target, 
		IpEndPoint source, 
		DWORD timeoutMs, 
		UCHAR ttl)
	{
		static constexpr auto BUFFER_SIZE{ sizeof(IcmpEchoContext::buffer) };
		static constexpr auto SEND_BYTES{ static_cast<DWORD>(32) };
		static constexpr auto RECV_BYTES{ static_cast<DWORD>(BUFFER_SIZE) };

		static_assert(BUFFER_SIZE >= sizeof(IcmpEchoReplyType));
		static_assert(SEND_BYTES <= BUFFER_SIZE);
		static_assert(RECV_BYTES <= BUFFER_SIZE);

		auto context{ std::make_unique<IcmpEchoContext>() };

		IpOptionInformationType options{};
		options.Ttl = ttl;

		IcmpSendEcho2Ex(
			context->file.get(), 
			context->event.get(), 
			nullptr, nullptr, 
			source.addr4(), 
			target.addr4(), 
			context->buffer.data(), 
			SEND_BYTES,
			reinterpret_cast<IP_OPTION_INFORMATION*>(&options),
			context->buffer.data(), 
			RECV_BYTES, 
			timeoutMs);

		context->sentTime = cr::steady_clock::now();
		context->errorCode = GetLastError();
		context->timoutMs = timeoutMs;

		return context;
	}

	bool sendIcmpEcho(
		IcmpEchoResult& result, 
		IpEndPoint target, 
		IpEndPoint source, 
		DWORD timeoutMs, 
		UCHAR ttl, 
		HANDLE stopEvent)
	{
		auto context{ asyncSendIcmpEcho(target, source, timeoutMs, ttl) };
		auto replyTime{ cr::steady_clock::now() };

		if (context->errorCode == ERROR_IO_PENDING)
		{
			context->errorCode = 0;

			const std::array<HANDLE, 2> events{ stopEvent, context->event.get() };
			const auto reason{ WaitForMultipleObjects(2, events.data(), false, timeoutMs) };

			replyTime = cr::steady_clock::now();

			// Waiting suspended by external stop event.
			if (reason - WAIT_OBJECT_0 == 0)
			{
				return false;
			}
		}
		
		result = makeIcmpPingResult(*context, replyTime);

		return true;
	}

	bool traceRoute(
		IpEndPoint& traceResult, 
		TraceType traceType, 
		HANDLE stopEvent,
		IpEndPoint traceTarget, 
		IpEndPoint source, 
		DWORD timeout,
		HWND resultHandler, 
		WPARAM resultTag)
	{
		const auto pushResult{ [resultHandler, resultTag](const auto& result) {
			SendMessageW(resultHandler, WM_TRACE_RESULT,
				resultTag, reinterpret_cast<LPARAM>(&result));
		} };

		const auto sendEcho{ [source, timeout, stopEvent]
			(IcmpEchoResult& result, IpEndPoint target, UCHAR ttl, DWORD waitTime) {
			return WaitForSingleObject(stopEvent, waitTime) == WAIT_TIMEOUT
				&& sendIcmpEcho(
					result,
					target,
					source,
					timeout,
					ttl,
					stopEvent);
		} };

		auto lastPrivateNode{ IpEndPoint::fromHostname("127.0.0.1") };

		for (UCHAR ttl{}; ttl <= 128; ++ttl)
		{
			IcmpEchoResult result;

			for (int tries{}; tries <= 2; ++tries)
			{
				if (!sendEcho(result, traceTarget, ttl, 250))
				{
					return false;
				}

				pushResult(result);

				if (result.errorCode == 0 && 
					result.statusCode == IP_TTL_EXPIRED_TRANSIT)
				{
					break;
				}
			}

			if (result.responder == traceTarget)
			{
				traceResult = result.responder;
				return true;
			}

			if (result.errorCode == 0 &&
				result.statusCode == IP_TTL_EXPIRED_TRANSIT &&
				result.responder != IpEndPoint{})
			{
				if (!result.responder.isPublicAddress())
				{
					if (!sendEcho(result, result.responder, 128, 50))
					{
						return false;
					}

					if (result.errorCode == 0 && result.statusCode == 0)
					{
						lastPrivateNode = result.responder;
					}
				}
				else
				{
					if (traceType == TraceType::LAST_PRIVATE)
					{
						traceResult = lastPrivateNode;
						return true;
					}
					else if (traceType == TraceType::FIRST_PUBLIC)
					{
						if (!sendEcho(result, result.responder, 128, 50))
						{
							return false;
						}

						pushResult(result);

						if (result.errorCode == 0 && result.statusCode == 0)
						{
							traceResult = result.responder;
							return true;
						}
					}
				}
			}
		}

		return false;
	}
}

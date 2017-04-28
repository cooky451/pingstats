#pragma once

#include <thread>

namespace utility // export
{
	template <typename DestructionPolicy>
	class ScopedThread
	{
		std::thread _thread;

	public:
		~ScopedThread()
		{
			DestructionPolicy()(_thread);
		}

		ScopedThread(ScopedThread&& other)
			: _thread(std::move(other._thread))
		{}

		ScopedThread& operator = (ScopedThread&& other)
		{
			_thread = std::move(other._thread);
			return *this;
		}

		ScopedThread(std::thread&& thr = std::thread())
			: _thread(std::move(thr))
		{}

		auto& get()
		{
			return _thread;
		}

		auto& get() const
		{
			return _thread;
		}

		auto release()
		{
			return std::move(_thread);
		}
	};

	struct ScopedThreadAutoJoinPolicy
	{
		void operator () (std::thread& thr) const
		{
			if (thr.joinable())
			{
				thr.join();
			}
		}
	};

	struct ScopedThreadAutoDetachPolicy
	{
		void operator () (std::thread& thr) const
		{
			if (thr.joinable())
			{
				thr.detach();
			}
		}
	};

	using AutojoinThread = ScopedThread<ScopedThreadAutoJoinPolicy>;
	using AutodetachThread = ScopedThread<ScopedThreadAutoDetachPolicy>;
}

#pragma once

#include <thread>

template <typename DestructionPolicy>
class scoped_thread
{
public:
	typedef std::thread::native_handle_type native_handle_type;
	typedef std::thread::id id;

private:
	std::thread thr_;

public:
	~scoped_thread()
	{
		DestructionPolicy()(thr_);
	}

	scoped_thread() noexcept
	{}

	template <typename Function, typename... Args>
	explicit scoped_thread(Function&& f, Args&&... args)
		: thr_(std::forward<Function>(f), std::forward<Args>(args)...)
	{}

	scoped_thread(const scoped_thread&) = delete;

	scoped_thread(scoped_thread&& other) noexcept
		: thr_(std::move(other.thr_))
	{}

	scoped_thread& operator = (const scoped_thread&) = delete;

	scoped_thread& operator = (scoped_thread&& other) noexcept
	{
		thr_ = std::move(other.thr_);
		return *this;
	}

	void swap(scoped_thread& other) noexcept
	{
		thr_.swap(other.thr_);
	}

	bool joinable() const noexcept
	{
		return thr_.joinable();
	}

	void join()
	{
		thr_.join();
	}

	void detach()
	{
		thr_.detach();
	}

	id get_id() const noexcept
	{
		return thr_.get_id();
	}

	native_handle_type native_handle()
	{
		return thr_.native_handle();
	}

	static unsigned hardware_concurrency() noexcept
	{
		return std::thread::hardware_concurrency();
	}
};

struct thread_join_if_joinable_policy
{
	void operator () (std::thread& thr) const
	{
		if (thr.joinable())
			thr.join();
	}
};

struct thread_detach_if_joinable_policy
{
	void operator () (std::thread& thr) const
	{
		if (thr.joinable())
			thr.detach();
	}
};

typedef scoped_thread<thread_join_if_joinable_policy> autojoin_thread;
typedef scoped_thread<thread_detach_if_joinable_policy> autodetach_thread;

#ifndef THREAD_POOL_EXECUTOR_HPP
#define THREAD_POOL_EXECUTOR_HPP

#include <queue>
#include <unordered_map>
#include <future>
#include <functional>

/**
 * C++ ThreadPoolExecutor that conforms to Java API
 * by Azuo
 */
class ThreadPoolExecutor {
public:
	template<class Rep, class Period>
	ThreadPoolExecutor(
		size_t corePoolSize,
		size_t maximumPoolSize,
		const std::chrono::duration<Rep, Period>& keepAliveTime,
		size_t queueCapacity
	);
	virtual ~ThreadPoolExecutor();

	void shutdown();
	void shutdownNow();

	template<class Rep, class Period>
	bool awaitTermination(const std::chrono::duration<Rep, Period>& timeout);

	template<class F, class... Args>
	std::future<typename std::result_of<F(Args...)>::type>
	submit(F&& f, Args&&... args);

	bool isShutdown() const { return terminate; }
	bool isTerminated() const { return terminate && workers.empty(); }

	size_t getPoolSize() const { return workers.size(); }
	size_t getActiveCount() const { return workers.size() - idles; }
	size_t getQueueSize() const { return works.size(); }
	size_t getCompletedTaskCount() const { return completions; }

	typedef std::shared_ptr<ThreadPoolExecutor> Ptr;
	static Ptr newCachedThreadPool() {
		return std::make_shared<ThreadPoolExecutor>(
			0,
			std::numeric_limits<size_t>::max(),
			std::chrono::seconds(60),
			0
		);
	}
	static Ptr newFixedThreadPool(size_t nThreads) {
		return std::make_shared<ThreadPoolExecutor>(
			nThreads,
			nThreads,
			clock::duration::max(),
			std::numeric_limits<size_t>::max()
		);
	}
	static Ptr newSingleThreadExecutor() { return newFixedThreadPool(1); }

	ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
	ThreadPoolExecutor(ThreadPoolExecutor&&) = delete;
	ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;
	ThreadPoolExecutor& operator=(ThreadPoolExecutor&&) = delete;

private:
	typedef std::chrono::steady_clock clock;

	const size_t size;
	const size_t maxSize;
	const clock::duration timeout;
	const size_t capacity;

	std::mutex mutex;
	std::condition_variable condition;
	std::condition_variable termination;

	bool terminate;
	std::unordered_map<std::thread::id, std::thread> workers;
	size_t idles;
	std::queue<std::function<void()>> works;
	size_t completions;

	template<class To, class From>
	static constexpr To duration_cast(const From& from);
};

template<class Rep, class Period>
ThreadPoolExecutor::ThreadPoolExecutor(
	size_t corePoolSize,
	size_t maximumPoolSize,
	const std::chrono::duration<Rep, Period>& keepAliveTime,
	size_t workQueueCapacity
) : size(corePoolSize),
	maxSize(maximumPoolSize),
	timeout(duration_cast<clock::duration>(keepAliveTime)),
	capacity(workQueueCapacity),
	terminate(false),
	idles(0),
	completions(0) {
	if (size < 0 || maxSize <= 0 || maxSize < size || capacity < 0 ||
		std::isnan(keepAliveTime.count()) || timeout < clock::duration::zero()
	) {
		throw std::invalid_argument(
			"Illegal arguments for constructing thread pool executor."
 		);
	}
}

inline ThreadPoolExecutor::~ThreadPoolExecutor() {
	std::unique_lock<std::mutex> lock(mutex);
	terminate = true;
	works = {};
	auto workers = std::move(this->workers);
	lock.unlock();
	condition.notify_all();
	for (auto it = workers.begin(); it != workers.end(); ++ it) {
		if (it->second.joinable())
			it->second.join();
	}
}

inline void ThreadPoolExecutor::shutdown() {
	std::unique_lock<std::mutex> lock(mutex);
	terminate = true;
	lock.unlock();
	condition.notify_all();
}

inline void ThreadPoolExecutor::shutdownNow() {
	std::unique_lock<std::mutex> lock(mutex);
	terminate = true;
	works = {};
	lock.unlock();
	condition.notify_all();
}

template<class Rep, class Period>
inline bool ThreadPoolExecutor::awaitTermination(
	const std::chrono::duration<Rep, Period>& timeout
) {
	std::unique_lock<std::mutex> lock(mutex);
	if (terminate && workers.empty())
		return true;

	auto duration = duration_cast<clock::duration>(timeout);
	if (duration <= clock::duration::zero())
		return false;

	auto now = clock::now();
	bool infinite = duration >= clock::time_point::max() - now;
	std::chrono::time_point<clock> until;
	if (!infinite)
		until = now + duration;
	while (!terminate || !workers.empty()) {
		if (infinite)
			termination.wait(lock);
		else if (
			termination.wait_until(lock, until) == std::cv_status::timeout
		) {
			return false;
		}
	}
	return true;
}

template<class F, class... Args>
std::future<typename std::result_of<F(Args...)>::type>
ThreadPoolExecutor::submit(F&& f, Args&&... args) {
	std::unique_lock<std::mutex> lock(mutex);
	const size_t pool = workers.size();
	const size_t queue = works.size();
	if (terminate) {
		throw std::logic_error(
			std::string("Task rejected from thread pool executor[") +
			(pool > 0 ? "Shutting down" : "Terminated") +
			", pool size = " + std::to_string(pool) +
			", active threads = " + std::to_string(pool - idles) +
			", queued tasks = " + std::to_string(queue) +
			", completed tasks = " + std::to_string(completions) +
			"]"
		);
	}

	bool newThread;
	if (pool < size)
		newThread = true;
	else if (pool > 0 && queue < capacity + idles)
		newThread = false;
	else if (pool < maxSize)
		newThread = true;
	else {
		throw std::runtime_error(
			"Task rejected from thread pool executor[Running"
			", pool size = " + std::to_string(pool) +
			", active threads = " + std::to_string(pool - idles) +
			", queued tasks = " + std::to_string(queue) +
			", completed tasks = " + std::to_string(completions) +
			"]"
		);
	}

	using Result = typename std::result_of<F(Args...)>::type;
	using Task = std::packaged_task<Result()>;
	auto task = std::make_shared<Task>(
		std::bind(std::forward<F>(f), std::forward<Args>(args)...)
	);

	if (newThread) {
		// tasks submitted after queue saturation will be executed immediately,
		// bypassing any already queued tasks, per Java's behavior
		std::thread worker([this](std::shared_ptr<Task>&& first) -> void {
			(*first)();
			first = nullptr;

			while (true) {
				std::unique_lock<std::mutex> lock(mutex);
				++ completions;

				auto now = clock::now();
				int wait = timeout >= clock::time_point::max() - now ? -1 :
						   timeout > clock::duration::zero() ?  1 : 0;
				std::chrono::time_point<clock> until;
				if (wait > 0)
					until = now + timeout;

				++ idles;
				while (!terminate && works.empty()) {
					if (workers.size() <= size || wait < 0)
						condition.wait(lock);
					else if (wait == 0)
						break;
					else if (
						condition.wait_until(lock, until)
							== std::cv_status::timeout
					) {
						wait = 0;
						//if (workers.size() > size) break;
					}
				}
				-- idles;

				if (works.empty()) {
					const auto it = workers.find(std::this_thread::get_id());
					if (it != workers.cend()) {
						it->second.detach();
						workers.erase(it);
					}
					if (terminate && workers.empty()) {
						std::notify_all_at_thread_exit(
							termination,
							std::move(lock)
						);
					}
					break;
				}

				auto work = std::move(works.front());
				works.pop();
				lock.unlock();
				work();
			}
		}, task);
		workers[worker.get_id()] = std::move(worker);
		lock.unlock();
	}
	else {
		works.emplace([task]() -> void { (*task)(); });
		lock.unlock();
		condition.notify_one();
	}

	std::this_thread::yield();
	return task->get_future();
}

template<class To, class From>
inline constexpr To ThreadPoolExecutor::duration_cast(const From& from) {
	return std::isnan(from.count()) ?
		   To::zero() :
		   from >= From::max() ||
		   std::chrono::duration_cast<std::chrono::duration<double>>(from) >=
		   std::chrono::duration_cast<std::chrono::duration<double>>(To::max())?
		   To::max() :
		   from <= From::min() ||
		   std::chrono::duration_cast<std::chrono::duration<double>>(from) <=
		   std::chrono::duration_cast<std::chrono::duration<double>>(To::min())?
		   To::min() :
		   std::chrono::duration_cast<To>(from);
}

/**
 * A wrapper using STL naming convention
 */
class thread_pool_executor {
public:
	template<class Rep, class Period>
	thread_pool_executor(
		size_t core_pool_size,
		size_t maximum_pool_size,
		const std::chrono::duration<Rep, Period>& keep_alive_time,
		size_t work_queue_capacity
	) : impl(
		core_pool_size,
		maximum_pool_size,
		keep_alive_time,
		work_queue_capacity
	) {}
	virtual ~thread_pool_executor() {}

	void shutdown() { impl.shutdown(); }
	void shutdown_now() { impl.shutdownNow(); };

	template<class Rep, class Period>
	bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
		return impl.awaitTermination(timeout);
	}
	void wait() { impl.awaitTermination(clock::duration::max()); }

	template<class F, class... Args>
	std::future<typename std::result_of<F(Args...)>::type>
	submit(F&& f, Args&&... args) {
		return impl.submit(std::forward<F>(f), std::forward<Args>(args)...);
	}

	bool is_shutdown() const { return impl.isShutdown(); }
	bool is_terminated() const { return impl.isTerminated(); }

	size_t pool_size() const { return impl.getPoolSize(); }
	size_t active_count() const { return impl.getActiveCount(); }
	size_t queue_size() const { return impl.getQueueSize(); }
	size_t completed_task_count() const { return impl.getCompletedTaskCount(); }

	typedef std::shared_ptr<thread_pool_executor> ptr;
	static ptr make_cached_thread_pool() {
		return std::make_shared<thread_pool_executor>(
			0,
			std::numeric_limits<size_t>::max(),
			std::chrono::seconds(60),
			0
		);
	}
	static ptr make_fixed_thread_pool(size_t threads) {
		return std::make_shared<thread_pool_executor>(
			threads,
			threads,
			clock::duration::max(),
			std::numeric_limits<size_t>::max()
		);
	}
	static ptr make_single_thread_executor() {
		return make_fixed_thread_pool(1);
	}

	thread_pool_executor(const thread_pool_executor&) = delete;
	thread_pool_executor(thread_pool_executor&&) = delete;
	thread_pool_executor& operator=(const thread_pool_executor&) = delete;
	thread_pool_executor& operator=(thread_pool_executor&&) = delete;

private:
	typedef std::chrono::steady_clock clock;
	ThreadPoolExecutor impl;
};

#endif

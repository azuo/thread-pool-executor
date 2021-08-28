#ifndef THREAD_POOL_EXECUTOR_H
#define THREAD_POOL_EXECUTOR_H

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
	std::future<std::result_of_t<F(Args...)>> submit(F&& f, Args&&... args);

	bool isShutdown() const { return terminate; }
	bool isTerminated() const { return terminate && workers.empty(); }

	size_t getActiveCount() const { return workers.size() - idles; }
	size_t getPoolSize() const { return workers.size(); }
	size_t getQueueSize() const { return works.size(); }

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
			std::chrono::duration<double>::max(),
			std::numeric_limits<size_t>::max()
		);
	}
	static Ptr newSingleThreadExecutor() { return newFixedThreadPool(1); }

	ThreadPoolExecutor(const ThreadPoolExecutor&) = delete;
	ThreadPoolExecutor(ThreadPoolExecutor&&) = delete;
	ThreadPoolExecutor& operator=(const ThreadPoolExecutor&) = delete;
	ThreadPoolExecutor& operator=(ThreadPoolExecutor&&) = delete;

private:
	const size_t size;
	const size_t maxSize;
	const std::chrono::duration<double> timeout;
	const size_t capacity;

	std::mutex mutex;
	std::condition_variable resumption;
	std::condition_variable termination;

	bool terminate;
	size_t idles;
	std::unordered_map<std::thread::id, std::thread> workers;
	std::queue<std::function<void()>> works;
};

template<class Rep, class Period>
ThreadPoolExecutor::ThreadPoolExecutor(
	size_t corePoolSize,
	size_t maximumPoolSize,
	const std::chrono::duration<Rep, Period>& keepAliveTime,
	size_t queueCapacity
) : size(corePoolSize),
	maxSize(maximumPoolSize),
	timeout(
		keepAliveTime >= std::chrono::duration<Rep, Period>::max() ?
		std::chrono::duration<double>::max() :
		keepAliveTime
	),
	capacity(queueCapacity),
	terminate(false),
	idles(0) {
	if (size < 0 || maxSize <= 0 || maxSize < size || capacity < 0 ||
		!(timeout >= std::chrono::duration<double>::zero())) // NaN
		throw std::invalid_argument("Invalid thread pool executor arguments.");
}

inline ThreadPoolExecutor::~ThreadPoolExecutor() {
	std::unique_lock<std::mutex> lock(mutex);
	terminate = true;
	works = std::queue<std::function<void()>>();
	auto workers = std::move(this->workers);
	lock.unlock();
	resumption.notify_all();
	for (auto it = workers.begin(); it != workers.end(); ++ it) {
		if (it->second.joinable())
			it->second.join();
	}
}

inline void ThreadPoolExecutor::shutdown() {
	std::unique_lock<std::mutex> lock(mutex);
	terminate = true;
	lock.unlock();
	resumption.notify_all();
}

inline void ThreadPoolExecutor::shutdownNow() {
	std::unique_lock<std::mutex> lock(mutex);
	terminate = true;
	works = std::queue<std::function<void()>>();
	lock.unlock();
	resumption.notify_all();
}

template<class Rep, class Period>
inline bool ThreadPoolExecutor::awaitTermination(
	const std::chrono::duration<Rep, Period>& timeout
) {
	std::unique_lock<std::mutex> lock(mutex);
	resumption.wait(lock, [this]() -> bool { return terminate; });
	if (workers.empty())
		return true;
	if (!(timeout > std::chrono::duration<Rep, Period>::zero())) // NaN
		return false;
	if (std::chrono::duration<Rep, Period>::max() > timeout) {
		termination.wait_for(lock, timeout);
		return workers.empty();
	}
	else {
		//termination.wait(lock);
		//return true;
		auto workers = std::move(this->workers);
		lock.unlock();
		for (auto it = workers.begin(); it != workers.end(); ++ it) {
			if (it->second.joinable())
				it->second.join();
		}
		return true;
	}
}

template<class F, class... Args>
std::future<std::result_of_t<F(Args...)>>
ThreadPoolExecutor::submit(F&& f, Args&&... args) {
	std::unique_lock<std::mutex> lock(mutex);
	if (terminate)
		throw std::runtime_error("Submit rejected: already shutdown.");

	size_t pool = workers.size();
	size_t queue = works.size();
	if (pool >= maxSize && queue >= idles && queue >= capacity)
		throw std::out_of_range("Submit rejected: pool/queue full.");

	auto task =
		std::make_shared<std::packaged_task<std::result_of_t<F(Args...)>()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...)
		);

	if (pool < size || (pool < maxSize && queue >= idles)) {
		std::thread worker([this](auto task) -> void {
			(*task)();
			task = nullptr;
			while (true) {
				std::unique_lock<std::mutex> lock(mutex);
				if (!terminate && works.empty() &&
					timeout > std::chrono::duration<double>::zero()) {
					++ idles;
					if (timeout >= std::chrono::duration<double>::max())
						resumption.wait(lock);
					else
						resumption.wait_for(lock, timeout);
					-- idles;
				}
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
	else if (queue < idles || queue < capacity) {
		works.emplace([task]() -> void { (*task)(); });
		lock.unlock();
		resumption.notify_one();
	}
	else
		throw std::runtime_error("Internal BUG!");	// should never happen

	return task->get_future();
}

/**
 * With STL naming conversions
 */
class thread_pool_executor : private ThreadPoolExecutor {
public:
	template<class Rep, class Period>
	thread_pool_executor(
		size_t core_pool_size,
		size_t maximum_pool_size,
		const std::chrono::duration<Rep, Period>& keep_alive_time,
		size_t queue_capacity
	) : ThreadPoolExecutor(
		core_pool_size,
		maximum_pool_size,
		keep_alive_time,
		queue_capacity
	) {}
	virtual ~thread_pool_executor() {}

	void shutdown() { ThreadPoolExecutor::shutdown(); }
	void shutdown_now() { shutdownNow(); };

	template<class Rep, class Period>
	bool wait_for(const std::chrono::duration<Rep, Period>& timeout) {
		return awaitTermination(timeout);
	}

	template<class F, class... Args>
	std::future<std::result_of_t<F(Args...)>> submit(F&& f, Args&&... args) {
		return ThreadPoolExecutor::submit(
			std::forward<F>(f),
			std::forward<Args>(args)...
		);
	}

	bool is_shutdown() const { return isShutdown(); }
	bool is_terminated() const { return isTerminated(); }

	size_t active_count() const { return getActiveCount(); }
	size_t pool_size() const { return getPoolSize(); }
	size_t queue_size() const { return getQueueSize(); }

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
			std::chrono::duration<double>::max(),
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
};

#endif

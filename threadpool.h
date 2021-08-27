#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <queue>
#include <unordered_map>
#include <future>
#include <functional>

class ThreadPool {
public:
	template<class Rep, class Period>
	ThreadPool(
		size_t maximumPoolSize,
		const std::chrono::duration<Rep, Period>& keepAliveTime
	);
	virtual ~ThreadPool();

	void shutdown();
	void shutdownNow();
	void awaitTermination();

	template<class F, class... Args>
	std::future<std::result_of_t<F(Args...)>> submit(F&& f, Args&&... args);

	typedef enum : size_t {
		unlimited = 0,
		singleThread = 1
	} Size;

	typedef std::shared_ptr<ThreadPool> Ptr;
	static Ptr newCachedThreadPool() {
		return std::make_shared<ThreadPool>(
			unlimited,
			std::chrono::seconds(60)
		);
	}
	static Ptr newFixedThreadPool(size_t nThreads) {
		return std::make_shared<ThreadPool>(
			nThreads,
			std::chrono::duration<double>::max()
		);
	}

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool(ThreadPool&&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
	ThreadPool& operator=(ThreadPool&&) = delete;

private:
	const size_t size;
	const std::chrono::duration<double> timeout;

	std::mutex mutex;
	std::condition_variable condition;

	bool stop;
	size_t idles;
	std::unordered_map<std::thread::id, std::thread> workers;
	std::queue<std::function<void()>> tasks;
};

template<class Rep, class Period>
ThreadPool::ThreadPool(
	size_t maximumPoolSize,
	const std::chrono::duration<Rep, Period>& keepAliveTime
) :
	size(maximumPoolSize),
	timeout(
		// keepAliveTime < std::chrono::duration<Rep, Period>::max() ?
		std::chrono::duration<Rep, Period>::max() > keepAliveTime ?
		keepAliveTime :
		std::chrono::duration<double>(-1)
	),
	stop(false),
	idles(0) {
}

inline ThreadPool::~ThreadPool() {
	std::unique_lock<std::mutex> lock(mutex);
	stop = true;
	tasks = std::queue<std::function<void()>>();
	auto workers = std::move(this->workers);
	lock.unlock();
	condition.notify_all();
	for (auto it = workers.begin(); it != workers.end(); ++ it) {
		if (it->second.joinable())
			it->second.join();
	}
}

inline void ThreadPool::shutdown() {
	std::unique_lock<std::mutex> lock(mutex);
	stop = true;
	condition.notify_all();
}

inline void ThreadPool::shutdownNow() {
	std::unique_lock<std::mutex> lock(mutex);
	stop = true;
	tasks = std::queue<std::function<void()>>();
	condition.notify_all();
}

inline void ThreadPool::awaitTermination() {
	std::unique_lock<std::mutex> lock(mutex);
	condition.wait(lock, [this]() -> bool { return stop; });
	auto workers = std::move(this->workers);
	lock.unlock();
	for (auto it = workers.begin(); it != workers.end(); ++ it) {
		if (it->second.joinable())
			it->second.join();
	}
}

template<class F, class... Args>
std::future<std::result_of_t<F(Args...)>>
ThreadPool::submit(F&& f, Args&&... args) {
	if (stop)	// no need to lock
		throw std::runtime_error("submit to stopped ThreadPool.");

	auto task =
		std::make_shared<std::packaged_task<std::result_of_t<F(Args...)>()>>(
			std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	std::unique_lock<std::mutex> lock(mutex);
	if (idles <= 0 && (size <= 0 || workers.size() < size)) {
		std::thread worker([this](auto task) -> void {
			(*task)();
			task = nullptr;
			while (true) {
				std::unique_lock<std::mutex> lock(mutex);
				if (!stop && tasks.empty() && timeout.count() != 0) {
					++ idles;
					if (timeout.count() < 0)
						condition.wait(lock);
					else
						condition.wait_for(lock, timeout);
					-- idles;
				}
				if (tasks.empty()) {
					const auto it = workers.find(std::this_thread::get_id());
					if (it != workers.cend()) {
						it->second.detach();
						workers.erase(it);
					}
					break;
				}
				auto f = std::move(tasks.front());
				tasks.pop();
				lock.unlock();

				f();
			}
		}, task);
		workers[worker.get_id()] = std::move(worker);
	}
	else {
		tasks.emplace([task]() -> void { (*task)(); });
		condition.notify_one();
	}
	lock.unlock();

	return task->get_future();
}

#endif

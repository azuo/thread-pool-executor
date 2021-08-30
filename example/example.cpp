#include <iostream>
#include <iomanip>
#include <sstream>
#include <random>

#include "../thread_pool_executor.hpp"

struct logger {
	std::stringstream out;
	logger() {
		auto now = std::chrono::system_clock::now();
		auto tm = std::chrono::system_clock::to_time_t(now);
		int ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()
		).count() % 1000;
		out << std::put_time(std::localtime(&tm), "%Y-%m-%d %H:%M:%S.")
			<< std::setfill('0') << std::setw(3) << ms << " ";
	}
	~logger() {
		out << std::endl;
		std::cout << out.str() << std::flush;
	}
};

#define LOG      logger().out
#define TLOG(id) LOG << "[thread-" << id << "] "

struct lifecycle_logger {
	static std::atomic_int nextId;
	const int id;
	lifecycle_logger() : id(++ nextId) { TLOG(id) << "+++"; }
	~lifecycle_logger()                { TLOG(id) << "---"; }
};
std::atomic_int lifecycle_logger::nextId;

int main() {
	thread_pool_executor executor(1, 3, std::chrono::seconds(0), 2);
	std::vector<std::future<int>> futures;

    std::default_random_engine random((std::random_device())());
    std::uniform_int_distribution<int> udist(1000, 5000);

	for (int i = 0; i < 2; ++ i) {
		for (int j = 0; j < 4; ++ j) {
			int n = i * 4 + j + 1;
			try {
				futures.push_back(executor.submit([](int n, int ms) -> int {
					thread_local lifecycle_logger lc;
					TLOG(lc.id) << "task " << n << " +++";
					std::this_thread::sleep_for(std::chrono::milliseconds(ms));
					TLOG(lc.id) << "task " << n << " --- (" << ms << "ms)";
					return n;
				}, n, udist(random)));
			}
			catch (const std::exception& e) {
				LOG << "submit " << n << ": *** ERROR *** " << e.what();
				break;
			}
			LOG << "submit " << n << ": pool = " << executor.pool_size()
				<< " (" << executor.active_count()
				<< " active), queue = " << executor.queue_size()
				<< ", completed = " << executor.completed_task_count();
		}
		if (i == 0)
			std::this_thread::sleep_for(std::chrono::seconds(5));
    }

	for (auto&& future : futures) {
		int result = future.get();
		LOG << "result " << result;
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));

	LOG << "shutdown: pool = " << executor.pool_size()
		<< " (" << executor.active_count()
		<< " active), queue = " << executor.queue_size()
		<< ", completed = " << executor.completed_task_count();

	executor.shutdown();
	executor.wait();

	LOG << "terminated: pool = " << executor.pool_size()
		<< " (" << executor.active_count()
		<< " active), queue = " << executor.queue_size()
		<< ", completed = " << executor.completed_task_count();
	return 0;
}

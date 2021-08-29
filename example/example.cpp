#include <iostream>
#include <iomanip>
#include <random>

#include "../thread_pool_executor.hpp"

struct logger {
	std::stringstream stream;
	logger() {
		auto now = std::chrono::system_clock::now();
		auto tm = std::chrono::system_clock::to_time_t(now);
		int ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			now.time_since_epoch()
		).count() % 1000;
		stream << std::put_time(std::localtime(&tm), "%Y-%m-%d %H:%M:%S.")
			   << std::setfill('0') << std::setw(3) << ms << " ";
	}
	~logger() {
		stream << std::endl;
		std::cout << stream.str() << std::flush;
	}
};

#define LOG  logger().stream
#define TLOG LOG << "[thread-" << std::this_thread::get_id() << "] "

struct life_cycle {
	life_cycle()  { TLOG << "+++"; }
	~life_cycle() { TLOG << "---"; }
};

using namespace std::chrono_literals;

int main() {
	thread_pool_executor executor(1, 3, 0s, 2);
	std::vector<std::future<int>> futures;

    std::default_random_engine random;
    std::uniform_int_distribution<int> udist(1, 5);

	for (int i = 0; i < 2; ++ i) {
		for (int j = 0; j < 5; ++ j) {
			int n = i * 5 + j;
			try {
				futures.push_back(executor.submit([](int n, int s) -> int {
					static thread_local life_cycle lc;
					TLOG << "task " << n << " +++";
					std::this_thread::sleep_for(std::chrono::seconds(s));
					TLOG << "task " << n << " ---";
					return n;
				}, n, udist(random)));
			}
			catch (const std::exception& e) {
				LOG << "submit " << n << ": *** ERROR *** " << e.what();
				break;
			}
			LOG << "submit " << n << ": pool = " << executor.pool_size()
				<< " (" << executor.active_count()
				<< " active), queue = " << executor.queue_size();
		}
		if (i == 0)
			std::this_thread::sleep_for(5s);
    }

	for (auto&& future : futures) {
		int result = future.get();
		LOG << "result " << result;
	}

	std::this_thread::sleep_for(3s);

	LOG << "shutdown: pool = " << executor.pool_size()
		<< " (" << executor.active_count()
		<< " active), queue = " << executor.queue_size();

	executor.shutdown();
	executor.wait();

	LOG << "terminated: pool = " << executor.pool_size()
		<< " (" << executor.active_count()
		<< " active), queue = " << executor.queue_size();
	return 0;
}

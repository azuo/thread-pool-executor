# C++ ThreadPoolExecutor

A simple C++ thread pool implementation that conforms to Java's API.

Usage example:
```c++
ThreadPoolExecutor::Ptr executor = ThreadPoolExecutor::newCachedThreadPool();
/*
ThreadPoolExecutor::Ptr executor = ThreadPoolExecutor::newFixedThreadPool(5);
ThreadPoolExecutor::Ptr executor = ThreadPoolExecutor::newSingleThreadExecutor();
ThreadPoolExecutor::Ptr executor = std::make_shared<ThreadPoolExecutor>(
    1,
    std::numeric_limits<size_t>::max(),
    std::chrono::seconds(60),
    10
);
*/

executor->submit([] { ... });
std::future<int> future = executor->submit([] { ... return result; });

int result = future.get();

executor->shutdown();
executor->awaitTermination(std::chrono::nanoseconds::max());
```

Or with C++ standard library naming conversions:
```c++
thread_pool_executor::ptr executor = thread_pool_executor::make_cached_thread_pool();
/*
thread_pool_executor::ptr executor = thread_pool_executor::make_fixed_thread_pool(5);
thread_pool_executor::ptr executor = thread_pool_executor::make_single_thread_executor();
thread_pool_executor::ptr executor = std::make_shared<thread_pool_executor>(
    1,
    std::numeric_limits<size_t>::max(),
    std::chrono::seconds(60),
    10
);
*/

executor->submit([] { ... });
std::future<int> future = executor->submit([] { ... return result; });

int result = future.get();

executor->shutdown();
executor->wait_for(std::chrono::nanoseconds::max());
```

Java equivalent:
```java
ExecutorService executor = Executors.newCachedThreadPool();
/*
ExecutorService executor = Executors.newFixedThreadPool(5);
ExecutorService executor = Executors.newSingleThreadExecutor();
ExecutorService executor = new ThreadPoolExecutor(
    1,
    Integer.MAX_VALUE,
    60, TimeUnit.SECONDS,
    new ArrayBlockingQueue<Runnable>(10)
);
*/

executor.submit(() -> { ... });
Future<Integer> future = executor.submit(() -> { ... return result; });

int result = future.get();

executor.shutdown();
executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
```

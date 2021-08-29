# C++ ThreadPoolExecutor

A simple C++ thread pool implementation that conforms to Java's API and
behaviors.

* C++11
* A single header file, no external dependencies
* API and behaviors are consistent with Java's ThreadPoolExecutor

## Usage example

```c++
ThreadPoolExecutor::Ptr executor = ThreadPoolExecutor::newCachedThreadPool();
/*
ThreadPoolExecutor::Ptr executor = ThreadPoolExecutor::newFixedThreadPool(5);
ThreadPoolExecutor::Ptr executor = ThreadPoolExecutor::newSingleThreadExecutor();
ThreadPoolExecutor::Ptr executor = std::make_shared<ThreadPoolExecutor>(
    1,                                      // corePoolSize
    std::numeric_limits<size_t>::max(),     // maximumPoolSize
    std::chrono::seconds(60),               // keepAliveTime
    10                                      // workQueueCapacity
);
*/

executor->submit([] { ... });
std::future<int> future = executor->submit([] { ... return result; });

int result = future.get();

executor->shutdown();
executor->awaitTermination(std::chrono::nanoseconds::max());
```

**Java equivalent:**

```java
ExecutorService executor = Executors.newCachedThreadPool();
/*
ExecutorService executor = Executors.newFixedThreadPool(5);
ExecutorService executor = Executors.newSingleThreadExecutor();
ExecutorService executor = new ThreadPoolExecutor(
    1,                                      // corePoolSize
    Integer.MAX_VALUE,                      // maximumPoolSize
    60, TimeUnit.SECONDS,                   // keepAliveTime & unit
    new ArrayBlockingQueue<Runnable>(10)    // workQueue
);
*/

executor.submit(() -> { ... });
Future<Integer> future = executor.submit(() -> { ... return result; });

int result = future.get();

executor.shutdown();
executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
```

## Alternate naming conversion

If preferred, a thin wrapper class is also available that uses
lower_case_with_underscores naming conversion, like that used by C++ standard
library:

```c++
thread_pool_executor::ptr executor = thread_pool_executor::make_cached_thread_pool();
/*
thread_pool_executor::ptr executor = thread_pool_executor::make_fixed_thread_pool(5);
thread_pool_executor::ptr executor = thread_pool_executor::make_single_thread_executor();
thread_pool_executor::ptr executor = std::make_shared<thread_pool_executor>(
    1,                                      // core_pool_size
    std::numeric_limits<size_t>::max(),     // maximum_pool_size
    std::chrono::seconds(60),               // keep_alive_time
    10                                      // work_queue_capacity
);
*/

executor->submit([] { ... });
std::future<int> future = executor->submit([] { ... return result; });

int result = future.get();

executor->shutdown();
executor->wait();   // executor->wait_for(std::chrono::nanoseconds::max());
```

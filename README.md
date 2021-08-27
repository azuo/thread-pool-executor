# C++ Thread Pool

A simple C++ thread pool implementation that conforms to Java's API.

Usage example:
```c++
ThreadPool::Ptr pool = ThreadPool::newFixedThreadPool(2);
//ThreadPool::Ptr pool = ThreadPool::newCachedThreadPool();
//ThreadPool::Ptr pool = std::make_shared<ThreadPool>(4, std::chrono::seconds(60));

std::future<int> compute = pool->submit([] { return computeInBackground(); });
pool->submit([] { runInBackground(); });

int result = compute.get();

pool->shutdown();
pool->awaitTermination();
```

Java equivalent:
```java
ExecutorService pool = Executors.newFixedThreadPool(2);
//ExecutorService pool = Executors.newCachedThreadPool();
//ExecutorService pool = new ThreadPoolExecutor(0, 4, 60, TimeUnit.SECONDS, new LinkedBlockingQueue<Runnable>());

Future<Integer> compute = pool.submit(() -> { return computeInBackground(); });
pool.submit(() -> { runInBackground(); });

int result = compute.get();

pool.shutdown();
pool.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
```

# C++ Thread Pool

A simple C++ thread pool implementation that conforms to Java's API.

Usage example:
```c++
ThreadPool::Ptr pool = ThreadPool::newFixedThreadPool(1);
//ThreadPool::Ptr pool = ThreadPool::newCachedThreadPool();
//ThreadPool::Ptr pool = std::make_shared<ThreadPool>(4, std::chrono::seconds(60));

std::future<int> future = pool->submit([] {
    std::cout << "A start" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout << "A stop" << std::endl;
    return 100;
});
pool->submit([] {
    std::cout << "B start" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    std::cout << "B stop" << std::endl;
});

int result = future.get();
std::cout << "A result: " << result << std::endl;

pool->shutdown();
pool->awaitTermination();
```

Java equivalent:
```java
ExecutorService pool = Executors.newFixedThreadPool(1);
//ExecutorService pool = Executors.newCachedThreadPool();
//ExecutorService pool = new ThreadPoolExecutor(0, 4, 60, TimeUnit.SECONDS, new LinkedBlockingQueue<Runnable>());

Future<Integer> future = pool.submit(() -> {
    System.out.println("A start");
    try { Thread.sleep(1000); } catch (InterruptedException e) {}
    System.out.println("A stop");
    return 100;
});
pool.submit(() -> {
    System.out.println("B start");
    try { Thread.sleep(1500); } catch (InterruptedException e) {}
    System.out.println("B stop");
});

try {
    int result = future.get();
    System.out.println("A result: " + result);
} catch (ExecutionException e) {
}

pool.shutdown();
pool.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);
```

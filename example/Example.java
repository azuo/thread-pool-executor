import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Random;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.Future;
import java.util.concurrent.RejectedExecutionException;
import java.util.concurrent.ThreadFactory;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public class Example {
	private static void LOG(String m) {
		System.out.println(
			new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS").format(new Date())
			+ " " + m
		);
	}

	private static void TLOG(String m) {
		LOG("[" + Thread.currentThread().getName() + "] " + m);
	}

	private static class LifecycleLogger implements ThreadFactory {
		private final AtomicInteger nextId = new AtomicInteger();
		@Override
		public Thread newThread(Runnable r) {
			Thread t = new Thread(r, "thread-" + nextId.incrementAndGet()) {
				@Override
				public void run() {
					TLOG("+++");
					super.run();
					TLOG("---");
				};
			};
			t.setDaemon(true);
			return t;
		}
	}

	public static void main(String[] args) throws Exception {
		ThreadPoolExecutor executor = new ThreadPoolExecutor(
			1, 3, 0, TimeUnit.SECONDS, new ArrayBlockingQueue<>(2),
			new LifecycleLogger()
		);
		List<Future<Integer>> futures = new ArrayList<>(8);

		Random random = new Random();

		for (int i = 0; i < 2; ++ i) {
			for (int j = 0; j < 4; ++ j) {
				final int n = i * 4 + j + 1;
				try {
					futures.add(executor.submit(() -> {
						TLOG("task " + n + " +++");
						int ms = random.nextInt(4001) + 1000;
						try {
							Thread.sleep(ms);
						}
						catch (InterruptedException e) {
						}
						TLOG("task " + n + " --- (" + ms + "ms)");
						return n;
					}));
				}
				catch (RejectedExecutionException e) {
					LOG("submit " + n + ": *** ERROR *** " + e.getMessage());
					break;
				}
				LOG("submit " + n + ": pool = " + executor.getPoolSize() +
					" (" + executor.getActiveCount() +
					" active), queue = " + executor.getQueue().size() +
					", completed = " + executor.getCompletedTaskCount());
			}
			if (i == 0)
				Thread.sleep(5000);
	    }

		for (Future<Integer> future : futures) {
			int result = future.get();
			LOG("result " + result);
		}

		Thread.sleep(3000);

		LOG("shutdown: pool = " + executor.getPoolSize() +
			" (" + executor.getActiveCount() +
			" active), queue = " + executor.getQueue().size() +
			", completed = " + executor.getCompletedTaskCount());

		executor.shutdown();
		executor.awaitTermination(Long.MAX_VALUE, TimeUnit.NANOSECONDS);

		LOG("terminated: pool = " + executor.getPoolSize() +
			" (" + executor.getActiveCount() +
			" active), queue = " + executor.getQueue().size() +
			", completed = " + executor.getCompletedTaskCount());
	}
}

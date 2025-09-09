#include<iostream>
#include"threadpool.h"
int myadd(int s, int e)
{
	int sum=0;
	for (int i = s; i <= e; i++)
		sum += i;
	std::this_thread::sleep_for(std::chrono::seconds(1));
	return sum;
}
int main()
{
    
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED);
        //pool.setMode(PoolMode::MODE_CACHED);
        pool.start(4);
        std::future<int> ans1 = pool.submitTask(myadd, 1, 1000000);
        std::future<int> ans2 = pool.submitTask(myadd, 1, 1000);
        std::future<int> ans3 = pool.submitTask(myadd, 1, 100);
        std::future<int> ans4 = pool.submitTask(myadd, 1, 10000);
        std::future<int> ans5 = pool.submitTask(myadd, 1, 10000);
        std::future<int> ans6 = pool.submitTask(myadd, 1, 10000);
        std::future<int> ans7 = pool.submitTask(myadd, 1, 10000);
        std::future<int> ans8 = pool.submitTask(myadd, 1, 10000);
        std::future<int> ans9 = pool.submitTask(myadd, 1, 10000);
        std::future<int> ans10 = pool.submitTask(myadd, 1, 10000);
        std::future<int> ans11 = pool.submitTask(myadd, 1, 10000);

        // 检查future是否有效，然后调用get()
         std::cout << ans1.get() << std::endl;
        std::cout << ans2.get() << std::endl;
        std::cout << ans3.get() << std::endl;
        std::cout << ans4.get() << std::endl;
         std::cout << ans5.get() << std::endl;
         std::cout << ans6.get() << std::endl;
        std::cout << ans7.get() << std::endl;
        std::cout << ans8.get() << std::endl;
         std::cout << ans9.get() << std::endl;
         std::cout << ans10.get() << std::endl;
         std::cout << ans11.get() << std::endl;

         std::this_thread::sleep_for(std::chrono::seconds(10));
         std::cout << "main thread is end" << std::endl;
    return 0;
}
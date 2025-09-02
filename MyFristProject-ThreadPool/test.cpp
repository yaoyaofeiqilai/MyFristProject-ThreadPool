#include<iostream>
#include<chrono>
#include <thread>

#include"threadpool.h"


class MyTask :public Task
{
public:
	Any run()
	{
		long long sum = 0;
		for (int i = start; i <= end; i++)
		{
			sum += i;
		}
		return sum;
	}
private:
	int start;
	int end;
};
int main()
{
	ThreadPool pool;
	pool.start(4);


	/*Result res=pool.submitTask(std::make_shared<MyTask>());

	int sum =res.get().castto<int>
	std::getchar();*/

	return 0;
}
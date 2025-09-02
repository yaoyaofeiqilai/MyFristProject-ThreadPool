#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 4;


//////////////////////////////////////////////////////////////////////////////线程池构造函数
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
{}

//线程池析构函数
ThreadPool::~ThreadPool()
{

}

void ThreadPool::start(int initThreadSize)
{
	//初始化线程数量
	initThreadSize_ = initThreadSize;

	//创建线程
	for (int i = 0; i < initThreadSize; i++)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}

	//启动线程
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
	}
};//开启线程池

void  ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
};   //设置线程池模式

void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	taskQueMaxThreshHold_ = threshhold;
};//设置任务队列阈值

Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//等待任务队列右空余,超过一秒钟返回失败
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskSize_ < taskQueMaxThreshHold_; }))
	{
		std::cerr << "task queue if full , submit is fail" << std::endl;
		return Result(sp,false);
	};
	//提交任务
	taskQue_.emplace(sp);
	taskSize_++;
	notEmpty_.notify_all();
	return Result(sp);
};   //用户提交任务接口



//////////////////////////////////////////////////////定义线程函数
void ThreadPool::threadFunc()
{
	/*std::cout << "begin threadFunc id:" <<std::this_thread::get_id() << std::endl;
	std::cout << "end threadFunc" << std::endl;*/

	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			//获取锁
			std::cout << std::this_thread::get_id() << "尝试获取任务" << std::endl;
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			//等待notempty
			notEmpty_.wait(lock, [&]()->bool {return taskSize_ > 0; });
			//从队列中取一个任务
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//通知其他线程
			if(taskSize_>0)notEmpty_.notify_all();
			std::cout << std::this_thread::get_id() << "尝试获取任务成功" << std::endl;
			notFull_.notify_all();
		}
	//执行任务
		
		if (task != nullptr)
		{
			task->run();
		}
	}

}


////////////////////////////////////////////////////线程相关定义
Thread::Thread(ThreadFunc func)
	:func_(func)    //获取bind返回的函数对象，即threadpool中定义的线程函数
{}


Thread::~Thread()
{
	   
}

//启动线程
void Thread::start()
{
	std::thread t(func_);   //线程对象t 和线程函数func_
	t.detach();     //设置分离线程
}

Result::Result(std::shared_ptr<Task> task , bool isValid )
	:task_(task)
	,isValid_(isValid)
{};

//设置线程运行结果
void Result::setVal()
{

}

Any Result::get()
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();
	return std::move(any_);
}
#ifndef THREADPOOL_H
#define THREADPOOL_H


#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include<thread>
#include <iostream>
enum class PoolMode
{
	MODE_FIXED,     //固定模式，
	MODE_CACHED,    //可增长模式
};


//线程类型
class Thread
{
public:
	using ThreadFunc = std::function<void()>;
	Thread(ThreadFunc func);//构造函数
	~Thread();//析构函数
	void start();
private:
	ThreadFunc func_;
};

class Any
{
public:
	Any(const Any&) = delete;      //unique不能有拷贝构造
	Any& operator=(const Any&) = delete;
	Any() = default;

	template<typename T>
	Any(T date)
		:sp_(std::make_unique<Derive<T>>(date))   //基类指针指向子类
	{};
	~Any()=default;
private:
	//基类
	class Base
	{
	public: virtual ~Base() = default;
	};

	//子类
	template<typename T>
	class Derive:public Base
	{
	public:
		Derive(T date)
			:date_(date)
		{};
		~Derive() = default;
		T date_;
	};

	template<typename T>
	T castto()
	{
		Derive<T>* ptr = dynamic_cast<Derive<T>*>(sp_.get());
		if (ptr == nullptr)
		{
			throw"type is unmath!";
		}
		return ptr->date_;
	}
private:
	std::unique_ptr<Base> sp_;
};

class Result;

//任务基类
class Task
{
public:
	virtual Any run() = 0;
};




class Semaphore    //信号量，实现线程通信
{
public:
	Semaphore(int cnt=0)
		:cnt_(cnt)
	{}
	~Semaphore() = default;

	void wait()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cv_.wait(lock, [&]()->bool {return cnt_ > 0; });
		cnt_--;
	}

	void post()
	{
		std::unique_lock<std::mutex> lock(mtx_);
		cnt_++;
		cv_.notify_all();
	}
private:
	int cnt_;
	std::mutex mtx_;
	std::condition_variable cv_;
};


//任务返回值类型
class Result
{
public:
	Result(std::shared_ptr<Task> task = NULL, bool isvaild = true);
	~Result() = default;

	//设置线程运行结果
	void setVal();

	Any get();
private:
	Semaphore sem_;     //线程通信信号量
	Any any_;   //接收线程放回值
	std::shared_ptr<Task> task_;   //对应的任务
	std::atomic_bool isValid_;   //是否提交成功
};

//线程池类型
class ThreadPool
{
public:
	ThreadPool();     //线程池构造函数
	~ThreadPool();     //线程池析构函数

	void start(int initThreadSize=4);//开启线程池

	void setMode(PoolMode mode);   //设置线程池模式

	void setTaskQueMaxThreshHold(int threshhold);//设置任务队列阈值

	Result submitTask(std::shared_ptr<Task> sp);   //用户提交任务接口

	//关闭拷贝构造函数
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	//定义线程函数
	void threadFunc();
private:
	std::vector<std::unique_ptr<Thread>> threads_;  //线程列表
	int initThreadSize_;  //初始线程数量

	std::queue<std::shared_ptr<Task>>  taskQue_; //任务队列
	std::atomic_int taskSize_;   //任务数量
	int taskQueMaxThreshHold_;   //最大任务数量

	std::mutex taskQueMtx_;  //任务队列互斥锁
	std::condition_variable notFull_;  //表示任务队列不满，用户可以添加任务
	std::condition_variable notEmpty_; //表示任务队列不空，线程可以消耗任务

	PoolMode poolMode_;   //当前线程池的模式
};
#endif
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
#include <unordered_map>
#include <future>

const int TASK_MAX_THRESHHOLD = 4;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME = 60;  //�߳�������ʱ�䣬��λ��

enum class PoolMode
{
	MODE_FIXED,     //�̶�ģʽ��
	MODE_CACHED,    //������ģʽ
};


//�߳�����
class Thread
{
public:
	using ThreadFunc = std::function<void(int)>;



	Thread(ThreadFunc func)
		:func_(func)    //��ȡbind���صĺ������󣬼�threadpool�ж�����̺߳���
		, threadId_(generateId_++)
	{}

	~Thread() = default;


	void start() 
	{
		std::thread t(func_, threadId_);   //�̶߳���t ���̺߳���func_
		std::cout << "�߳�id:" << t.get_id() << "�߳��Ѵ���" << std::endl;
		t.detach();     //���÷����߳�
	}//����һ��std::thread


	int getThreadId() const
	{
		return threadId_;
	}//��ȡ�߳�id//��ȡ�߳�id
private:
	static int generateId_;
	ThreadFunc func_;
	int threadId_;
};


int Thread::generateId_ = 0;
//�̳߳�����
class ThreadPool
{
public:

	ThreadPool()
		:initThreadSize_(0)
		, taskSize_(0)
		, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
		, poolMode_(PoolMode::MODE_FIXED)
		, idleThreadSize_(0)
		, isPoolRunning_(false)
		, curThreadSize_(0)
		, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	{}//�̳߳ع��캯��

	//�̳߳���������
	~ThreadPool()
	{
		isPoolRunning_ = false;     //�����߳�״̬
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		notEmpty_.notify_all();     //�������еȴ��߳�
		poolExit_.wait(lock, [&]()->bool {return threads_.size() == 0; });    //�ȴ��߳�ȫ������
	}

	void  setMode(PoolMode mode)
	{
		if (checkRunningState())    //�����������״̬�򲻿����޸�
			return;
		poolMode_ = mode;
	};   //�����̳߳�ģʽ



	
	void setTaskQueMaxThreshHold(int threshhold)
	{
		if (checkRunningState())    //�����������״̬�򲻿����޸�
			return;
		taskQueMaxThreshHold_ = threshhold;
	};//�������������ֵ


	void setThreadSizeMaxThreshHold(int threshhold)
	{
		if (checkRunningState())
			return;
		if (poolMode_ == PoolMode::MODE_CACHED)
		{
			threadSizeThreshHold_ = threshhold;
		}
	}//����chachedģʽ������߳�����





	template<typename Func, typename...Args>
	auto submitTask(Func&& func, Args&&... args)->std::future<decltype(func(args...))>
	{
		using RType = decltype(func(args...));
		//��װ��
		auto task = std::make_shared<std::packaged_task<RType()>>
			(std::bind(std::forward<Func>(func), std::forward<Args>(args)...));
		std::future<RType> result = task->get_future();

		//��ȡ��
		std::unique_lock<std::mutex> lock(taskQueMtx_);
		//�ȴ���������ҿ���,����һ���ӷ���ʧ��
		if (!notFull_.wait_for(lock, std::chrono::seconds(1),
			[&]()->bool {return taskSize_ < taskQueMaxThreshHold_; }))
		{
			std::cerr << "task queue if full , submit is fail" << std::endl;
			//return std::future<Rtype>();
			auto task = std::make_shared<std::packaged_task<RType()>>(
				[]()->RType { return RType(); });
			(*task)();
			return task->get_future();
		};
		//�ύ����
		taskQue_.emplace([task]() {(*task)(); });
		taskSize_++;
		notEmpty_.notify_all();

		//���ݿ����߳������������������ж��Ƿ���Ҫ����µ��߳�
		if (poolMode_ == PoolMode::MODE_CACHED &&
			taskSize_ > idleThreadSize_ &&
			curThreadSize_ < threadSizeThreshHold_)
		{
			//�����µ��߳�
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			int threadid = ptr->getThreadId();         //��ȡid
			threads_.emplace(threadid, std::move(ptr));

			threads_[threadid]->start();   //�����߳�
			curThreadSize_++;           //��ǰ�Լ������̼߳�һ
			idleThreadSize_++;
		}
		return result;  //����result����
	};   //�û��ύ����ӿ�













	void start(int initThreadSize)
	{
		//�����̳߳�״̬
		isPoolRunning_ = true;
		//��ʼ���߳�����
		initThreadSize_ = initThreadSize;
		curThreadSize_ = initThreadSize;
		//�����߳�
		for (int i = 0; i < initThreadSize; i++)
		{
			auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
			threads_.emplace(ptr->getThreadId(), std::move(ptr));
		}

		//�����߳�
		for (int i = 0; i < initThreadSize_; i++)
		{
			threads_[i]->start();
			idleThreadSize_++;
		}
	};//�����̳߳�
	//�رտ������캯��





	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	//�����̺߳���
	//////////////////////////////////////////////////////�����̺߳���
	void threadFunc(int threadid)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();
		while (true)
		{
			
			Task task;    //std::queue<std::function<void()>>  taskQue_; //�������
			{
				//��ȡ��
				std::unique_lock<std::mutex> lock(taskQueMtx_);     //���Ի�ȡ��
				std::cout << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;


				//ÿ�뷵��һ��     ���ֳ�ʱ���أ������������ִ�з���
				while (taskQue_.size() == 0)     //������ֱ��ȡִ������
				{
					if (!isPoolRunning_)     //û����ʱ���ж��̳߳��Ƿ����������ֱ���˳������������߳�
					{
						curThreadSize_--;
						threads_.erase(threadid);
						poolExit_.notify_all();
						std::cout << "threadid:" << std::this_thread::get_id() << "�ѻ���" << std::endl;
						return;
					}

					//�̳߳�δ����������ȴ�״̬
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						//cachedģʽ�£�����ʱ�䳬��60s���߳�,���л���
						//��ǰʱ��-last�ϴ��߳�ִ�����ʱ��
						if (std::cv_status::timeout == 
							notEmpty_.wait_for(lock, std::chrono::seconds(1)))   //��ʱ�˳�
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto cur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);    //����ֹͣʱ��
							if (cur.count() >= THREAD_MAX_IDLE_TIME &&
								curThreadSize_ > initThreadSize_)
							{
								//��ʱ�����߳�
								threads_.erase(threadid);
								curThreadSize_--;
								idleThreadSize_--;
								std::cout << "threadid:" << std::this_thread::get_id() <<
									"�ѻ���" << std::endl;
								poolExit_.notify_all();
								return;
							}
						}
					}
					else
					{
						//�ȴ�notempty,fixedģʽ
						notEmpty_.wait(lock);
					}
				}
				idleThreadSize_--;
				//�Ӷ�����ȡһ������
				task = taskQue_.front();
				if (taskQue_.empty())std::cout << "error!!!!!!!!!!!!!!!!" << std::endl;
				taskQue_.pop();
				taskSize_--;

				//֪ͨ�����߳�
				if (taskQue_.size() > 0)notEmpty_.notify_all();

				//std::cout << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;
				notFull_.notify_all();
				//ִ������
			}	

			if (task != nullptr)
			{
				task();
			}

			idleThreadSize_++;
			lastTime = std::chrono::high_resolution_clock().now();
		}
	}

























	bool checkRunningState() const
	{
		return isPoolRunning_;   //δ�������и�����ж�
	}

private:
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;  //�߳��б�
	int initThreadSize_;  //��ʼ�߳�����
	std::atomic_int idleThreadSize_; //�����߳�����
	int threadSizeThreshHold_;     //�߳�����������
	std::atomic_int curThreadSize_;  //��ǰ�߳�����

	using Task = std::function<void()>;
	std::queue<Task>  taskQue_; //�������
	std::atomic_int taskSize_;   //��������
	int taskQueMaxThreshHold_;   //�����������

	std::mutex taskQueMtx_;  //������л�����
	std::condition_variable notFull_;  //��ʾ������в������û������������
	std::condition_variable notEmpty_; //��ʾ������в��գ��߳̿�����������

	PoolMode poolMode_;   //��ǰ�̳߳ص�ģʽ
	std::atomic_bool isPoolRunning_;//��ǰ�̳߳�״̬
	std::condition_variable poolExit_;
};
#endif


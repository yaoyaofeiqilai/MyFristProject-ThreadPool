#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 4;


//////////////////////////////////////////////////////////////////////////////�̳߳ع��캯��
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
{}

//�̳߳���������
ThreadPool::~ThreadPool()
{

}

void ThreadPool::start(int initThreadSize)
{
	//��ʼ���߳�����
	initThreadSize_ = initThreadSize;

	//�����߳�
	for (int i = 0; i < initThreadSize; i++)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
		threads_.emplace_back(std::move(ptr));
	}

	//�����߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
	}
};//�����̳߳�

void  ThreadPool::setMode(PoolMode mode)
{
	poolMode_ = mode;
};   //�����̳߳�ģʽ

void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	taskQueMaxThreshHold_ = threshhold;
};//�������������ֵ

Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ��
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	//�ȴ���������ҿ���,����һ���ӷ���ʧ��
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return taskSize_ < taskQueMaxThreshHold_; }))
	{
		std::cerr << "task queue if full , submit is fail" << std::endl;
		return Result(sp,false);
	};
	//�ύ����
	taskQue_.emplace(sp);
	taskSize_++;
	notEmpty_.notify_all();
	return Result(sp);
};   //�û��ύ����ӿ�



//////////////////////////////////////////////////////�����̺߳���
void ThreadPool::threadFunc()
{
	/*std::cout << "begin threadFunc id:" <<std::this_thread::get_id() << std::endl;
	std::cout << "end threadFunc" << std::endl;*/

	for (;;)
	{
		std::shared_ptr<Task> task;
		{
			//��ȡ��
			std::cout << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;
			std::unique_lock<std::mutex> lock(taskQueMtx_);
			//�ȴ�notempty
			notEmpty_.wait(lock, [&]()->bool {return taskSize_ > 0; });
			//�Ӷ�����ȡһ������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//֪ͨ�����߳�
			if(taskSize_>0)notEmpty_.notify_all();
			std::cout << std::this_thread::get_id() << "���Ի�ȡ����ɹ�" << std::endl;
			notFull_.notify_all();
		}
	//ִ������
		
		if (task != nullptr)
		{
			task->run();
		}
	}

}


////////////////////////////////////////////////////�߳���ض���
Thread::Thread(ThreadFunc func)
	:func_(func)    //��ȡbind���صĺ������󣬼�threadpool�ж�����̺߳���
{}


Thread::~Thread()
{
	   
}

//�����߳�
void Thread::start()
{
	std::thread t(func_);   //�̶߳���t ���̺߳���func_
	t.detach();     //���÷����߳�
}

Result::Result(std::shared_ptr<Task> task , bool isValid )
	:task_(task)
	,isValid_(isValid)
{};

//�����߳����н��
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
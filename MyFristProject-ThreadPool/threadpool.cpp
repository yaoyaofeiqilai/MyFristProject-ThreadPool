#include "threadpool.h"

const int TASK_MAX_THRESHHOLD = 4;
const int THREAD_MAX_THRESHHOLD = 10;
const int THREAD_MAX_IDLE_TIME = 5;  //�߳�������ʱ�䣬��λ��


//////////////////////////////////////////////////////////////////////////////�̳߳ع��캯��
ThreadPool::ThreadPool()
	:initThreadSize_(0)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	,idleThreadSize_(0)
	,isPoolRunning_(false)
	,curThreadSize_(0)
	,threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
{}

//�̳߳���������
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;     //�����߳�״̬
	std::unique_lock<std::mutex> lock(taskQueMtx_);
	notEmpty_.notify_all();     //�������еȴ��߳�
	poolExit_.wait(lock, [&]()->bool {return curThreadSize_ == 0; });    //�ȴ��߳�ȫ������
}

void ThreadPool::start(int initThreadSize)
{
	//�����̳߳�״̬
	isPoolRunning_ = true;
	//��ʼ���߳�����
	initThreadSize_ = initThreadSize;
	curThreadSize_ = initThreadSize;
	//�����߳�
	for (int i = 0; i < initThreadSize; i++)
	{
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		threads_.emplace(ptr->getThreadId(),std::move(ptr));
	}

	//�����߳�
	for (int i = 0; i < initThreadSize_; i++)
	{
		threads_[i]->start();
		idleThreadSize_++;
	}
};//�����̳߳�

void  ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())    //�����������״̬�򲻿����޸�
		return;
	poolMode_ = mode;
};   //�����̳߳�ģʽ

void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	if (checkRunningState())    //�����������״̬�򲻿����޸�
		return;
	taskQueMaxThreshHold_ = threshhold;
};//�������������ֵ

void ThreadPool::setThreadSizeMaxThreshHold(int threshhold)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		threadSizeThreshHold_ = threshhold;
	}
}//����chachedģʽ������߳�����


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

	//���ݿ����߳������������������ж��Ƿ���Ҫ����µ��߳�
	if (poolMode_ == PoolMode::MODE_CACHED &&
		taskSize_ > idleThreadSize_ &&
		curThreadSize_ < threadSizeThreshHold_)
	{
		//�����µ��߳�
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,std::placeholders::_1));
		int threadid = ptr->getThreadId();         //��ȡid
		threads_.emplace(ptr->getThreadId(),std::move(ptr));
		threads_[threadid]->start();   //�����߳�
		curThreadSize_++;           //��ǰ�Լ������̼߳�һ
		idleThreadSize_++;
	}
	return Result(sp);  //����result����
};   //�û��ύ����ӿ�

bool ThreadPool::checkRunningState() const
{
	return isPoolRunning_;   //δ�������и�����ж�
}


//////////////////////////////////////////////////////�����̺߳���
void ThreadPool::threadFunc(int threadid)
{
	
	while(true)
	{
		auto lastTime = std::chrono::high_resolution_clock().now();
		std::shared_ptr<Task> task;
		{
			//��ȡ��
			std::cout << std::this_thread::get_id() << "���Ի�ȡ����" << std::endl;
			std::unique_lock<std::mutex> lock(taskQueMtx_);     //���Ի�ȡ��

				//ÿ�뷵��һ��     ���ֳ�ʱ���أ������������ִ�з���
				while (taskQue_.size() == 0)     //������ֱ��ȡִ������
				{
					if (!isPoolRunning_)     //û����ʱ���ж��̳߳��Ƿ����������ֱ���˳������������߳�
					{
						std::cout << "threadid:" << std::this_thread::get_id() << "�ѻ���" << std::endl;
						curThreadSize_--;
						poolExit_.notify_all();
						return;
					}

					//�̳߳�δ����������ȴ�״̬
					if (poolMode_ == PoolMode::MODE_CACHED)
					{
						//cachedģʽ�£�����ʱ�䳬��60s���߳�,���л���
					    //��ǰʱ��-last�ϴ��߳�ִ�����ʱ��
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))   //��ʱ�˳�
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
		}
			idleThreadSize_--;
			//�Ӷ�����ȡһ������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//֪ͨ�����߳�
			if(taskSize_>0)notEmpty_.notify_all();
			std::cout << std::this_thread::get_id() << "��ȡ����ɹ�" << std::endl;
			notFull_.notify_all();
	    //ִ������
		
		if (task != nullptr)
		{
			task->exec();
		}
		lastTime = std::chrono::high_resolution_clock().now();//������һ��ִ��ʱ��
		idleThreadSize_++;
	}
}


////////////////////////////////////////////////////�߳���ض���

int Thread::generateId_ = 0;

Thread::Thread(ThreadFunc func)
	:func_(func)    //��ȡbind���صĺ������󣬼�threadpool�ж�����̺߳���
	,threadId_(generateId_++)
{}


Thread::~Thread()
{
	   
}

int Thread::getThreadId() const
{
	return threadId_;
}//��ȡ�߳�id

//�����߳�
void Thread::start()
{
	std::thread t(func_,threadId_);   //�̶߳���t ���̺߳���func_
	std::cout << "�߳�id:" << t.get_id() << "�߳��Ѵ���" << std::endl;
	t.detach();     //���÷����߳�
}

Result::Result(std::shared_ptr<Task> task , bool isValid )
	:task_(task)
	,isValid_(isValid)
{
	task_->setVal(this);
};

//�����߳����н��
void Result::setVal(Any any)
{
	any_ = std::move(any);  //��ֵ���ź�������
	sem_.post();
}

Any Result::get()
{
	if (!isValid_)    //��ȡ�̼߳�����
	{
		return "";
	}
	sem_.wait();  //���δ������ɣ��������߳�
	return std::move(any_);
}


void Task::setVal(Result* res)
{
	result_ = res;
}

void Task::exec()
{
	if(result_!=nullptr)      //����result��ֵ�����̵߳����н��
	{
		result_->setVal(run());
	}
}

Task::Task()
	:result_ (nullptr)
{};
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
	Thread(ThreadFunc func);//���캯��
	~Thread();//��������
	void start();   //����һ��std::thread
	int getThreadId() const;//��ȡ�߳�id
private:
	static int generateId_;
	ThreadFunc func_;
	int threadId_;
};

class Any
{
public:
	Any(const Any&) = delete;      //unique�����п�������
	Any& operator=(const Any&) = delete;
	Any() = default;
	Any(Any&& ) = default;
	Any& operator=(Any&&) = default;

	template<typename T>
	Any(T date)
		:sp_(std::make_unique<Derive<T>>(date))   //����ָ��ָ������
	{};
	~Any()=default;
private:
	//����
	class Base
	{
	public: virtual ~Base() = default;
	};

	//����
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
public:
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

//�������
class Task
{
public:
	Task();
	~Task() = default;
	virtual Any run() = 0;
	void setVal(Result* res);
	void exec();
private:
	Result* result_;
};




class Semaphore    //�ź�����ʵ���߳�ͨ��
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


//���񷵻�ֵ����
class Result
{
public:
	Result(std::shared_ptr<Task> task = NULL, bool isvaild = true);
	~Result() = default;

	//�����߳����н��
	void setVal(Any);

	Any get();
private:
	Semaphore sem_;     //�߳�ͨ���ź���
	Any any_;   //�����̷߳Ż�ֵ
	std::shared_ptr<Task> task_;   //��Ӧ������
	std::atomic_bool isValid_;   //�Ƿ��ύ�ɹ�
};

//�̳߳�����
class ThreadPool
{
public:
	ThreadPool();     //�̳߳ع��캯��
	~ThreadPool();     //�̳߳���������

	void start(int initThreadSize=std::thread::hardware_concurrency());//�����̳߳�

	void setMode(PoolMode mode);   //�����̳߳�ģʽ

	void setTaskQueMaxThreshHold(int threshhold); //�������������ֵ

	void setThreadSizeMaxThreshHold(int threshhold); //��������߳�����
	Result submitTask(std::shared_ptr<Task> sp);   //�û��ύ����ӿ�

	//�رտ������캯��
	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	//�����̺߳���
	void threadFunc(int threadid);
	bool checkRunningState() const;   //�ж��̳߳ص�״̬���ڲ����������ṩ�ⲿ�ӿ�
private:
	std::unordered_map<int, std::unique_ptr<Thread>> threads_;  //�߳��б�
	int initThreadSize_;  //��ʼ�߳�����
	std::atomic_int idleThreadSize_; //�����߳�����
	int threadSizeThreshHold_;     //�߳�����������
	std::atomic_int curThreadSize_;  //��ǰ�߳�����

	std::queue<std::shared_ptr<Task>>  taskQue_; //�������
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
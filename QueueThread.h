#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>
#include <functional>

template<typename T>
class QueueThread
{
public:
	QueueThread();
	~QueueThread();

public:
	void RegisterQueueProcessFunc(const std::function<void(T)>& func);
	bool Start();
	bool Start(const std::function<void(T)>& func);
	void Stop();
	void AddQueueData(T data);
	bool IsRunning() const;

private:
	void QueueProcessThread();
	void ProcessQueueData(std::queue<T>& queue);

private:
	std::unique_ptr<std::thread> m_pThread;
	std::condition_variable m_cv;
	std::queue<T> m_queue;
	std::mutex m_mutexThread;
	std::mutex m_mutexFunc;
	std::unique_ptr<std::function<void(T)>> m_pFunc;
	bool m_bExit;
};

template<typename T>
inline QueueThread<T>::QueueThread() :
	m_bExit{ false }
{
}

template<typename T>
inline QueueThread<T>::~QueueThread()
{
	Stop();
}

template<typename T>
inline void QueueThread<T>::RegisterQueueProcessFunc(const std::function<void(T)>& func)
{
	std::lock_guard lock{ m_mutexFunc };
	m_pFunc = std::make_unique<std::function<void(T)>>(func);
}

template<typename T>
inline bool QueueThread<T>::Start()
{
	if (m_pThread != nullptr)
		return true;

	m_pThread = std::make_unique<std::thread>(&QueueThread::QueueProcessThread, this);
	if (m_pThread == nullptr)
	{
		return false;
	}

	return true;
}

template<typename T>
inline bool QueueThread<T>::Start(const std::function<void(T)>& func)
{
	RegisterQueueProcessFunc(func);
	return Start();
}

template<typename T>
inline void QueueThread<T>::Stop()
{
	if (m_pThread == nullptr)
		return;

	{
		std::lock_guard lock{ m_mutexThread };
		m_bExit = true;
	}

	m_cv.notify_all();
	if (m_pThread->joinable())
	{
		m_pThread->join();
	}
}

template<typename T>
inline void QueueThread<T>::AddQueueData(T data)
{
	std::lock_guard lock{ m_mutexThread };
	m_queue.push(std::move(data));
	m_cv.notify_all();
}

template<typename T>
inline bool QueueThread<T>::IsRunning() const
{
	if (m_pThread != nullptr)
		return true;
	else
		return false;
}

template<typename T>
inline void QueueThread<T>::QueueProcessThread()
{
	std::unique_lock lock{ m_mutexThread, std::defer_lock };
	while (!m_bExit)
	{
		lock.lock();
		m_cv.wait(lock, [&]()
		{
			return !m_queue.empty() || m_bExit;
		});

		if (m_bExit)
		{
			ProcessQueueData(queue);
			lock.unlock();
			break;
		}

		std::queue<T> queue;
		queue.swap(m_queue);
		lock.unlock();

		ProcessQueueData(queue);
	}
}

template<typename T>
inline void QueueThread<T>::ProcessQueueData(std::queue<T>& queue)
{
	if (m_pFunc == nullptr)
	{
		return;
	}

	while (!queue.empty())
	{
		T data{ std::move(queue.front()) };
		queue.pop();

		std::lock_guard lock{ m_mutexFunc };
		(*m_pFunc)(std::move(data));
	}
}
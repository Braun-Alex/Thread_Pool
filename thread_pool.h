#ifndef SIMPLE_THREAD_POOL_H
#define SIMPLE_THREAD_POOL_H
#include <vector>
#include <queue>
#include <future>
#include <thread>
#include <memory>
#include <mutex>
class SimpleThreadPool
{
public:
explicit SimpleThreadPool(size_t entered_thread_count)
{
unsigned int hardware_concurrency=std::thread::hardware_concurrency();
if (entered_thread_count==0||entered_thread_count>hardware_concurrency)
    thread_count=hardware_concurrency;
else thread_count=entered_thread_count;
for (unsigned int i=0; i<thread_count; ++i)
    threads.emplace_back(std::thread(performing_function));
}
~SimpleThreadPool()
{
if (!stop) Destroy();
}
SimpleThreadPool(const SimpleThreadPool&)=delete;
SimpleThreadPool(SimpleThreadPool&&)=delete;
SimpleThreadPool &operator=(const SimpleThreadPool&)=delete;
SimpleThreadPool &operator=(SimpleThreadPool&&)=delete;
template<typename Function>
auto Post(Function given_task) -> std::future<decltype(given_task())>
{
    if (stop) return {};
    using return_type=decltype(given_task());
    auto task=std::make_shared<std::packaged_task<return_type()>>(
            [function=std::forward<Function>(given_task)]
            {
                return function();
            });
    std::future<return_type> result=task->get_future();
    {
        std::unique_lock<std::mutex> lock(mutex);
        tasks.emplace([=]()
                      {
                          (*task)();
                      });
    }
    condition.notify_one();
    return result;
}
void Destroy()
{
    if (stop) return;
    {
        std::unique_lock<std::mutex> lock(mutex);
        stop=true;
    }
    condition.notify_all();
    for (std::thread &the_thread:threads) the_thread.join();
    threads.clear();
}
size_t Thread_Count() const
{
return thread_count;
}
private:
size_t thread_count;
std::function<void()> performing_function=[&]()
{
std::function<void()> task;
while (true)
  {
      {
          std::unique_lock<std::mutex> lock(mutex);
          condition.wait(lock,[&]()
             {
             return !tasks.empty()||stop;
             });
          if (tasks.empty()&&stop) return;
          task=std::move(tasks.front());
          tasks.pop();
      }
  task();
  }
};
std::vector<std::thread> threads;
std::queue<std::function<void()>> tasks;
std::condition_variable condition;
std::mutex mutex;
bool stop=false;
};
#endif
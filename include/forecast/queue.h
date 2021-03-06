#pragma once

#include "task.h"

#include <CL/cl.hpp>
#include <condition_variable>
#include <map>
#include <set>
#include <thread>
#include <util.h>

namespace forecast {

using TaskCallback = std::function<void(Task)>;

class Queue {
public:
  Queue(const cl::Context& ctx, cl::Program* program, TaskCallback &&clb)
    : _program(program)
    , _command_queue(cl::CommandQueue(ctx))
    , _clb(std::move(clb))
    , _thread(std::bind(&Queue::queue_loop, this))
  {
  }
  Queue(cl::CommandQueue&& command_queue) = delete;
  Queue() = delete;

  void enqueue(Task &&task) {
    assert(!_finished);
    debug("-> Task {} ({})", task.function_name(), task.id());
    {
      std::lock_guard<std::mutex> lg(_m);
      _tasks.push_back(task);
    }
    _cv.notify_all();
  }

  void set_program(cl::Program* program) {
    std::lock_guard<std::mutex> lg(_m);
    _program = program;
  }

  void wait() {
    std::unique_lock<std::mutex> lk(_m);
    _cv.wait(lk, [this]() { return not _working && _tasks.size() == 0; });
  }

  void finish() {
    {
      std::lock_guard<std::mutex> lg(_m);
      _finished = true;
    }
    _cv.notify_all();
  }

  void queue_loop() {
    while(not _finished) {
      std::unique_lock<std::mutex> lk(_m);
      _cv.wait(lk, [this]() {
        return not _working && (_tasks.size() > 0 || _finished);
      });
      if (_finished && _tasks.size() == 0) {
        return;
      }
      auto& task  = _tasks.front();
      auto& event = pass_to_cl(task);
      _working = true;
      lk.unlock();

      event.wait();


      {
        std::unique_lock<std::mutex> lg(_m);
        auto finished_task = std::move(_tasks.front());
        finished_task.finished_now();
        _tasks.pop_front();
        _working = false;
        lg.unlock();
        _clb(finished_task);
        debug("<- Task {}", task.id());
      }
      _cv.notify_all();
    }
  }

  std::size_t size() const {
    return _tasks.size();
  }

  const Tasks &tasks() const {
    return _tasks;
  }

  ~Queue() {
    finish();
    _thread.join();
  }
private:
  cl::Event& pass_to_cl(Task& task) {
    auto& kernel = task.generate_kernel(*_program);
    auto& kernel_done = task.kernel_done();
    task.enqueued_now();
    cl_ok(_command_queue.enqueueNDRangeKernel(
        kernel,
        task.offset(),
        task.global(),
        task.local(),
        NULL,
        std::addressof(kernel_done)));
    return kernel_done;
  }

private:
  cl::Program*            _program;
  std::condition_variable _cv;
  std::mutex              _m;
  cl::CommandQueue        _command_queue;
  bool                    _finished = false;
  bool                    _working = false;
  Tasks                   _tasks;
  TaskCallback            _clb;
  std::thread             _thread;
};
}

#pragma once

#include <benchmark/benchmark.h>
#include <benchmarks/fixtures.h>
#include <log.h>
#include <util.h>

// Benchmarks for copying data from/to the FPGA
BENCHMARK_DEFINE_F(BasicKernelFixture, Bandwidth)(benchmark::State& state)
{
  using value_t                 = double;
  auto&                ctx      = clstate.ctx;
  auto&                queue    = clstate.queue;
  size_t               buf_size = state.range(0) * 1024 * 1024;
  std::vector<value_t> host_buf(buf_size, value_t{1});
  cl::Buffer buf(ctx, CL_MEM_READ_WRITE, buf_size * sizeof(value_t));

  for (auto _ : state) {
    cl::Event copy_event;
    queue.enqueueWriteBuffer(
        buf,
        CL_TRUE,
        0,
        buf_size * sizeof(value_t),
        host_buf.data(),
        NULL,
        &copy_event);
    copy_event.wait();
  }

  state.SetBytesProcessed(
      size_t(state.iterations()) * buf_size * sizeof(value_t));
}

// Test how well reconfiguration and copying can be overlapped
BENCHMARK_DEFINE_F(BasicKernelFixture, ReconfigureCopyOverlap)
(benchmark::State& state)
{
  using value_t                 = double;
  const size_t         buf_size = state.range(0) * 1024 * 1024;
  auto&                ctx      = clstate.ctx;
  auto&                queue    = clstate.queue;
  std::vector<value_t> host_buf(buf_size);
  cl::Buffer buf(ctx, CL_MEM_READ_WRITE, buf_size * sizeof(value_t));

  auto kernel1 = kernel("empty", "empty");
  auto kernel2 = kernel("multi_empty", "empty1");

  for (auto _ : state) {
    state.PauseTiming();
    cl::CommandQueue other_queue(ctx);
    {
      cl::Event kernel_event;
      queue.enqueueTask(kernel1, NULL, &kernel_event);
      kernel_event.wait();
      state.ResumeTiming();
    }
    {
      cl::Event kernel_event, copy_event;
      std::vector<cl::Event> events;
      queue.enqueueTask(kernel2, NULL, &kernel_event);
      events.push_back(kernel_event);
      // If the two tasks can be overlapped, both events will be finished at
      // max(t(copy), t(kernel2)). This would switch from t(kernel2) to
      // t(copy) when buf gets large enough.
      /* copy_event.wait(); */
      other_queue.enqueueReadBuffer(
          buf,
          CL_FALSE,
          0,
          buf_size * sizeof(value_t),
          host_buf.data(),
          &events,
          &copy_event);
      other_queue.finish();
    }
  }

  state.SetBytesProcessed(
      size_t(state.iterations()) * buf_size * sizeof(value_t));
}

BENCHMARK_REGISTER_F(BasicKernelFixture, Bandwidth)
    ->RangeMultiplier(2)
    ->Range(1, 2048)
    ->Unit(benchmark::kMillisecond);
BENCHMARK_REGISTER_F(BasicKernelFixture, ReconfigureCopyOverlap)
    ->RangeMultiplier(2)
    ->Range(1, 2048)
    ->Unit(benchmark::kMillisecond);

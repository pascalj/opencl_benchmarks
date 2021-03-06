#include <log.h>
#include "spdlog/cfg/argv.h"
#include <util.h>
#include <benchmark/benchmark.h>

#include <benchmarks/copy.h>
#include <benchmarks/performance.h>
#include <benchmarks/reconfigure.h>
#include <benchmarks/forecast.h>
#include <benchmarks/fft.h>

int main(int argc, char **argv) {
  std::srand(std::time(0));
  spdlog::set_pattern("[%H:%M:%S] [%^%L%$] [%t] %v");
  spdlog::cfg::load_argv_levels(argc, argv);
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
}

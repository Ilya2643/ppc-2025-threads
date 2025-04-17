#include "stl/Konstantinov_I_Sort_Batcher/include/ops_stl.hpp"

#include <thread>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace konstantinov_i_sort_batcher_stl {
namespace {
uint64_t DoubleToKey(double d) {
  uint64_t u = 0;
  std::memcpy(&u, &d, sizeof(d));

  if ((u >> 63) != 0) {
    return ~u;
  }
  return u ^ 0x8000000000000000ULL;
}

double KeyToDouble(uint64_t key) {
  if ((key >> 63) != 0) {
    key = key ^ 0x8000000000000000ULL;
  } else {
    key = ~key;
  }
  double d = NAN;
  std::memcpy(&d, &key, sizeof(d));
  return d;
}

void RadixSorted(std::vector<double>& arr) {
  if (arr.empty()) return;

  const size_t n = arr.size();
  const int radix = 256;
  const int thread_count = std::thread::hardware_concurrency();
  const size_t block_size = (n + thread_count - 1) / thread_count;

  std::vector<uint64_t> keys(n);
  std::vector<uint64_t> output_keys(n);

  {
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
      threads.emplace_back([&arr, &keys, t, block_size, n]() {
        size_t begin = t * block_size;
        size_t end = std::min(begin + block_size, n);
        for (size_t i = begin; i < end; ++i) {
          keys[i] = DoubleToKey(arr[i]);
        }
      });
    }
    for (auto& thread : threads) thread.join();
  }

  for (int pass = 0; pass < 8; ++pass) {
    const int shift = pass * 8;

    std::vector<std::vector<size_t>> local_counts(thread_count, std::vector<size_t>(radix, 0));

    {
      std::vector<std::thread> threads;
      for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&keys, &local_counts, t, shift, block_size, n]() {
          size_t begin = t * block_size;
          size_t end = std::min(begin + block_size, n);
          for (size_t i = begin; i < end; ++i) {
            uint8_t byte = static_cast<uint8_t>((keys[i] >> shift) & 0xFF);
            local_counts[t][byte]++;
          }
        });
      }
      for (auto& thread : threads) thread.join();
    }

    std::vector<size_t> count(radix, 0);
    for (int b = 0; b < radix; ++b)
      for (int t = 0; t < thread_count; ++t) count[b] += local_counts[t][b];

    std::vector<size_t> position(radix, 0);
    for (int i = 1; i < radix; ++i) position[i] = position[i - 1] + count[i - 1];

    std::vector<std::vector<size_t>> local_pos(thread_count, std::vector<size_t>(radix));
    for (int b = 0; b < radix; ++b) {
      size_t pos = position[b];
      for (int t = 0; t < thread_count; ++t) {
        local_pos[t][b] = pos;
        pos += local_counts[t][b];
      }
    }

    {
      std::vector<std::thread> threads;
      for (int t = 0; t < thread_count; ++t) {
        threads.emplace_back([&, t, shift]() {
          size_t begin = t * block_size;
          size_t end = std::min(begin + block_size, n);
          std::vector<size_t> pos_copy = local_pos[t];
          for (size_t i = begin; i < end; ++i) {
            uint8_t byte = static_cast<uint8_t>((keys[i] >> shift) & 0xFF);
            output_keys[pos_copy[byte]++] = keys[i];
          }
        });
      }
      for (auto& thread : threads) thread.join();
    }

    keys.swap(output_keys);
  }

  {
    std::vector<std::thread> threads;
    for (int t = 0; t < thread_count; ++t) {
      threads.emplace_back([&arr, &keys, t, block_size, n]() {
        size_t begin = t * block_size;
        size_t end = std::min(begin + block_size, n);
        for (size_t i = begin; i < end; ++i) {
          arr[i] = KeyToDouble(keys[i]);
        }
      });
    }
    for (auto& thread : threads) thread.join();
  }
}

void BatcherOddEvenMerge(std::vector<double>& arr, int low, int high) {
  if (high - low <= 1) {
    return;
  }
  int mid = (low + high) / 2;
  BatcherOddEvenMerge(arr, low, mid);
  BatcherOddEvenMerge(arr, mid, high);

  std::vector<std::thread> threads;
  int thread_count = std::thread::hardware_concurrency();
  int block_size = (mid - low + thread_count - 1) / thread_count;

  for (int t = 0; t < thread_count; ++t) {
    int begin = low + t * block_size;
    int end = std::min(begin + block_size, mid);
    threads.emplace_back([&arr, begin, end, mid, low]() {
      for (int i = begin; i < end; ++i) {
        if (arr[i] > arr[i + mid - low]) {
          std::swap(arr[i], arr[i + mid - low]);
        }
      }
    });
  }
  for (auto& thread : threads) thread.join();
}

void RadixSort(std::vector<double>& arr) {
  RadixSorted(arr);
  BatcherOddEvenMerge(arr, 0, static_cast<int>(arr.size()));
}
}  // namespace
}  // namespace konstantinov_i_sort_batcher_stl

bool konstantinov_i_sort_batcher_stl::RadixSortBatcherSTL::PreProcessingImpl() {
  unsigned int input_size = task_data->inputs_count[0];
  auto* in_ptr = reinterpret_cast<double*>(task_data->inputs[0]);
  mas_ = std::vector<double>(in_ptr, in_ptr + input_size);

  unsigned int output_size = task_data->outputs_count[0];
  output_ = std::vector<double>(output_size, 0);

  return true;
}

bool konstantinov_i_sort_batcher_stl::RadixSortBatcherSTL::ValidationImpl() {
  return task_data->inputs_count[0] == task_data->outputs_count[0];
}

bool konstantinov_i_sort_batcher_stl::RadixSortBatcherSTL::RunImpl() {
  output_ = mas_;
  konstantinov_i_sort_batcher_stl::RadixSort(output_);
  return true;
}

bool konstantinov_i_sort_batcher_stl::RadixSortBatcherSTL::PostProcessingImpl() {
  for (size_t i = 0; i < output_.size(); i++) {
    reinterpret_cast<double*>(task_data->outputs[0])[i] = output_[i];
  }
  return true;
}
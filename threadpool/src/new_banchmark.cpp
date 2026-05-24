// =============================================================================
// benchmark.cpp — CPU-bound and I/O-bound ThreadPool benchmark
//
// Compile:
//   g++ -O2 -std=c++17 -pthread benchmark.cpp -o benchmark
// Run:
//   ./benchmark
// =============================================================================

#include "ThreadPool.hpp"
#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <thread>
#include <string>

using namespace std;
using namespace chrono;


const int TRIALS = 10;   

const vector<int> THREAD_COUNTS = { 1, 4, 8, 16, 32 };

const vector<int> TASK_COUNTS = { 50, 200, 500 };

const vector<microseconds> CPU_DURATIONS = {
    microseconds(100),    // 100 µs
    microseconds(1000),   // 1 ms
    microseconds(10000),  // 10 ms
};

const vector<int> IO_SLEEP_MS = { 10, 50, 100 };

double median(vector<double> v) {
    sort(v.begin(), v.end());
    return v[v.size() / 2];
}

double percentile(vector<double> sorted_v, double p) {
    size_t idx = min((size_t)(p / 100.0 * sorted_v.size()), sorted_v.size() - 1);
    return sorted_v[idx];
}

string us_label(microseconds us) {
    if (us.count() < 1000) return to_string(us.count()) + "us";
    return to_string(us.count() / 1000) + "ms";
}

void print_sep(char c = '-', int w = 100) {
    cout << string(w, c) << "\n";
}

void print_row(int threads, int tasks, const string& dur_label,
               double wall_ms, double tput, double p50, double p95, double p99) {
    cout << fixed << setprecision(2)
         << setw(9)  << left  << threads
         << setw(8)  << left  << tasks
         << setw(10) << left  << dur_label
         << setw(12) << right << wall_ms  << "ms"
         << setw(12) << right << tput     << " t/s"
         << setw(10) << right << p50      << "ms"
         << setw(10) << right << p95      << "ms"
         << setw(10) << right << p99      << "ms"
         << "\n";
}

void print_col_header() {
    cout << left
         << setw(9)  << "Threads"
         << setw(8)  << "Tasks"
         << setw(10) << "Dur"
         << setw(14) << "  WallTime"
         << setw(14) << "  Throughput"
         << setw(12) << "  p50"
         << setw(12) << "  p95"
         << setw(10) << "  p99"
         << "\n";
    print_sep('.');
}
void work_cpu(microseconds dur) {
    auto start = steady_clock::now();
    volatile uint64_t x = 1;
    uint64_t i = 0;
    while (true) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        if (++i % 512 == 0)
            if (steady_clock::now() - start >= dur) break;
    }
}

void work_io(int sleep_ms) {
    this_thread::sleep_for(milliseconds(sleep_ms));
}

struct RunResult { double wall_ms, p50, p95, p99; };

template<typename TaskFn>
RunResult run_config(int num_threads, int num_tasks, TaskFn task_fn) {
    vector<double> trial_walls;
    trial_walls.reserve(TRIALS);

    // We only collect per-task latencies on the last trial to save overhead
    vector<double> last_latencies;

    for (int t = 0; t < TRIALS; ++t) {
        ThreadPool pool(num_threads);
        vector<future<void>> futures;
        futures.reserve(num_tasks);

        vector<steady_clock::time_point> submit_times(num_tasks);

        auto wall_start = steady_clock::now();

        for (int i = 0; i < num_tasks; ++i) {
            submit_times[i] = steady_clock::now();
            futures.push_back(pool.executetasks(task_fn));
        }

        vector<double> latencies(num_tasks);
        for (int i = 0; i < num_tasks; ++i) {
            futures[i].get();
            latencies[i] = duration<double, milli>(
                steady_clock::now() - submit_times[i]).count();
        }

        auto wall_end = steady_clock::now();
        trial_walls.push_back(
            duration<double, milli>(wall_end - wall_start).count());

        if (t == TRIALS - 1)
            last_latencies = move(latencies);
    }

    sort(last_latencies.begin(), last_latencies.end());
    return {
        median(trial_walls),
        percentile(last_latencies, 50),
        percentile(last_latencies, 95),
        percentile(last_latencies, 99)
    };
}



void bench_cpu() {
    cout << "\n";
    print_sep('=');
    cout << "  CPU-BOUND BENCHMARK\n";
    cout << "  Workload: LCG spin loop (pure ALU, no memory traffic, no sleep)\n";
    cout << "  Metric: total wall time (median of " << TRIALS << " trials), per-task p50/p95/p99\n";
    print_sep('=');

    for (int threads : THREAD_COUNTS) {
        cout << "\n  -- threads = " << threads << " --\n";
        print_col_header();

        for (int tasks : TASK_COUNTS) {
            for (auto dur : CPU_DURATIONS) {
                auto r = run_config(threads, tasks,
                    [dur]() { work_cpu(dur); });

                double tput = tasks / (r.wall_ms / 1000.0);
                print_row(threads, tasks, us_label(dur),
                          r.wall_ms, tput, r.p50, r.p95, r.p99);
            }
            cout << "\n";
        }
    }
}

void bench_io() {
    cout << "\n";
    print_sep('=');
    cout << "  I/O-BOUND BENCHMARK\n";
    cout << "  Workload: sleep(N ms) simulating a blocking network/database call\n";
    cout << "  Metric: total wall time (median of " << TRIALS << " trials), per-task p50/p95/p99\n";
    print_sep('=');

    for (int threads : THREAD_COUNTS) {
        cout << "\n  -- threads = " << threads << " --\n";
        print_col_header();

        for (int tasks : TASK_COUNTS) {
            for (int slp : IO_SLEEP_MS) {
                auto r = run_config(threads, tasks,
                    [slp]() { work_io(slp); });

                double tput = tasks / (r.wall_ms / 1000.0);
                string label = to_string(slp) + "ms";
                print_row(threads, tasks, label,
                          r.wall_ms, tput, r.p50, r.p95, r.p99);
            }
            cout << "\n";
        }
    }
}

void bench_cpu_sweep() {
    cout << "\n";
    print_sep('=');
    cout << "  CPU-BOUND — POOL SIZE SWEEP\n";
    cout << "  Fixed: 500 tasks, 1ms each. Vary thread count. Find optimal pool size.\n";
    print_sep('=');
    print_col_header();

    for (int threads : THREAD_COUNTS) {
        auto r = run_config(threads, 500,
            []() { work_cpu(microseconds(1000)); });
        double tput = 500.0 / (r.wall_ms / 1000.0);
        print_row(threads, 500, "1ms", r.wall_ms, tput, r.p50, r.p95, r.p99);
    }
}

void bench_io_sweep() {
    cout << "\n";
    print_sep('=');
    cout << "  I/O-BOUND — POOL SIZE SWEEP\n";
    cout << "  Fixed: 100 tasks, sleep 50ms each. Vary thread count. Find optimal pool size.\n";
    cout << "  Theoretical floor: 50ms (all tasks run concurrently).\n";
    cout << "  Formula: optimal threads = N_cores * (1 + wait/compute)\n";
    print_sep('=');
    print_col_header();

    for (int threads : THREAD_COUNTS) {
        auto r = run_config(threads, 100,
            []() { work_io(50); });
        double tput = 100.0 / (r.wall_ms / 1000.0);
        print_row(threads, 100, "50ms", r.wall_ms, tput, r.p50, r.p95, r.p99);
    }
}

int main() {
    int hw = (int)thread::hardware_concurrency();

    cout << "\n";
    print_sep('=', 100);
    cout << "  THREADPOOL BENCHMARK — CPU-bound and I/O-bound\n";
    cout << "  Hardware threads: " << hw << "\n";
    cout << "  Trials per config: " << TRIALS << " (median reported)\n";
    print_sep('=', 100);

    bench_cpu();
    bench_io();
    bench_cpu_sweep();
    bench_io_sweep();

    cout << "\n";
    print_sep('=', 100);
    cout << "  DONE\n";
    print_sep('=', 100);
    cout << "\n";

    return 0;
}

# Thread-Pool-Architectures And Centralized-Queue-Work-Stealing

##  Components

### 1. ThreadPool
- thread pool for task parallelism
- Best for: Independent tasks, I/O operations, variable workloads
- **Performance**: Up to 8X speedup on 8 cores

### 2. ForkJoinPool : Workstealing
- Divide-and-conquer parallel execution framework
- Best for: Recursive algorithms, merge sort, tree operations
- **Performance**: Up to 2X speedup on recursive workloads

##  Quick Comparison

| Feature | ThreadPool | ForkJoin |
|---------|-----------|----------|
| Use Case | Independent tasks | Recursive divide-and-conquer |
| Overhead | Low | Medium (task spawning) |
| Best For | I/O, variable tasks | CPU-intensive recursion |
| Examples | Image processing | Merge sort, tree traversal |
 # fork join pool with work stealing:
 benifits of having own deque per thread:
 instead of having a centralised work queue each thread has its own tasks queue 

## When to Use What?
**Use ThreadPool when:**
- Tasks are independent
- Variable execution times
- I/O-bound operations

**Use ForkJoin when:**
- Recursive algorithms
- Divide-and-conquer problems


# performance measure:
## threadpool:
## fork join threadpool

## Requirements

- C++17 or later
- pthread support

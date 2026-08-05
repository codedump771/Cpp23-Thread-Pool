# C++23 Thread Pool #

This is a very minimal thread pool implementation I made to teach myself threading with modern C++.

## Usage ##

The code should be compatible with any compiler/platform, as it uses standard C++ code. You simply need to 
`#include "thread_pool.hpp"` in your cpp file and you should be fine.

## Example ##

```C++
#include "thread_pool.hpp"
#include <print>
#include <vector>
#include <future>

int times_two(int x)
{
    return x + x;
}

void greet(const std::string& name) {
    std::println("Hi, {}!", name);
}

int main() {
    // Initialize the pool with 5 threads. 
    // Leaving the constructor blank will use the maximum amount of threads your hardware allows.    
    ThreadPool pool{5};
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 10; ++i) {
        // Enqueue a void function
        pool.enqueue_void(greet, "Bob");
    }

    pool.wait(); // Wait for all tasks to be completed.

    for (int i = 0; i < 10; ++i) {
        // Enqueue a function and return a future with the result.
        futures.push_back(pool.enqueue(times_two, i));
    }
    for (auto& future : futures) {
        std::println("{}", future.get());
    }
    
    pool.shutdown(); // Wait for all tasks to be completed and shutdown the pool.
}
```

## Output ##
```
Hi, Bob!
Hi, Bob!
Hi, Bob!
Hi, Bob!
Hi, Bob!
Hi, Bob!
Hi, Bob!
Hi, Bob!
Hi, Bob!
Hi, Bob!
0
2
4
6
8
10
12
14
16
18
```
# nova

nova is a header-only C++14 job system for Windows. It spins up a thread pool which you can push function invocations to syncronously, asynchronously, or semi-synchronously.

### Getting started

Using the system is easy: download all three files and stick them in your project directory (and add them to your project if you need to), `#include nova.h`, and use all the stuff in the `nova` namespace.

#### Synchronous usage

Here's a sample program to get you started:

```C++
#include <iostream>
#include "nova.h"

void NextJob() {
  std::cout << "Hello from NextJob" << std::endl;
}

void JobWithParam(int number) {
  std::cout << "Hello from NextJob, with param: " << std::to_string(number) << std::endl;
}

void InitialJob() {
  nova::call(&NextJob, nova::bind(&JobWithParam, 5));
}

int main() {
  nova::start_sync(&InitialJob);
}
```

There are a few things going on here.

First, the program starts with a call to `nova::start_sync`. `nova::start_sync`, and its counterpart `nova::start_async`, initialize the job system, creating the thread pool, performing some miscellaneous tasks, and entering the first job, represented here by `InitialJob`. The first job can be any **callable object**, meaning any object that can be called like a function (i.e. `callableObj()`). The function pointer `&InitialJob` satisfies this requirement, as would a member function pointer, a lambda, a `std::function`, etc. If the callable object has parameters you can add them after the **callable object** (e.g. `nova::start_sync(&InitialJob, true, 5, &objOfTypeFoo);` if `InitialJob` took a `bool`, an `int`, and a `Foo*`).

After the call to `start_sync` `InitialJob` will run, and we reach the call to `nova::call`. `nova::call` takes any number of **callable objects** that can be called with _no parameters_, pushes them to the thread pool, and returns when they've all finished. But what if your **callable objects** have parameters? `nova::bind` takes a **callable object** and all its parameters and returns a wrapper that allows it to be called with an empty call operator. It's essentially the same as `std::bind`, but it returns a `nova::function` rather than a `std::function`. You can use `std::bind` instead if you like, it makes no difference.

After `nova::call` is called `NextJob` and `JobWithParam` should run simultaneously. Once both have returned `nova::call` will return, `InitialJob` will return, then the job system will shut down and `nova::start_sync` will return.

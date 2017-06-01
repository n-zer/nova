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

The program starts with a call to `nova::start_sync` which initializes the job system, enters the first job, represented here by `InitialJob`, and returns when that job finishes. The first job can be any **callable** object, meaning any object that can be called like a function (e.g. function pointers, member function pointers, lambdas, classes that define `operator()`, etc.), and if it needs any parameters you can add them like so:

```C++
void InitialJob(int number, Foo foo) { ... }

int main() {
  nova::start_sync(&InitialJob, 5, Foo());
}
```

After entering `InitialJob` we reach the call to `nova::call`, which takes one or more **runnable** objects (**callable** objects that can be called with no parameters), runs them in parallel, and returns when they've all finished. If you need to you can make a **runnable** object from a **callable** object and its parameters with `nova::bind` (or `std::bind`, though there are some reasons to prefer `nova::bind` that I'll cover later).

Once `NextJob` and `JobWithParam` return `nova::call` will return, then `InitialJob` will return and the job system will shutdown, then `nova::start_sync` will return and the program will end.

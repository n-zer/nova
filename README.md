# nova

nova is a header-only C++14 job system for Windows. It spins up a thread pool which you can push function invocations to syncronously, asynchronously, or semi-synchronously.

### Getting started

Using the system is easy: download the headers, `#include nova.h`, and use all the stuff in the `nova` namespace.

#### Synchronous usage

Here's a sample program to get you started. It starts the job system, runs `NextJob` and `JobWithParam` in parallel, then exits.

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

We start with a call to `nova::start_sync`, which initializes the job system, enters the first job, represented here by `InitialJob`, and returns when that job finishes. The first job can be any **callable** object (e.g. function pointers, member function pointers, lambdas, classes that define `operator()`, etc.), and if it needs any parameters you can add them like so:

```C++
void InitialJob(int number, Foo foo) { ... }

int main() {
  nova::start_sync(&InitialJob, 5, Foo());
}
```

After entering `InitialJob` we reach the call to `nova::call`, which takes one or more **runnable** objects (**callable** objects that can be called with no parameters), runs them in parallel, and returns* when they've all finished. You can use `nova::bind` to get a **runnable** wrapper for a **callable** object and its parameters (or `std::bind`, though there are some reasons to prefer `nova::bind` that I'll cover later).

Once `NextJob` and `JobWithParam` return `nova::call` will return, then `InitialJob` will return, job system will shutdown, `nova::start_sync` will return, and the program will end.

\* *Note that `nova::call` will not necessarily return to the same thread it was called from*

#### Asynchronous usage

Let's rewrite the sample program to work asynchronously:

```C++
#include <iostream>
#include "nova.h"

void NextJob(nova::dependency_token dt) {
  std::cout << "Hello from NextJob" << std::endl;
}

void JobWithParam(int number, nova::dependency_token dt) {
  std::cout << "Hello from NextJob, with param: " << std::to_string(number) << std::endl;
}

void InitialJob() {
  nova::dependency_token dt(&nova::kill_all_workers);
  nova::push(nova::bind(&NextJob, dt), nova::bind(&JobWithParam, 5, dt));
}

int main() {
  nova::start_async(&InitialJob);
}
```

Here we start with `nova::start_async`, which doesn't return until `nova::kill_all_workers` is called elsewhere in the program. This allows us to use `nova::push`, which returns immediately, rather than `nova::call`.

If we had changed only those two calls, the program would run `NextJob` and `JobWithParam` in parallel then continue to run indefinitely in an idle state. To get `nova::kill_all_workers` to run once both `NextJob` and `JobWithParam` have finished we need to use a `dependency_token`, which takes a **runnable** object and calls it once all copies of the token are destroyed.

Because `InitialJob`, `NextJob`, and `JobWithParam` all have a copy of `dt`, `nova::kill_all_workers` will only run once all three of those functions have returned, at which point `nova::start_async` will also return and the program will end.

Despite the syntax being heavier, asynchronous invocations are much more flexible than synchronous invocations; any dependency graph can be implemented with `nova::push` and `nova::dependency_token`s.

#### Semi-synchronous usage

We can rewrite the previous example in a way that uses both a synchronous start *and* an asynchronous invocation, and doesn't need any `dependency_token`s:

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
  nova::push_dependent(&NextJob, nova::bind(&JobWithParam, 5));
}

int main() {
  nova::start_sync(&InitialJob);
}
```

`nova::push_dependent` is asynchronous and will return immediately, but will extend a current synchronous invocation to its invokees. That is to say, because `InitialJob` was called synchronously by `nova::start_sync`, `nova::start_sync` will not return until `InitialJob`, `NextJob`, and `JobWithParam` have all returned.

`nova::call` is also synchronous, so the behavior would be the same if `main` was changed to the following:

```C++
int main() {
  nova::start_sync([]() {
    nova::call(&InitialJob);
  });
}
```

Semi-synchronous invocations are more expensive than asynchronous invocations when they actually extend a synchronous invocation (and negligibly more so when they don't), but they allow dependency graphs implemented with asynchronous invocations to be invoked synchronously.

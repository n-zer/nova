# nova

nova is a header-only C++14 job system for Windows. It spins up a thread pool which you can push function invocations to syncronously, asynchronously, or semi-synchronously.

## Table of contents
* [Getting started](#getting-started)
* [Synchronous usage](#synchronous-usage)
	* [`start_sync`](#synchronous-usage)
	* [`call`](#synchronous-usage)
	* [`bind`](#synchronous-usage)
* [Asynchronous usage](#asynchronous-usage)
	* [`start_async`](#asynchronous-usage)
	* [`push`](#asynchronous-usage)
	* [`dependency_token`](#asynchronous-usage)
* [Semi-synchronous usage](#semi-synchronous-usage)
	* [`push_dependent`](#semi-synchronous-usage)
* [Batching](#batching)
	* [`bind_batch`](#batching)
	* [`parallel_for`](#batching)
* [Main thread invocation](#main-thread-invocation)
	* [`switch_to_main`](#main-thread-invocation)
* [API reference](https://github.com/narrill/nova/wiki/API-reference)

## Getting started

Using the system is easy: download the headers, `#include nova.h`, and use all the stuff in the `nova` namespace.

## Synchronous usage
#### [`start_sync`](https://github.com/narrill/nova/wiki/API-reference#novastart_sync), [`call`](https://github.com/narrill/nova/wiki/API-reference#novacall), [`bind`](https://github.com/narrill/nova/wiki/API-reference#novabind) <sub>API reference</sub>

Here's a sample program to get you started. It starts the job system, runs `NextJob` and `JobWithParam` in parallel, then exits.

```C++
#include <iostream>
#include <string>
#include "nova.h"

void NextJob() {
	std::cout << "Hello from NextJob" << std::endl;
}

void JobWithParam(int number) {
	std::cout << "Hello from JobWithParam, with param: " << std::to_string(number) << std::endl;
}

void InitialJob() {
	nova::call(NextJob, nova::bind(JobWithParam, 5));
}

int main() {
	nova::start_sync(InitialJob);
}
```

We start with a call to `nova::start_sync`, which initializes the job system, enters the first job (represented here by `InitialJob`), and returns when that job finishes. The first job can be any **callable** object (e.g. function pointers, member function pointers, lambdas, classes that define `operator()`, etc.), and if it needs any parameters you can add them like so:

```C++
void InitialJob(int number, Foo foo) { ... }

int main() {
	nova::start_sync(InitialJob, 5, Foo());
}
```

After entering `InitialJob` we reach the call to `nova::call`, which takes one or more **runnable** objects (**callable** objects that can be called with no parameters), runs them in parallel, and returns[\*](#invisible-anchor) when they've all finished. You can use `nova::bind` (or`std::bind`) to get a **runnable** wrapper for a **callable** object and its parameters\*\*.

Once `NextJob` and `JobWithParam` return `nova::call` will return, then `InitialJob` will return, the job system will shutdown, `nova::start_sync` will return, and the program will end.

<br />

<a id="user-content-invisible-anchor"></a>\* *`nova::call` will not necessarily return to the same thread it was called from.*

\*\* *By default, both `nova::bind` and `std::bind` will pass references to copies to a **callable** that expects references. If you want a true reference you need to use `std::ref` or `std::cref`:*

```C++
void TestFunc(int& n){ n++; }

int test = 0;

nova::bind(TestFunc, test)(); // TestFunc is passed a reference to a copy of test.
std::bind(TestFunc, test)();

// test is still 0.

nova::bind(TestFunc, std::ref(test))(); // TestFunc is passed a reference to test.
std::bind(TestFunc, std::ref(test))();

// test is now 2.
```

## Asynchronous usage
#### [`start_async`](https://github.com/narrill/nova/wiki/API-reference#novastart_async), [`push`](https://github.com/narrill/nova/wiki/API-reference#novapush), [`dependency_token`](https://github.com/narrill/nova/wiki/API-reference#novadependency_token) <sub>API reference</sub>

Let's rewrite the sample program to work asynchronously:

```C++
#include <iostream>
#include <string>
#include "nova.h"

void NextJob(nova::dependency_token dt) {
	std::cout << "Hello from NextJob" << std::endl;
}

void JobWithParam(int number, nova::dependency_token dt) {
	std::cout << "Hello from JobWithParam, with param: " << std::to_string(number) << std::endl;
}

void InitialJob() {
	nova::dependency_token dt(nova::kill_all_workers);
	nova::push(nova::bind(NextJob, dt), nova::bind(JobWithParam, 5, dt));
}

int main() {
	nova::start_async(InitialJob);
}
```

Here we start with `nova::start_async`, which doesn't return until `nova::kill_all_workers` is called elsewhere in the program. This allows us to use `nova::push`, which returns immediately, rather than `nova::call`.

If we had changed only those two calls, the program would run `NextJob` and `JobWithParam` in parallel then continue to run indefinitely in an idle state. To get `nova::kill_all_workers` to run once both `NextJob` and `JobWithParam` have finished we need to use a `nova::dependency_token`, which takes a **runnable** object and calls it once all copies of the token are destroyed.

Because `InitialJob`, `NextJob`, and `JobWithParam` all have a copy of `dt`, `nova::kill_all_workers` will only run once all three of those functions have returned, at which point `nova::start_async` will also return and the program will end.

Despite the syntax being heavier, asynchronous invocations are much more flexible than synchronous invocations; any dependency graph can be implemented with `nova::push` and `nova::dependency_token`s.

## Semi-synchronous usage
#### [`push_dependent`](https://github.com/narrill/nova/wiki/API-reference#novapush_dependent) <sub>API reference</sub>

We can rewrite the previous example in a way that uses both a synchronous start *and* an asynchronous invocation, and doesn't need any `nova::dependency_token`s:

```C++
#include <iostream>
#include <string>
#include "nova.h"

void NextJob() {
	std::cout << "Hello from NextJob" << std::endl;
}

void JobWithParam(int number) {
	std::cout << "Hello from JobWithParam, with param: " << std::to_string(number) << std::endl;
}

void InitialJob() {
	nova::push_dependent(NextJob, nova::bind(JobWithParam, 5));
}

int main() {
	nova::start_sync(InitialJob);
}
```

`nova::push_dependent` is asynchronous and will return immediately, but will extend a current synchronous invocation to its invokees. That is to say, because `InitialJob` was called synchronously by `nova::start_sync`, `nova::start_sync` will not return until `InitialJob`, `NextJob`, and `JobWithParam` have all returned.

`nova::call` is also synchronous, so the behavior would be the same if `main` was changed to the following:

```C++
int main() {
	nova::start_sync([]() {
		nova::call(InitialJob);
	});
}
```

Semi-synchronous invocations are more expensive than asynchronous invocations when they actually extend a synchronous invocation (and negligibly more so when they don't), but they allow dependency graphs implemented with asynchronous invocations to be invoked synchronously.

## Batching
#### [`bind_batch`](https://github.com/narrill/nova/wiki/API-reference#novabind_batch), [`parallel_for`](https://github.com/narrill/nova/wiki/API-reference#novaparallel_for) <sub>API reference</sub>

`nova::bind_batch` allows you to take a **callable** object that takes a numerical range as two of its parameters* and turn it into a **batch runnable**. Rather than being invoked as a single job, **batch runnables** are invoked as a set of jobs (one per thread), with each one receiving a contiguous portion of the original range.

For example, if this code was run on a machine with eight logical cores

```C++
void BatchJob(unsigned start, unsigned end){ ... }

nova::push(nova::bind_batch(BatchJob, 0, 8000));
```

it would be functionally equivalent to the following:

```C++
void BatchJob(unsigned start, unsigned end){ ... }

nova::push(
	nova::bind(BatchJob, 0, 1000),
	nova::bind(BatchJob, 1000, 2000),
	nova::bind(BatchJob, 2000, 3000),
	nova::bind(BatchJob, 3000, 4000),
	nova::bind(BatchJob, 4000, 5000),
	nova::bind(BatchJob, 5000, 6000),
	nova::bind(BatchJob, 6000, 7000),
	nova::bind(BatchJob, 7000, 8000)
);
```

This will work with `nova::call` and `nova::push_dependent` as well, and it will work with any combination of **batch** and non-**batch runnables**. The following are all valid:

```C++
nova::push(
	nova::bind_batch(BatchJob, 0, 8000),
	nova::bind(OtherJob)
);
nova::call(
	nova::bind(OtherJob),
	nova::bind_batch(BatchJob, 0, 8000),
	nova::bind_batch(OtherBatchJob, 0, 100, Foo())
);
nova::push_dependent(
	nova::bind(OtherJob),
	nova::bind(BatchJob, 0, 8000),
	nova::bind_batch(OtherBatchJob, 0, 100, Foo())
);
etc.
```

Batching is leveraged to create an efficient `parallel_for` implementation that is performant both with expensive operations on a small amount of data and with cheap operations on a large amount of data. The following call

```C++
nova::parallel_for([](unsigned index) {
	// do something
	...
}, 0, 1000);
```
will call the lambda 1000 times, but will only create as many jobs as the system can run concurrently.

However, if you can process multiple elements at once (e.g. SIMD) it may be more performant to use a batch function directly.

<br />

\* *`nova::bind_batch` assumes the parameters denoting the range are sequential (i.e. `..., start, end, ...`), and it assumes that `start` is the first parameter to satisfy `std::is_integral`:*

```C++
// Correct, uses start as the start and end as the end.
void CorrectBatchSignature(Foo foo, int start, unsigned end, char c);

// Doesn't compile (unless Foo can convert to std::size_t)
// Uses start as the start and foo as the end.
void IncorrectBatchSignature(int start, Foo foo, long end, char c);

// Compiles, but almost certainly incorrect.
// Uses c as the start and start as the end
void VeryIncorrectBatchSignature(Foo foo, char c, int start, unsigned end);
```

## Main thread invocation
#### [`switch_to_main`](https://github.com/narrill/nova/wiki/API-reference#novaswitch_to_main) <sub>API reference</sub>

`nova::call`, `nova::push`, and `nova::push_dependent` all have a template parameter that controls whether their invocations will happen on the main thread, and `nova::call` has a second template parameter that controls whether the call should return on the main thread:

```C++
// These will run their invokees on the main thread.
nova::call<true>(...);
nova::push<true>(...);
nova::push_dependent<true>(...);

// These may run their invokees on any thread.
nova::call<false>(...);
nova::push<false>(...);
nova::push_dependent<false>(...);

// These will always return to the main thread.
nova::call<true, true>(...);
nova::call<false, true>(...);

// These may return to any thread (which may be different from the thread they were called on).
nova::call<true, false>(...);
nova::call<false, false>(...);
```

`nova::switch_to_main` allows you to move to the main thread at any time:

```C++
... // We're on an arbitrary worker thread, main or otherwise.

nova::switch_to_main();

... // Now we're on the main thread.
```

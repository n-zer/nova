#pragma once

#ifndef NOVA_H
#define NOVA_H

#include <vector>
#include <array>
#include <thread>
#include <mutex>
#include <tuple>
#include <Windows.h>
#include <memory>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <atomic>

#include "concurrentqueue.h"

#define NOVA_CACHE_LINE_BYTES 64
#define NOVA_SPIN_COUNT 10000

namespace nova {

#pragma region helpers

	namespace impl {

		template<typename T>
		struct is_shared {
			static const bool value = false;
		};

		template<typename T>
		struct is_shared<std::shared_ptr<T>> {
			static const bool value = true;
		};

		template<typename T>
		void to_new_loc(void* loc, T&& obj) {
			new (loc) T(std::forward<T>(obj));
		}

		template<typename Runnable, std::enable_if_t<!is_shared<Runnable>::value, int> = 0>
		static void run_runnable(Runnable* runnable) {
			(*runnable)();
		}

		template<typename Runnable, std::enable_if_t<is_shared<Runnable>::value, int> = 0>
		static void run_runnable(Runnable* runnable) {
			(*runnable->get())();
		}

		template<typename T>
		constexpr std::size_t ceil(T num)
		{
			return (static_cast<T>(static_cast<std::size_t>(num)) == num)
				? static_cast<std::size_t>(num)
				: static_cast<std::size_t>(num) + ((num > 0) ? 1 : 0);
		}

		template<typename T>
		class raii_ptr {
		public:
			raii_ptr(std::decay_t<T> *& ptr, std::decay_t<T> * val)
				: m_ptr(ptr) {
				m_ptr = val;
			}
			~raii_ptr() {
				m_ptr = nullptr;
			}
		private:
			T *& m_ptr;
		};

		namespace detail {
			template <class F, class... Args>
			inline auto INVOKE(F&& f, Args&&... args) ->
				decltype(std::forward<F>(f)(std::forward<Args>(args)...)) {
				return std::forward<F>(f)(std::forward<Args>(args)...);
			}

			template <class Base, class T, class Derived>
			inline auto INVOKE(T Base::*pmd, Derived&& ref) ->
				decltype(std::forward<Derived>(ref).*pmd) {
				return std::forward<Derived>(ref).*pmd;
			}

			template <class PMD, class Pointer>
			inline auto INVOKE(PMD pmd, Pointer&& ptr) ->
				decltype((*std::forward<Pointer>(ptr)).*pmd) {
				return (*std::forward<Pointer>(ptr)).*pmd;
			}

			template <class Base, class T, class Derived, class... Args>
			inline auto INVOKE(T Base::*pmf, Derived&& ref, Args&&... args) ->
				decltype((std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...)) {
				return (std::forward<Derived>(ref).*pmf)(std::forward<Args>(args)...);
			}

			template <class PMF, class Pointer, class... Args>
			inline auto INVOKE(PMF pmf, Pointer&& ptr, Args&&... args) ->
				decltype(((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...)) {
				return ((*std::forward<Pointer>(ptr)).*pmf)(std::forward<Args>(args)...);
			}
		} // namespace detail

		template< class F, class... ArgTypes>
		decltype(auto) invoke(F&& f, ArgTypes&&... args) {
			return detail::INVOKE(std::forward<F>(f), std::forward<ArgTypes>(args)...);
		}

		namespace detail {
			template <class F, class Tuple, std::size_t... I>
			constexpr decltype(auto) apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>) {
				return impl::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...);
			}
		}  // namespace detail

		template <class F, class Tuple>
		constexpr decltype(auto) apply(F &&f, Tuple &&t) {
			return detail::apply_impl(
				std::forward<F>(f), std::forward<Tuple>(t),
				std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>{});
		}

		template<class Tuple>
		struct integral_index;

		template<bool Val, class Tuple>
		struct integral_index_inner;

		template<class T, class ... Types>
		struct integral_index<std::tuple<T, Types...>> {
			static const std::size_t value = integral_index_inner<std::is_integral<T>::value, std::tuple<T, Types...>>::value;
		};

		template<typename Tuple>
		struct integral_index_inner<true, Tuple> {
			static const std::size_t value = 0;
		};

		template<typename T, typename ... Types>
		struct integral_index_inner<false, std::tuple<T, Types...>> {
			static const std::size_t value = 1 + integral_index<std::tuple<Types...>>::value;
		};
	}

#pragma endregion

#pragma region job & dependency_token

	// Takes a Runnable and invokes it when all copies of the token are released or destroyed.
	class dependency_token {
	public:
		dependency_token() {}

		dependency_token(const dependency_token &) = default;
		dependency_token& operator=(const dependency_token&) = default;

		dependency_token(dependency_token &&) = default;
		dependency_token& operator=(dependency_token&&) = default;

		template<typename Runnable, 
			std::enable_if_t<!std::is_same<std::decay_t<Runnable>, dependency_token>::value, int> = 0>
		dependency_token(Runnable&& runnable);

		void Release() {
			m_token.reset();
		}
	private:
		struct shared_token;
		std::shared_ptr<shared_token> m_token;
	};

	namespace impl{

		class alignas(NOVA_CACHE_LINE_BYTES) job {
		private:
			static const std::size_t paddingSize = NOVA_CACHE_LINE_BYTES - sizeof(dependency_token);

			class job_base {
			public:
				virtual ~job_base() {}
				virtual void move_to(void*) = 0;
				virtual void operator()() = 0;
			};

			class job_empty : job_base {
			public:
				virtual void move_to(void* loc) {}
				virtual void operator()() {}
			};

			template<typename T>
			class job_derived : job_empty {
			public:
				job_derived(T& t)
					: runnable(new T(t)) {
				}
				job_derived(T&& t)
					: runnable(new std::decay_t<T>(std::forward<T>(t))) {
				}

				virtual ~job_derived() {
					delete runnable;
				}

				job_derived(const job_derived& other) = delete;
				job_derived& operator=(const job_derived&) = delete;

				job_derived(job_derived&& other)
					: runnable(other.runnable) {
					other.runnable = nullptr;
				}

				virtual void move_to(void* loc) {
					to_new_loc(loc, std::move(*this));
				}
				virtual void operator()() {
					run_runnable(runnable);
				}
			private:
				T* runnable;
			};

			template<typename T>
			class job_derived_smo : job_empty {
			public:
				job_derived_smo(T& t)
					: runnable(t) {
				}
				job_derived_smo(T&& t)
					: runnable(std::forward<T>(t)) {
				}

				job_derived_smo(const job_derived_smo& other) = delete;
				job_derived_smo& operator=(const job_derived_smo&) = delete;

				job_derived_smo(job_derived_smo&& other) 
					: runnable(std::move(other.runnable)) {
				}

				virtual void move_to(void* loc) {
					to_new_loc(loc, std::move(*this));
				}
				virtual void operator()() {
					run_runnable(&runnable);
				}
			private:
				T runnable;
			};

			template<typename T>
			class job_derived_no_own : job_empty {
			public:
				job_derived_no_own(T* t)
					: runnable(t) {
				}

				virtual ~job_derived_no_own() = default;

				job_derived_no_own(const job_derived_no_own& other) = delete;
				job_derived_no_own& operator=(const job_derived_no_own&) = delete;

				job_derived_no_own(job_derived_no_own&& other) {
					runnable = other.runnable;
					other.runnable = nullptr;
				}

				virtual void move_to(void* loc) {
					to_new_loc(loc, std::move(*this));
				}
				virtual void operator()() {
					run_runnable(runnable);
				}
			private:
				T* runnable;
			};
		public:
			job() {
				new (padding) job_empty();
			}

			~job() {
				get_runnable_base()->~job_base();
			}

			job(const job &) = delete;
			job& operator=(const job &) = delete;

			job(job && other) noexcept {
				other.get_runnable_base()->move_to(padding);
				set_dependency_token(std::move(other.get_dependency_token()));
			}
			job& operator=(job && other) noexcept {
				get_runnable_base()->~job_base();
				other.get_runnable_base()->move_to(padding);
				set_dependency_token(std::move(other.get_dependency_token()));
				return *this;
			}

			template<typename Runnable, std::enable_if_t<!(alignof(Runnable) <= NOVA_CACHE_LINE_BYTES && sizeof(job_derived_smo<std::decay_t<Runnable>>) <= paddingSize), int> = 0>
			job(Runnable&& runnable) {
				new (padding) job_derived<std::decay_t<Runnable>>(std::forward<Runnable>(runnable));
			}

			template<typename Runnable, std::enable_if_t<alignof(Runnable) <= NOVA_CACHE_LINE_BYTES && sizeof(job_derived_smo<std::decay_t<Runnable>>) <= paddingSize, int> = 0>
			job(Runnable&& runnable) {
				new (padding) job_derived_smo<std::decay_t<Runnable>>(std::forward<Runnable>(runnable));
			}

			template<typename Runnable>
			job(Runnable* runnable) {
				new (padding) job_derived_no_own<Runnable>(runnable);
			}

			void operator()() {
				(*get_runnable_base())();
			}

			dependency_token& get_dependency_token() {
				return m_dt;
			}

			void set_dependency_token(dependency_token& dt) {
				m_dt = dt;
			}

			void set_dependency_token(dependency_token&& dt) {
				m_dt = std::forward<dependency_token>(dt);
			}
		private:
			job_base* get_runnable_base() {
				return reinterpret_cast<job_base*>(padding);
			}

			char padding[paddingSize];
			dependency_token m_dt;
		};
	}

	struct dependency_token::shared_token {
		shared_token(impl::job & e)
			: m_job(std::move(e)) {
		}

		template <typename Runnable>
		shared_token(Runnable&& runnable)
			: m_job(std::forward<Runnable>(runnable)) {
		}

		~shared_token() {
			m_job();
		}

		impl::job m_job;
	};

	template<typename Runnable, std::enable_if_t<!std::is_same<std::decay_t<Runnable>, dependency_token>::value, int>>
	dependency_token::dependency_token(Runnable&& runnable)
		: m_token(std::make_shared<shared_token>(std::forward<Runnable>(runnable))) {
	}

#pragma endregion

#pragma region resources

	namespace impl {
		class resources {
		public:
			// Meyers singletons			
			static std::vector<LPVOID>& available_fibers() {
				static thread_local std::vector<LPVOID> af;
				return af;
			}
			static dependency_token *& call_token() {
				static thread_local dependency_token * ct;
				return ct;
			}
			static dependency_token *& dependent_token() {
				static thread_local dependency_token * se;
				return se;
			}			
		};
	}

#pragma endregion

#pragma region queue_wrapper

	namespace impl{
		class critical_lock {
		public:
			critical_lock(CRITICAL_SECTION& cs) : m_cs(cs) {
				EnterCriticalSection(&m_cs);
			}

			~critical_lock() {
				LeaveCriticalSection(&m_cs);
			}

		private:
			critical_lock(const critical_lock&) = delete;

		private:
			CRITICAL_SECTION & m_cs;
		};

		struct critical_wrapper {
			critical_wrapper() {
				InitializeCriticalSection(&cs);
			}

			~critical_wrapper() {
				DeleteCriticalSection(&cs);
			}

			CRITICAL_SECTION cs;
		}; 

		struct condition_wrapper {
			condition_wrapper() {
				InitializeConditionVariable(&cv);
			}

			CONDITION_VARIABLE cv;
		};

		typedef ::nova::impl::job queue_item_t;
		
		class moodycamel_adaptor {
		public:
			struct queue_data {
				::moodycamel::ConsumerToken ct;
				::moodycamel::ProducerToken pt;
			};

			bool pop(queue_data& qd, queue_item_t& item) {
				return m_queue.try_dequeue(qd.ct, item);
			}

			void push(queue_data& qd, queue_item_t&& item) {
				m_queue.enqueue(qd.pt, std::forward<queue_item_t>(item));
			}

			template<std::size_t N>
			void push(queue_data& qd, std::array<queue_item_t, N> && items) {
				m_queue.enqueue_bulk(qd.pt, std::make_move_iterator(std::begin(items)), items.size());
			}

			void push(queue_data& qd, std::vector<queue_item_t> && items) {
				m_queue.enqueue_bulk(qd.pt, std::make_move_iterator(std::begin(items)), items.size());
			}

			queue_data make_queue_data() {
				return { ::moodycamel::ConsumerToken(m_queue), ::moodycamel::ProducerToken(m_queue) };
			}
		private:
			::moodycamel::ConcurrentQueue<queue_item_t> m_queue;
		};

		class queue_wrapper {
		public:
			struct thread_data {
				moodycamel_adaptor::queue_data globalData;
				moodycamel_adaptor::queue_data mainData;
			};

			queue_wrapper(const queue_wrapper& other) = delete;
			queue_wrapper& operator=(const queue_wrapper& other) = delete;
			queue_wrapper(queue_wrapper&& other) = delete;
			queue_wrapper& operator=(queue_wrapper&& other) = delete;

			static thread_data *& current_thread_data() {
				static thread_local thread_data * td;
				return td;
			}

			static queue_wrapper& instance() {
				static nova::impl::queue_wrapper qw;
				return qw;
			}

			void pop(queue_item_t& item) {
				unsigned counter = 0;
				while (!m_globalQueue.pop(current_thread_data()->globalData, item)) {
					if (counter++ > NOVA_SPIN_COUNT) {
						counter = 0;
						SleepConditionVariableCS(&global_condition_variable(), &dummy_critical_section(), INFINITE);
					}
				}
			}

			void pop_main(queue_item_t& item) {
				unsigned counter = 0;
				while (!try_pop_main_queue(item) && !m_globalQueue.pop(current_thread_data()->globalData, item)) {
					if (counter++ > NOVA_SPIN_COUNT) {
						counter = 0;
						SleepConditionVariableCS(&main_condition_variable(), &dummy_critical_section(), INFINITE);
					}
				}
			}

			template<bool ToMain, std::enable_if_t<!ToMain, int> = 0>
			void push(queue_item_t&& item) {
				m_globalQueue.push(current_thread_data()->globalData, std::forward<queue_item_t>(item));
				WakeConditionVariable(&global_condition_variable());
				WakeConditionVariable(&main_condition_variable());
			}

			template<bool ToMain, typename Collection, std::enable_if_t<!ToMain, int> = 0>
			void push(Collection && items) {
				m_globalQueue.push(current_thread_data()->globalData, std::forward<decltype(items)>(items));
				WakeAllConditionVariable(&global_condition_variable());
				WakeConditionVariable(&main_condition_variable());
			}

			template<bool ToMain, std::enable_if_t<ToMain, int> = 0>
			void push(queue_item_t&& item) {
				m_mainQueue.push(current_thread_data()->mainData, std::forward<queue_item_t>(item));
				is_main_queue_empty.store(false, std::memory_order_relaxed);
				WakeConditionVariable(&main_condition_variable());
			}

			template<bool ToMain, typename Collection, std::enable_if_t<ToMain, int> = 0>
			void push(Collection && items) {
				m_mainQueue.push(current_thread_data()->mainData, std::forward<decltype(items)>(items));
				is_main_queue_empty.store(false, std::memory_order_relaxed);
				WakeConditionVariable(&main_condition_variable());
			}

			thread_data make_thread_data() {
				return { m_globalQueue.make_queue_data(), m_mainQueue.make_queue_data() };
			}

		private:
			bool try_pop_main_queue(queue_item_t & item) {
				bool exp = false;
				if (is_main_queue_empty.compare_exchange_weak(exp, true, std::memory_order_relaxed)
					&& m_mainQueue.pop(current_thread_data()->mainData, item)) {
					is_main_queue_empty.store(false, std::memory_order_relaxed);
					return true;
				}
				return false;
			}

			queue_wrapper() = default;
			std::atomic_bool is_main_queue_empty{ true };
			moodycamel_adaptor m_globalQueue;
			moodycamel_adaptor m_mainQueue;

			// Meyers singletons
			static CONDITION_VARIABLE & global_condition_variable() {
				static condition_wrapper cw;
				return cw.cv;
			}
			static CONDITION_VARIABLE & main_condition_variable() {
				static condition_wrapper cw;
				return cw.cv;
			}
			static CRITICAL_SECTION & dummy_critical_section() {
				static thread_local critical_wrapper cs;
				static thread_local critical_lock cl(cs.cs);
				return cs.cs;
			}
		};
	}

#pragma endregion

#pragma region worker_thread

	namespace impl {
		class worker_thread {
		public:
			worker_thread()
				: m_thread_data(queue_wrapper::instance().make_thread_data()), 
				m_thread(&worker_thread::init_thread, this) {
			}
			worker_thread(worker_thread&&) noexcept = default;
			worker_thread& operator=(worker_thread&&) noexcept = default;
			static void job_loop() {
				while (running()) {
					job j;
					if (worker_thread::get_thread_id() == 0)
						queue_wrapper::instance().pop_main(j);
					else
						queue_wrapper::instance().pop(j);

					raii_ptr<dependency_token> rp(resources::dependent_token(), &j.get_dependency_token());
					j();
				}
			}
			static std::size_t get_thread_id() {
				return thread_id();
			}
			static std::size_t get_thread_count() {
				return thread_count();
			}
			static void kill_worker() {
				running() = false;
			}
			void Join() {
				m_thread.join();
			}
		private:
			void init_thread() {
				{
					critical_lock cl(init_lock().cs);
					thread_id() = thread_count();
					thread_count()++;
				}
				queue_wrapper::current_thread_data() = &m_thread_data;
				ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
				job_loop();
			}

			// Meyers singletons
			static std::size_t & thread_id() {
				static thread_local std::size_t id = 0;
				return id;
			}
			static bool & running() {
				static thread_local bool running = true;
				return running;
			}
			static std::size_t & thread_count() {
				static std::size_t count = 1;
				return count;
			}
			static critical_wrapper & init_lock() {
				static critical_wrapper lock;
				return lock;
			}

			queue_wrapper::thread_data m_thread_data;
			std::thread m_thread;
		};
	}

#pragma endregion

#pragma region function & batch_function

	namespace impl {

		template<typename Callable, typename ... Params>
		class batch_function;

		template <typename Callable, typename ... Params>
		class function {
		public:
			template<typename _Callable, typename ... _Params, std::enable_if_t<!std::is_same<std::decay_t<_Callable>, function<Callable, Params...>>::value, int> = 0>
			function(_Callable&& callable, _Params&&... args)
				: m_callable(std::forward<_Callable>(callable)), m_tuple(std::forward<_Params>(args)...) {
			}

			void operator () () {
				impl::apply(m_callable, m_tuple);
			}

			//Ignore the squiggly, this is defined further down
			//operator BatchJob<Callable, Params...>() const;

			typedef batch_function<Callable, Params...> batchType;

		protected:
			typedef std::tuple<Params...> tuple_t;
			Callable m_callable;
			tuple_t m_tuple;
		};

		template <typename Callable, typename ... Params>
		class batch_function : public function<Callable, Params...> {
		public:
			template<typename _Callable, typename ... _Params, std::enable_if_t<!std::is_same<std::decay_t<_Callable>, batch_function<Callable, Params...>>::value, int> = 0>
			batch_function(_Callable&& callable, _Params&&... args)
				: function<Callable, Params...>(std::forward<_Callable>(callable), std::forward<_Params>(args)...), m_sections((std::min)(static_cast<std::size_t>(end() - start()), impl::worker_thread::get_thread_count())) {
			}

			/*explicit BatchJob(SimpleJob<Callable, Params...> & sj)
			: SimpleJob<Callable, Params...>(sj), m_sections((std::min)(End() - Start(), static_cast<indexType>(internal::WorkerThread::GetThreadCount()))) {
			}*/

			void operator () () {
				tuple_t params = this->m_tuple;
				std::size_t batchStart = start();
				std::size_t batchEnd = end();
				float count = static_cast<float>(batchEnd - batchStart);
				std::size_t section = static_cast<start_index_t>(InterlockedIncrement(&m_currentSection));
				std::size_t newStart = static_cast<start_index_t>(batchStart + std::floor(static_cast<float>(count*(section - 1) / m_sections)));
				batchEnd = static_cast<std::size_t>(batchStart + std::floor(count*section / m_sections));

				start(params) = static_cast<start_index_t>(newStart);
				end(params) = static_cast<end_index_t>(batchEnd);
				impl::apply(this->m_callable, params);
			}

			std::size_t get_sections() const {
				return m_sections;
			}

			void* operator new(size_t i)
			{
				return _mm_malloc(i, 32);
			}

			void operator delete(void* p)
			{
				_mm_free(p);
			}

			typedef function<Callable, Params...> simpleType;

		private:
			typedef typename simpleType::tuple_t tuple_t;
			typedef impl::integral_index<tuple_t> tupleIntegralIndex;
			typedef std::tuple_element_t<tupleIntegralIndex::value, tuple_t> start_index_t;
			typedef std::tuple_element_t<tupleIntegralIndex::value + 1, tuple_t> end_index_t;
			alignas(32) uint32_t m_currentSection = 0;
			std::size_t m_sections;

			start_index_t& start() {
				return start(this->m_tuple);
			}
			static start_index_t& start(tuple_t & tuple) {
				return std::get<tupleIntegralIndex::value>(tuple);
			}
			end_index_t& end() {
				return end(this->m_tuple);
			}
			static end_index_t& end(tuple_t & tuple) {
				return std::get<tupleIntegralIndex::value + 1>(tuple);
			}
		};

		/*template<typename Callable, typename ...Params>
		inline SimpleJob<Callable, Params...>::operator BatchJob<Callable, Params...>() const {
		return BatchJob<Callable, Params...>(*this);
		}*/
	}

	// Returns a Runnable wrapper for the given Callable and parameters.
	template <typename Callable, typename ... Params>
	auto bind(Callable&& callable, Params&&... args) {
		return impl::function<std::decay_t<Callable>, std::decay_t<Params>...>(std::forward<Callable>(callable), std::forward<Params>(args)...);
	}

	// Returns a Batch Runnable wrapper for the given Callable and parameters. The Callable must have two sequential integral parameters; these are, respectively, the start and end of the range over which the batch will be split.
	template <typename Callable, typename ... Params>
	auto bind_batch(Callable&& callable, Params&&... args) {
		return impl::batch_function<std::decay_t<Callable>, std::decay_t<Params>...>(std::forward<Callable>(callable), std::forward<Params>(args)...);
	}

#pragma endregion

#pragma region split_batch_function

	namespace impl {

		//Converts a BatchJob into a vector of Envelopes
		template<typename Callable, typename ... Params>
		static std::vector<job> split_batch_function(batch_function<Callable, Params...> && bf) {
			std::vector<job> jobs;
			jobs.reserve(bf.get_sections());
			typedef batch_function<Callable, Params...> ptrType;
			std::shared_ptr<ptrType> basePtr = std::make_shared<ptrType>(bf);
			for (unsigned int section = 0; section < bf.get_sections(); section++) {
				jobs.emplace_back(basePtr);
			}
			return jobs;
		}

		//Converts a BatchJob into a vector of Envelopes without copying the job to the heap
		template<typename Callable, typename ... Params>
		static std::vector<job> split_batch_function_no_alloc(batch_function<Callable, Params...> & bf) {
			std::vector<job> jobs;
			jobs.reserve(bf.get_sections());
			for (unsigned int section = 0; section < bf.get_sections(); section++) {
				jobs.emplace_back(&bf);
			}
			return jobs;
		}

	}

#pragma endregion

#pragma region pack_runnable

	namespace impl {

		//Loads a Runnable into an envelope and pushes it to the given vector. Allocates.
		template<bool Alloc, typename Runnable, std::size_t N, std::enable_if_t<Alloc, int> = 0>
		static void pack_runnable(std::array<job, N> & jobs, std::size_t & index, std::vector<job> & batchJobs, Runnable&& runnable) {
			jobs[index++] = std::move(job{ std::forward<Runnable>(runnable) });
		}

		//Loads a Runnable into an envelope and pushes it to the given vector
		template<bool Alloc, typename Runnable, std::size_t N, std::enable_if_t<!Alloc, int> = 0>
		static void pack_runnable(std::array<job, N> & jobs, std::size_t & index, std::vector<job> & batchJobs, Runnable&& runnable) {
			jobs[index++] = { &runnable };
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector Allocates.
		template<bool Alloc, typename Callable, typename ... Params, std::size_t N, std::enable_if_t<Alloc, int> = 0>
		static void pack_runnable(std::array<job, N> & jobs, std::size_t & index, std::vector<job> & batchJobs, batch_function<Callable, Params...> && bf) {
			std::vector<job> splitEnvs = split_batch_function(std::forward<decltype(bf)>(bf));
			batchJobs.insert(batchJobs.end(), std::make_move_iterator(splitEnvs.begin()), std::make_move_iterator(splitEnvs.end()));
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
		template<bool Alloc, typename Callable, typename ... Params, std::size_t N, std::enable_if_t<!Alloc, int> = 0>
		static void pack_runnable(std::array<job, N> & envs, std::size_t & index, std::vector<job> & batchJobs, batch_function<Callable, Params...> && bf) {
			std::vector<job> splitEnvs = split_batch_function_no_alloc(bf);
			batchJobs.insert(batchJobs.end(), std::make_move_iterator(splitEnvs.begin()), std::make_move_iterator(splitEnvs.end()));
		}

		//Breaks a Runnable off the parameter pack and recurses
		template<bool Alloc, std::size_t N, typename Runnable, typename ... Runnables>
		static void pack_runnable(std::array<job, N> & jobs, std::size_t & index, std::vector<job> & batchJobs, Runnable && runnable, Runnables&&... runnables) {
			pack_runnable<Alloc>(jobs, index, batchJobs, std::forward<Runnable>(runnable));
			pack_runnable<Alloc>(jobs, index, batchJobs, std::forward<Runnables>(runnables)...);
		}

		//Generates Envelopes from the given Runnables and inserts them into a std::array (for standalone) or a std::vector (for batch)
		template<bool Alloc, std::size_t N, typename ... Runnables>
		static void pack_runnable(std::array<job, N> & jobs, std::vector<job> & batchJobs, Runnables&&... runnables) {
			std::size_t index(0);
			pack_runnable<Alloc>(jobs, index, batchJobs, std::forward<Runnables>(runnables)...);
		}

	}

#pragma endregion

#pragma region push

#pragma region helpers

	namespace impl{

		template<typename... Args> struct batch_count;

		template<>
		struct batch_count<> {
			static const int value = 0;
		};

		template<typename ... Params, typename... Args>
		struct batch_count<batch_function<Params...>, Args...> {
			static const int value = 1 + batch_count<Args...>::value;
		};

		template<typename First, typename... Args>
		struct batch_count<First, Args...> {
			static const int value = batch_count<Args...>::value;
		};
	}

#pragma endregion

	namespace impl{

		//Queues an array of Envelopes
		template<bool ToMain, std::size_t N>
		void push(std::array<impl::job, N> && jobs) {
			queue_wrapper::instance().push<ToMain>(std::forward<decltype(jobs)>(jobs));
		}

		template<bool ToMain, std::size_t N>
		void push(dependency_token & dt, std::array<job, N> && jobs) {
			for (job & j : jobs)
				j.set_dependency_token(dt);
			push<ToMain>(std::forward<decltype(jobs)>(jobs));
		}

		//Queues a vector of envelopes
		template<bool ToMain>
		void push(std::vector<impl::job> && jobs) {
			queue_wrapper::instance().push<ToMain>(std::forward<decltype(jobs)>(jobs));
		}

		//Queues a vector of envelopes
		template<bool ToMain>
		void push(dependency_token & dt, std::vector<job> && jobs) {
			for (job & j : jobs)
				j.set_dependency_token(dt);
			push<ToMain>(std::forward<decltype(jobs)>(jobs));
		}

		template<bool ToMain, bool Dependent, typename Collection, std::enable_if_t<Dependent, int> = 0>
		void push_picker(Collection && collection) {
			impl::push<ToMain>(*resources::dependent_token(), std::forward<Collection>(collection));
		}

		template<bool ToMain, bool Dependent, typename Collection, std::enable_if_t<!Dependent, int> = 0>
		void push_picker(Collection && collection) {
			impl::push<ToMain>(std::forward<Collection>(collection));
		}

		//Queues a set of Runnables
		template<bool ToMain, bool Dependent, typename ... Runnables >
		void push(Runnables&&... runnables) {
			using namespace impl;
			std::array<job, sizeof...(Runnables)-batch_count<Runnables...>::value> jobs;
			std::vector<job> batchJobs;
			batchJobs.reserve(batch_count<Runnables...>::value * 4);
			pack_runnable<true>(jobs, batchJobs, std::forward<Runnables>(runnables)...);
			push_picker<ToMain, Dependent>(std::move(jobs));
			push_picker<ToMain, Dependent>(std::move(batchJobs));
		}
	}

#pragma endregion

#pragma region thread control helpers

	// Control that causes Runnables to be invoked on the main thread.
	struct to_main {};
	// Control that causes a synchronous invocation to return to the main thread
	struct return_main {};
	// Control that prevents a currently active synchronous invocation from returning until the invokees of the asynchronous invocation affected by the Control return
	struct dependent {};

	template<typename T, typename ... Ts>
	struct includes_type;

	template<typename T>
	struct includes_type<T> {
		static const bool value = false;
	};

	template<typename T, typename ... Ts>
	struct includes_type<T, T, Ts...> {
		static const bool value = true;
	};

	template<typename T, typename U, typename ... Ts>
	struct includes_type<T, U, Ts...> {
		static const bool value = false || includes_type<T, Ts...>::value;
	};

#pragma endregion

	// Asynchronously invokes a set of Runnable objects.
	// Accepts the following Controls:
	// to_main - the Runnables will be invoked on the main thread
	// dependent - if the current job was invoked synchronously, it will not return until the Runnables all return
	template<typename ... Controls, typename ... Runnables>
	void push(Runnables&&... runnables) {
		impl::push<includes_type<to_main, Controls...>::value, includes_type<dependent, Controls...>::value>(std::forward<Runnables>(runnables)...);
	}

#pragma region call

	namespace impl{

		template<bool ToMain, std::size_t N>
		void call_push(dependency_token & dt, std::array<job, N> && jobs, std::vector<job> && batchJobs) {
			push<ToMain>(dt, std::forward<decltype(jobs)>(jobs));
			push<ToMain>(dt, std::forward<decltype(batchJobs)>(batchJobs));
		}

		template<bool ToMain>
		void call_push(dependency_token & dt) {}

		inline void finish_called_job(LPVOID oldFiber) {
			//Mark for re-use
			resources::available_fibers().push_back(GetCurrentFiber());
			SwitchToFiber(oldFiber);

			//Re-use starts here
			resources::call_token()->Release();
		}

		//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
		//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
		//has been suspended.
		inline void open_call_token_enter_job_loop(LPVOID jobPtr) {
			resources::call_token()->Release();
			worker_thread::job_loop();
		}

		inline LPVOID get_fresh_fiber() {
			LPVOID newFiber;

			if (resources::available_fibers().size() > 0) {
				newFiber = resources::available_fibers()[resources::available_fibers().size() - 1];
				resources::available_fibers().pop_back();
			}
			else
				newFiber = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, (LPFIBER_START_ROUTINE)open_call_token_enter_job_loop, nullptr);

			return newFiber;
		}

		template<typename ... Controls, typename ... Params>
		void call(Params&&... params) {
			PVOID currentFiber = GetCurrentFiber();
			auto completionJob = [=]() {
				nova::push<std::conditional_t<includes_type<return_main, Controls...>::value, to_main, void>>(bind(&finish_called_job, currentFiber));
			};

			dependency_token dt(job{ &completionJob });
			resources::call_token() = &dt;

			call_push<includes_type<to_main, Controls...>::value>(dt, std::forward<Params>(params)...);

			SwitchToFiber(get_fresh_fiber());
		}

	}

#pragma endregion

	// Synchronously invokes a set of Runnable objects.
	// Accepts the following Controls:
	// to_main - the Runnables will be invoked on the main thread
	// return_main - the call will return to the main thread
	template<typename ... Controls, typename ... Runnables>
	void call(Runnables&&... runnables) {
		using namespace impl;
		std::array<job, sizeof...(Runnables)-batch_count<Runnables...>::value> jobs;
		std::vector<job> batchJobs;
		pack_runnable<false>(jobs, batchJobs, std::forward<Runnables>(runnables)...);
		impl::call<Controls...>(std::move(jobs), std::move(batchJobs));
	}

	// Moves the current call stack to the main thread, then returns.
	inline void switch_to_main() {
		impl::call<return_main>();
	}

	// Invokes a Callable object once for each value between start (inclusive) and end (exclusive), passing the value to each invocation.
	template<typename Callable, typename ... Params>
	void parallel_for(std::size_t start, std::size_t end, Callable&& callable, Params&&... args) {
		call(bind_batch([&](std::size_t start, std::size_t end, Params&&... args) {
			for (std::size_t c = start; c < end; c++)
				std::forward<Callable>(callable)(c, std::forward<Params>(args)...);
		}, start, end, std::forward<Params>(args)...));
	}

	// Starts the job system with the given number of threads and enters the given Callable with the given parameters. Returns when kill_all_workers is called.
	template <typename Callable, typename ... Params>
	void start_async(unsigned threadCount, Callable&& callable, Params&& ... args) {
		using namespace impl;

		//create threads
		std::vector<worker_thread> threads;

		threads.resize(threadCount - 1);

		queue_wrapper::thread_data td = queue_wrapper::instance().make_thread_data();
		queue_wrapper::current_thread_data() = &td;
		ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);

		push<true>(bind(std::forward<Callable>(callable), std::forward<Params>(args)...));

		worker_thread::job_loop();

		for (worker_thread & wt : threads)
			wt.Join();
	}

	// Starts the job system with as many threads as the system can run concurrently and enters the given Callable with the given parameters. Returns when kill_all_workers is called.
	template <typename Callable, typename ... Params>
	void start_async(Callable&& callable, Params&& ... args) {
		start_async(std::thread::hardware_concurrency(), std::forward<Callable>(callable), std::forward<Params>(args)...);
	}

	//Starts the job system with the given number of threads and enters the given Callable with the given parameters. Returns when the Callable returns.
	template <typename Callable, typename ... Params>
	void start_sync(unsigned threadCount, Callable&& callable, Params&& ... args) {
		using namespace impl;

		//create threads
		std::vector<worker_thread> threads;

		threads.resize(threadCount - 1);

		queue_wrapper::thread_data td = queue_wrapper::instance().make_thread_data();
		queue_wrapper::current_thread_data() = &td;
		ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);

		nova::call<nova::to_main, nova::return_main>(bind(std::forward<Callable>(callable), std::forward<Params>(args)...));

		for (worker_thread & wt : threads)
			push(bind(worker_thread::kill_worker));
		for (worker_thread & wt : threads)
			wt.Join();
	}

	//Starts the job system with as many threads as the system can run concurrently and enters the given Callable with the given parameters. Returns when the Callable returns.
	template <typename Callable, typename ... Params>
	void start_sync(Callable&& callable, Params&& ... args) {
		start_sync(std::thread::hardware_concurrency(), std::forward<Callable>(callable), std::forward<Params>(args)...);
	}

	// Stops the job system, triggering a return from the start function. No invocations attempted after this one will occur.
	inline void kill_all_workers() {
		using namespace impl;
		for (unsigned c = 0; c < worker_thread::get_thread_count(); c++)
			nova::push(bind(worker_thread::kill_worker));
	}
}

#endif // !NOVA_H

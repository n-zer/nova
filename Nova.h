#pragma once

#include <vector>
#include <array>
#include <thread>
#include <mutex>
#include <tuple>
#include <Windows.h>
#include <memory>

#include "QueueAdaptors.h"

namespace Nova {

#pragma region SimpleJob & BatchJob

	namespace internal{

#pragma region Helpers

		namespace detail {
			template <class F, class Tuple, std::size_t... I>
			constexpr decltype(auto) apply_impl(F &&f, Tuple &&t, std::index_sequence<I...>) {
				return std::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...);
			}
		}  // namespace detail

		template <class F, class Tuple>
		constexpr decltype(auto) apply(F &&f, Tuple &&t) {
			return detail::apply_impl(
				std::forward<F>(f), std::forward<Tuple>(t),
				std::make_index_sequence<std::tuple_size_v<std::decay_t<Tuple>>>{});
		}

		template<class Tuple>
		struct IntegralIndex;

		template<bool Val, class Tuple>
		struct IntegralIndexInner;

		template<class T, class ... Types>
		struct IntegralIndex<std::tuple<T, Types...>> {
			static const std::size_t value = IntegralIndexInner<std::is_integral<T>::value, std::tuple<T, Types...>>::value;
		};

		template<typename Tuple>
		struct IntegralIndexInner<true, Tuple> {
			static const std::size_t value = 0;
		};

		template<typename T, typename ... Types>
		struct IntegralIndexInner<false, std::tuple<T, Types...>> {
			static const std::size_t value = 1 + IntegralIndex<std::tuple<Types...>>::value;
		};

#pragma endregion

		template<typename Callable, typename ... Params>
		class BatchJob;

		template <typename Callable, typename ... Params>
		class SimpleJob {
		public:
			SimpleJob(Callable callable, Params... args)
				: m_callable(callable), m_tuple(args...) {
			}

			void operator () () const {
				internal::apply(m_callable, m_tuple);
			}

			//Ignore the squiggly, this is defined further down
			//operator BatchJob<Callable, Params...>() const;

			typedef BatchJob<Callable, Params...> batchType;

		protected:
			Callable m_callable;
			std::tuple<Params...> m_tuple;
		};

		template <typename Callable, typename ... Params>
		class BatchJob : public SimpleJob<Callable, Params...> {
		public:
			typedef internal::IntegralIndex<std::tuple<Params...>> tupleIntegralIndex;
			typedef std::tuple_element_t<tupleIntegralIndex::value, std::tuple<Params...>> startIndexType;
			typedef std::tuple_element_t<tupleIntegralIndex::value + 1, std::tuple<Params...>> endIndexType;

			BatchJob(Callable callable, Params... args)
				: SimpleJob<Callable, Params...>(callable, args...), m_sections((std::min)(static_cast<std::size_t>(End() - Start()), internal::WorkerThread::GetThreadCount())) {
			}

			/*explicit BatchJob(SimpleJob<Callable, Params...> & sj)
			: SimpleJob<Callable, Params...>(sj), m_sections((std::min)(End() - Start(), static_cast<indexType>(internal::WorkerThread::GetThreadCount()))) {
			}*/

			void operator () () {
				std::tuple<Params...> params = this->m_tuple;
				std::size_t start = Start();
				std::size_t end = End();
				float count = static_cast<float>(end - start);
				std::size_t section = static_cast<startIndexType>(InterlockedIncrement(&m_currentSection));
				std::size_t newStart = static_cast<startIndexType>(start + floorf(static_cast<float>(count*(section - 1) / m_sections)));
				end = static_cast<std::size_t>(start + floorf(count*section / m_sections));

				Start(params) = static_cast<startIndexType>(newStart);
				End(params) = static_cast<endIndexType>(end);
				internal::apply(this->m_callable, params);
			}

			std::size_t GetSections() const {
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

			typedef SimpleJob<Callable, Params...> simpleType;

		private:
			alignas(32) uint32_t m_currentSection = 0;
			std::size_t m_sections;

			startIndexType& Start() {
				return Start(this->m_tuple);
			}
			static startIndexType& Start(std::tuple<Params...> & tuple) {
				return std::get<tupleIntegralIndex::value>(tuple);
			}
			endIndexType& End() {
				return End(this->m_tuple);
			}
			static endIndexType& End(std::tuple<Params...> & tuple) {
				return std::get<tupleIntegralIndex::value + 1>(tuple);
			}
		};

		/*template<typename Callable, typename ...Params>
		inline SimpleJob<Callable, Params...>::operator BatchJob<Callable, Params...>() const {
		return BatchJob<Callable, Params...>(*this);
		}*/
	}

	//Creates a job from a Callable object and parameters
	template <typename Callable, typename ... Params>
	internal::SimpleJob<Callable, Params...> MakeJob(Callable callable, Params... args) {
		return internal::SimpleJob<Callable, Params...>(callable, args...);
	}

	//Creates a batch job from a Callable object and parameters (starting with a pair of BatchIndexes)
	template <typename Callable, typename ... Params>
	internal::BatchJob<Callable, Params...> MakeBatchJob(Callable callable, Params... args) {
		return internal::BatchJob<Callable, Params...>(callable, args...);
	}

#pragma endregion


#pragma region Helpers

	namespace internal{

		template<typename... Args> struct BatchCount;

		template<>
		struct BatchCount<> {
			static const int value = 0;
		};

		template<typename ... Params, typename... Args>
		struct BatchCount<BatchJob<Params...>, Args...> {
			static const int value = 1 + BatchCount<Args...>::value;
		};

		template<typename First, typename... Args>
		struct BatchCount<First, Args...> {
			static const int value = BatchCount<Args...>::value;
		};

		template<typename T>
		struct IsShared {
			static const bool value = false;
		};

		template<typename T>
		struct IsShared<std::shared_ptr<T>> {
			static const bool value = true;
		};
	}

#pragma endregion

#pragma region Envelope & SealedEnvelope

	namespace internal{ class Envelope; }

	class SealedEnvelope {
	public:
		SealedEnvelope() {}
		SealedEnvelope(internal::Envelope & e);
		SealedEnvelope(internal::Envelope && e);
		template<typename Runnable>
		SealedEnvelope(Runnable runnable);
		void Open() {
			m_seal.reset();
		}
	private:
		struct Seal;
		std::shared_ptr<Seal> m_seal;
	};

	namespace internal{
		template<typename T>
		class RaiiPointer {
		public:
			RaiiPointer(std::decay_t<T> *& se, std::decay_t<T> * val)
				: m_ptr(se) {
				m_ptr = val;
			}
			~RaiiPointer() {
				m_ptr = nullptr;
			}
		private:
			T *& m_ptr;
		};

		class Envelope {
		public:
			Envelope() {}
			~Envelope() {
				m_deleteFunc(m_runnable);
			}
			Envelope(const Envelope &) = delete;
			Envelope operator=(const Envelope &) = delete;
			Envelope(Envelope && e) noexcept {
				Move(std::forward<Envelope>(e));
			}
			Envelope& operator=(Envelope && e) noexcept {
				m_deleteFunc(m_runnable);
				Move(std::forward<Envelope>(e));
				return *this;
			}

			template<typename Runnable, std::enable_if_t<!std::is_same<std::decay_t<Runnable>, Envelope>::value, int> = 0>
			Envelope(Runnable&& runnable)
				: m_runFunc(&Envelope::RunRunnable<std::decay_t<Runnable>>), 
				m_deleteFunc(&Envelope::DeleteRunnable<std::decay_t<Runnable>>), 
				m_runnable(new std::decay_t<Runnable>(std::forward<Runnable>(runnable))) {
			}

			template<typename Runnable, std::enable_if_t<!std::is_same<std::decay_t<Runnable>, Envelope>::value, int> = 0>
			Envelope(Runnable * runnable)
				: m_runFunc(&Envelope::RunRunnable<std::decay_t<Runnable>>), m_runnable(runnable) {
			}

			Envelope(void(*runFunc)(void*), void(*deleteFunc)(void*), void * runnable)
				: m_runFunc(runFunc), m_deleteFunc(deleteFunc), m_runnable(runnable) {
			}

			void operator () () const {
				m_runFunc(m_runnable);
			}

			void AddSealedEnvelope(SealedEnvelope & se) {
				m_sealedEnvelope = se;
			}
			void OpenSealedEnvelope() {
				m_sealedEnvelope.Open();
			}

			SealedEnvelope & GetSealedEnvelope() {
				return m_sealedEnvelope;
			}

			template <typename Runnable, std::enable_if_t<!IsShared<Runnable>::value, int> = 0>
			static void RunRunnable(void * runnable) {
				(static_cast<Runnable*>(runnable))->operator()();
			}

			template <typename Runnable, std::enable_if_t<IsShared<Runnable>::value, int> = 0>
			static void RunRunnable(void * runnable) {
				static_cast<Runnable*>(runnable)->get()->operator()();
			}

			template <typename Runnable>
			static void DeleteRunnable(void * runnable) {
				delete static_cast<Runnable*>(runnable);
			}

			static void NoOp(void * runnable) {};

		private:
			void Move(Envelope && e) noexcept {
				m_runFunc = e.m_runFunc;
				m_deleteFunc = e.m_deleteFunc;
				m_runnable = e.m_runnable;
				m_sealedEnvelope = std::move(e.m_sealedEnvelope);
				e.m_runFunc = &Envelope::NoOp;
				e.m_deleteFunc = &Envelope::NoOp;
				e.m_runnable = nullptr;
			}

			void(*m_runFunc)(void *) = &Envelope::NoOp;
			void(*m_deleteFunc)(void*) = &Envelope::NoOp;
			void * m_runnable = nullptr;
			SealedEnvelope m_sealedEnvelope;
		};
	}

	struct SealedEnvelope::Seal {
		Seal(internal::Envelope & e)
			: m_env(std::move(e)) {
		}

		template <typename Runnable>
		Seal(Runnable runnable)
			: m_env(std::forward<Runnable>(runnable)) {
		}

		~Seal() {
			m_env();
		}

		internal::Envelope m_env;
	};

	template<typename Runnable>
	SealedEnvelope::SealedEnvelope(Runnable runnable)
		: m_seal(std::make_shared<Seal>(std::forward<Runnable>(runnable))) {
	}

	inline SealedEnvelope::SealedEnvelope(internal::Envelope & e)
		: m_seal(std::make_shared<Seal>(e)) {
	}

	inline SealedEnvelope::SealedEnvelope(internal::Envelope && e)
		: m_seal(std::make_shared<Seal>(std::forward<internal::Envelope>(e))) {
	}

#pragma endregion

#pragma region QueueWrapper

#define SPIN_COUNT 10000

#ifndef NOVA_QUEUE_TYPE
#define NOVA_QUEUE_TYPE MoodycamelAdaptor
#endif // !NOVA_QUEUE_TYPE

	namespace internal{
		class CriticalLock {
		public:
			CriticalLock(CRITICAL_SECTION& cs) : m_cs(cs) {
				EnterCriticalSection(&m_cs);
			}

			~CriticalLock() {
				LeaveCriticalSection(&m_cs);
			}

		private:
			CriticalLock(const CriticalLock&) = delete;

		private:
			CRITICAL_SECTION & m_cs;
		};

		struct CriticalWrapper {
			CriticalWrapper() {
				InitializeCriticalSection(&cs);
			}

			~CriticalWrapper() {
				DeleteCriticalSection(&cs);
			}

			CRITICAL_SECTION cs;
		};

		template <typename T>
		class QueueWrapper {
		public:
			QueueWrapper() {
				InitializeConditionVariable(&s_cv);
			}

			void Pop(T& item) {
				unsigned counter = 0;
				while (!m_globalQueue.Pop(item)) {
					if (counter++ > SPIN_COUNT) {
						counter = 0;
						SleepConditionVariableCS(&s_cv, &s_cs.cs, INFINITE);
					}
				}
			}

			void PopMain(T& item) {
				unsigned counter = 0;
				while (!m_globalQueue.Pop(item) && !m_mainQueue.Pop(item)) {
					if (counter++ > SPIN_COUNT) {
						counter = 0;
						SleepConditionVariableCS(&s_mainCV, &s_cs.cs, INFINITE);
					}
				}
			}

			template<bool ToMain, std::enable_if_t<!ToMain, int> = 0>
			void Push(T&& item) {
				m_globalQueue.Push(std::forward<T>(item));
				WakeConditionVariable(&s_cv);
				WakeConditionVariable(&s_mainCV);
			}

			template<bool ToMain, typename Collection, std::enable_if_t<!ToMain, int> = 0>
			void Push(Collection && items) {
				m_globalQueue.Push(std::forward<decltype(items)>(items));
				WakeAllConditionVariable(&s_cv);
				WakeConditionVariable(&s_mainCV);
			}

			template<bool ToMain, std::enable_if_t<ToMain, int> = 0>
			void Push(T&& item) {
				m_mainQueue.Push(std::forward<T>(item));
				WakeConditionVariable(&s_mainCV);
			}

			template<bool ToMain, typename Collection, std::enable_if_t<ToMain, int> = 0>
			void Push(Collection && items) {
				m_mainQueue.Push(std::forward<decltype(items)>(items));
				WakeConditionVariable(&s_mainCV);
			}

		private:
			NOVA_QUEUE_TYPE<T> m_globalQueue;
			NOVA_QUEUE_TYPE<T> m_mainQueue;
			static CONDITION_VARIABLE s_cv;
			static CONDITION_VARIABLE s_mainCV;
			static thread_local CriticalWrapper s_cs;
			static thread_local CriticalLock s_cl;
		};

		template<typename T>
		CONDITION_VARIABLE QueueWrapper<T>::s_cv;

		template<typename T>
		CONDITION_VARIABLE QueueWrapper<T>::s_mainCV;

		template<typename T>
		thread_local CriticalWrapper QueueWrapper<T>::s_cs;

		template<typename T>
		thread_local CriticalLock QueueWrapper<T>::s_cl(QueueWrapper<T>::s_cs.cs);
	}

#pragma endregion

	namespace internal{
		class Resources {
		public:
			//Meyers singletons
			static QueueWrapper<Envelope>& QueueWrapper() {
				static Nova::internal::QueueWrapper<Nova::internal::Envelope> qw;
				return qw;
			}
			static std::vector<LPVOID>& AvailableFibers() {
				static thread_local std::vector<LPVOID> af;
				return af;
			}
			static SealedEnvelope *& CallTrigger() {
				static thread_local SealedEnvelope * ct;
				return ct;
			}
			static SealedEnvelope *& DependentToken() {
				static thread_local SealedEnvelope * se;
				return se;
			}
		};
	}

#pragma region WorkerThread

	namespace internal {
		//Forward declarations
		void PopMain(Envelope & e);
		void Pop(Envelope & e);

		class WorkerThread {
		public:
			WorkerThread() {
				m_thread = std::thread(InitThread);
			}
			static void JobLoop() {
				while (Running()) {
					Envelope e;
					if (WorkerThread::GetThreadId() == 0)
						PopMain(e);
					else
						Pop(e);

					RaiiPointer<SealedEnvelope> rp(Resources::DependentToken(), &e.GetSealedEnvelope());
					e();
				}
			}
			static std::size_t GetThreadId() {
				return ThreadId();
			}
			static std::size_t GetThreadCount() {
				return ThreadCount();
			}
			static void KillWorker() {
				Running() = false;
			}
			void Join() {
				m_thread.join();
			}
		private:
			static void InitThread() {
				{
					std::lock_guard<std::mutex> lock(InitLock());
					ThreadId() = ThreadCount();
					ThreadCount()++;
				}
				ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
				JobLoop();
			}

			//Meyers singletons
			static std::size_t & ThreadId() {
				static thread_local std::size_t id = 0;
				return id;
			}
			static bool & Running() {
				static thread_local bool running = true;
				return running;
			}
			static std::size_t & ThreadCount() {
				static std::size_t count = 1;
				return count;
			}
			static std::mutex & InitLock() {
				static std::mutex lock;
				return lock;
			}

			std::thread m_thread;
		};
	}

#pragma endregion

	namespace internal{

#pragma region Push

		//Queues a set of Runnables
		template<bool ToMain, bool Dependent, typename ... Runnables >
		void Push(Runnables&&... runnables) {
			using namespace internal;
			std::array<Envelope, sizeof...(Runnables)-BatchCount<Runnables...>::value> envs;
			std::vector<Envelope> batchEnvs;
			batchEnvs.reserve(BatchCount<Runnables...>::value * 4);
			PackRunnable<true>(envs, batchEnvs, std::forward<Runnables>(runnables)...);
			PushPicker<ToMain, Dependent>(std::move(envs));
			PushPicker<ToMain, Dependent>(std::move(batchEnvs));
		}

		template<bool ToMain, bool Dependent, typename Collection, std::enable_if_t<Dependent, int> = 0>
		void PushPicker(Collection && collection) {
			internal::Push<ToMain>(*Resources::DependentToken(), std::forward<Collection>(collection));
		}

		template<bool ToMain, bool Dependent, typename Collection, std::enable_if_t<!Dependent, int> = 0>
		void PushPicker(Collection && collection) {
			internal::Push<ToMain>(std::forward<Collection>(collection));
		}

		//Queues an array of Envelopes
		template<bool ToMain, std::size_t N>
		void Push(std::array<internal::Envelope, N> && envs) {
			internal::Resources::QueueWrapper().Push<ToMain>(std::forward<decltype(envs)>(envs));
		}

		template<bool ToMain, std::size_t N>
		void Push(SealedEnvelope & se, std::array<Envelope, N> && envs) {
			for (Envelope & e : envs)
				e.AddSealedEnvelope(se);
			Push<ToMain>(std::forward<decltype(envs)>(envs));
		}

		//Queues a vector of envelopes
		template<bool ToMain>
		void Push(std::vector<internal::Envelope> && envs) {
			internal::Resources::QueueWrapper().Push<ToMain>(std::forward<decltype(envs)>(envs));
		}

		//Queues a vector of envelopes
		template<bool ToMain>
		void Push(SealedEnvelope & se, std::vector<Envelope> && envs) {
			for (Envelope & e : envs)
				e.AddSealedEnvelope(se);
			Push<ToMain>(std::forward<decltype(envs)>(envs));
		}

#pragma endregion


#pragma region Call

		template<bool ToMain, bool FromMain, typename ... Params>
		void Call(Params&&... params) {
			PVOID currentFiber = GetCurrentFiber();
			auto completionJob = [=]() {
				Nova::Push<FromMain>(MakeJob(&FinishCalledJob, currentFiber));
			};

			SealedEnvelope se(Envelope{ &completionJob });
			Resources::CallTrigger() = &se;

			CallPush<ToMain>(se, std::forward<Params>(params)...);

			SwitchToFiber(GetNewFiber());
		}

		template<bool ToMain, std::size_t N>
		void CallPush(SealedEnvelope & se, std::array<Envelope, N> && envs, std::vector<Envelope> && batchEnvs) {
			Push<ToMain>(se, std::forward<decltype(envs)>(envs));
			Push<ToMain>(se, std::forward<decltype(batchEnvs)>(batchEnvs));
		}

		template<bool ToMain>
		void CallPush(SealedEnvelope & se){}

		inline void FinishCalledJob(LPVOID oldFiber) {
			//Mark for re-use
			Resources::AvailableFibers().push_back(GetCurrentFiber());
			SwitchToFiber(oldFiber);

			//Re-use starts here
			Resources::CallTrigger()->Open();
		}

		//Starting point for a new fiber, queues a list of jobs and immediately enters the job loop.
		//This is used by the Call- functions to delay queueing of jobs until after the calling fiber
		//has been suspended.
		inline void OpenCallTriggerAndEnterJobLoop(LPVOID jobPtr) {
			Resources::CallTrigger()->Open();
			WorkerThread::JobLoop();
		}

		inline LPVOID GetNewFiber() {
			LPVOID newFiber;

			if (Resources::AvailableFibers().size() > 0) {
				newFiber = Resources::AvailableFibers()[Resources::AvailableFibers().size() - 1];
				Resources::AvailableFibers().pop_back();
			}
			else
				newFiber = CreateFiberEx(0, 0, FIBER_FLAG_FLOAT_SWITCH, (LPFIBER_START_ROUTINE)OpenCallTriggerAndEnterJobLoop, nullptr);

			return newFiber;
		}

#pragma endregion

#pragma region Pop

		//Attempts to grab an envelope from the queue
		inline void Pop(Envelope &e) {
			Resources::QueueWrapper().Pop(e);
		}

		inline void PopMain(Envelope &e) {
			Resources::QueueWrapper().PopMain(e);
		}

#pragma endregion

#pragma region PackRunnable

		//Generates Envelopes from the given Runnables and inserts them into a std::array (for standalone) or a std::vector (for batch)
		template<bool Alloc, std::size_t N, typename ... Runnables>
		static void PackRunnable(std::array<Envelope, N> & envs, std::vector<Envelope> & batchEnvs, Runnables&&... runnables) {
			std::size_t index(0);
			PackRunnable<Alloc>(envs, index, batchEnvs, std::forward<Runnables>(runnables)...);
		}

		//Breaks a Runnable off the parameter pack and recurses
		template<bool Alloc, std::size_t N, typename Runnable, typename ... Runnables>
		static void PackRunnable(std::array<Envelope, N> & envs, std::size_t & index, std::vector<Envelope> & batchEnvs, Runnable && runnable, Runnables&&... runnables) {
			PackRunnable<Alloc>(envs, index, batchEnvs, std::forward<Runnable>(runnable));
			PackRunnable<Alloc>(envs, index, batchEnvs, std::forward<Runnables>(runnables)...);
		}

		//Loads a Runnable into an envelope and pushes it to the given vector. Allocates.
		template<bool Alloc, typename Runnable, std::size_t N, std::enable_if_t<Alloc, int> = 0>
		static void PackRunnable(std::array<Envelope, N> & envs, std::size_t & index, std::vector<Envelope> & batchEnvs, Runnable&& runnable) {
			envs[index++] = std::move(Envelope{ std::forward<Runnable>(runnable) });
		}

		//Loads a Runnable into an envelope and pushes it to the given vector
		template<bool Alloc, typename Runnable, std::size_t N, std::enable_if_t<!Alloc, int> = 0>
		static void PackRunnable(std::array<Envelope, N> & envs, std::size_t & index, std::vector<Envelope> & batchEnvs, Runnable&& runnable) {
			envs[index++] = { &runnable };
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector Allocates.
		template<bool Alloc, typename Callable, typename ... Params, std::size_t N, std::enable_if_t<Alloc, int> = 0>
		static void PackRunnable(std::array<Envelope, N> & envs, std::size_t & index, std::vector<Envelope> & batchEnvs, BatchJob<Callable, Params...> && j) {
			std::vector<Envelope> splitEnvs = SplitBatchJob(std::forward<decltype(j)>(j));
			batchEnvs.insert(batchEnvs.end(), std::make_move_iterator(splitEnvs.begin()), std::make_move_iterator(splitEnvs.end()));
		}

		//Special overload for batch jobs - splits into envelopes and inserts them into the given vector
		template<bool Alloc, typename Callable, typename ... Params, std::size_t N, std::enable_if_t<!Alloc, int> = 0>
		static void PackRunnable(std::array<Envelope, N> & envs, std::size_t & index, std::vector<Envelope> & batchEnvs, BatchJob<Callable, Params...> && j) {
			std::vector<Envelope> splitEnvs = SplitBatchJobNoAlloc(j);
			envs.insert(envs.end(), splitEnvs.begin(), splitEnvs.end());
		}

#pragma endregion

#pragma region SplitBatchJob

		//Converts a BatchJob into a vector of Envelopes
		template<typename Callable, typename ... Params>
		static std::vector<Envelope> SplitBatchJob(BatchJob<Callable, Params...> && j) {
			std::vector<Envelope> jobs;
			jobs.reserve(j.GetSections());
			//auto* basePtr = new BatchJob<Callable, Params...>(std::forward<decltype(j)>(j));
			typedef BatchJob<Callable, Params...> ptrType;
			std::shared_ptr<ptrType> basePtr = std::make_shared<ptrType>(j);
			//SealedEnvelope se(Envelope(&Envelope::NoOp, &Envelope::DeleteRunnable<BatchJob<Callable, Params...>>, basePtr));
			for (unsigned int section = 0; section < j.GetSections(); section++) {
				jobs.emplace_back(basePtr);
				//jobs[section].AddSealedEnvelope(se);
			}
			return jobs;
		}

		//Converts a BatchJob into a vector of Envelopes without copying the job to the heap
		template<typename Callable, typename ... Params>
		static std::vector<Envelope> SplitBatchJobNoAlloc(BatchJob<Callable, Params...> & j) {
			std::vector<Envelope> jobs;
			jobs.reserve(j.GetSections());
			for (unsigned int section = 0; section < j.GetSections(); section++) {
				jobs.emplace_back(&j);
			}
			return jobs;
		}

#pragma endregion

	}

	//Queues a set of Runnables
	template<bool ToMain = false, typename ... Runnables>
	void Push(Runnables&&... runnables) {
		internal::Push<ToMain, false>(std::forward<Runnables>(runnables)...);
	}

	//Queues a set of Runnables
	template<bool ToMain = false, typename ... Runnables>
	void PushDependent(Runnables&&... runnables) {
		internal::Push<ToMain, true>(std::forward<Runnables>(runnables)...);
	}

	//Loads a set of Runnables, queues them, then pauses the current call stack until they finish
	template<bool ToMain = false, bool FromMain = false, typename ... Runnables>
	void Call(Runnables&&... runnables) {
		using namespace internal;
		std::array<Envelope, sizeof...(Runnables)-BatchCount<Runnables...>::value> envs;
		std::vector<Envelope> batchEnvs;
		PackRunnable<false>(envs, batchEnvs, std::forward<Runnables>(runnables)...);
		internal::Call<ToMain, FromMain>(std::move(envs), std::move(batchEnvs));
	}

	inline void SwitchToMain() {
		internal::Call<false, true>();
	}

	//Invokes a Callable object once for each value between start (inclusive) and end (exclusive), passing the value to each invocation
	template<typename Callable, typename ... Params>
	void ParallelFor(Callable callable, unsigned start, unsigned end, Params... args) {
		Call(MakeBatchJob([&](unsigned start, unsigned end, Params... args) {
			for (BatchIndex c = start; c < end; c++)
				callable(c, args...);
		}, start, end, args...));
	}

	//Starts the job system. Pass in a callable object and some parameters.
	template <typename Callable, typename ... Params>
	void StartAsync(Callable callable, Params ... args) {
		StartAsync(std::thread::hardware_concurrency(), callable, args...);
	}

	//Starts the job system. Pass in a callable object and some parameters.
	template <typename Callable, typename ... Params>
	void StartAsync(unsigned threadCount, Callable callable, Params ... args) {
		using namespace internal;

		//create threads
		std::vector<WorkerThread> threads;

		threads.resize(threadCount - 1);

		ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);

		Push<true>(MakeJob(callable, args...));

		WorkerThread::JobLoop();

		for (WorkerThread & wt : threads)
			wt.Join();
	}

	//Starts the job system. Pass in a callable object and some parameters.
	template <typename Callable, typename ... Params>
	void StartSync(Callable callable, Params ... args) {
		StartSync(std::thread::hardware_concurrency(), callable, args...);
	}

	//Starts the job system. Pass in a callable object and some parameters.
	template <typename Callable, typename ... Params>
	void StartSync(unsigned threadCount, Callable callable, Params ... args) {
		using namespace internal;

		//create threads
		std::vector<WorkerThread> threads;

		threads.resize(threadCount - 1);

		ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);

		Nova::Call<true, true>(MakeJob(callable, args...));

		for (WorkerThread & wt : threads)
			Push(MakeJob(WorkerThread::KillWorker));
		for (WorkerThread & wt : threads)
			wt.Join();
	}


	inline void KillAllWorkers() {
		using namespace internal;
		for (unsigned c = 0; c < WorkerThread::GetThreadCount(); c++)
			Nova::Push(MakeJob(WorkerThread::KillWorker));
	}
}

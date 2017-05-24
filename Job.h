#pragma once

#include <tuple>
#include <Windows.h>

namespace Nova {
	//Denotes the start or end of a batch job's range
	typedef unsigned BatchIndex;

	namespace internal{
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
			operator BatchJob<Callable, Params...>() const;

			typedef BatchJob<Callable, Params...> batchType;

		protected:
			Callable m_callable;
			std::tuple<Params...> m_tuple;
		};

		template <typename Callable, typename ... Params>
		class BatchJob : public SimpleJob<Callable, Params...> {
		public:
			BatchJob(Callable callable, Params... args)
				: SimpleJob<Callable, Params...>(callable, args...), m_sections((std::min)(End() - Start(), internal::WorkerThread::GetThreadCount())) {
			}

			BatchJob(SimpleJob<Callable, Params...> & sj)
				: SimpleJob<Callable, Params...>(sj), m_sections((std::min)(End() - Start(), internal::WorkerThread::GetThreadCount())) {
			}

			void operator () () {
				std::tuple<Params...> params = this->m_tuple;
				BatchIndex start = Start();
				BatchIndex end = End();
				float count = static_cast<float>(end - start);
				unsigned int section = InterlockedIncrement(&m_currentSection);
				unsigned int newStart = static_cast<unsigned>(start + floorf(static_cast<float>(count*(section - 1) / m_sections)));
				end = static_cast<unsigned int>(start + floorf(count*section / m_sections));

				Start(params) = newStart;
				End(params) = end;
				internal::apply(this->m_callable, params);
			}

			unsigned GetSections() const {
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
			unsigned m_sections;

			unsigned& Start() {
				return Start(this->m_tuple);
			}
			static unsigned& Start(std::tuple<Params...> & tuple) {
				return std::get<internal::Index<BatchIndex, decltype(tuple)>::value>(tuple);
			}
			unsigned& End() {
				return End(this->m_tuple);
			}
			static unsigned& End(std::tuple<Params...> & tuple) {
				return std::get<internal::Index<BatchIndex, decltype(tuple)>::value + 1>(tuple);
			}
		};

		template<typename Callable, typename ...Params>
		inline SimpleJob<Callable, Params...>::operator BatchJob<Callable, Params...>() const {
			return BatchJob<Callable, Params...>(*this);
		}
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
}

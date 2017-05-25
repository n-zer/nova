#pragma once

#include <tuple>
#include <Windows.h>

namespace Nova {

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
}

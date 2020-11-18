/*
 * Copyright 2019 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Giannis Georgalis, https://github.com/ggeorgalis
 */

#pragma once

#include <future>
#include <memory>
#include <type_traits>

#include <thousandeyes/futures/detail/FutureWithContinuation.h>
#include <thousandeyes/futures/detail/FutureWithChaining.h>
#include <thousandeyes/futures/detail/typetraits.h>

#include <thousandeyes/futures/Default.h>
#include <thousandeyes/futures/Executor.h>

namespace thousandeyes {
namespace futures {

//! \brief type trait that checks if the return value of a continuation function is encapsulated in a future
template<template<typename> typename TFuture, class TIn, class TFunc>
using is_res_encapsulated_in_future =
    std::is_same<
        TFuture<
            typename detail::nth_template_param<
                0,
                typename std::result_of<
                    typename std::decay<TFunc>::type(TFuture<TIn>)
                >::type
            >::type
        >
        ,
        typename std::result_of<
            typename std::decay<TFunc>::type(TFuture<TIn>)
        >::type
    >;

//! \brief SFINAE meta-type that resolves to the continuation's return type.
template<template<typename> typename TFuture, class TIn, class TFunc>
using cont_returns_value_t =
    typename std::enable_if<
        (
            !detail::is_template<
                typename std::result_of<
                    typename std::decay<TFunc>::type(TFuture<TIn>)
                >::type
            >::value 
            || 
            !is_res_encapsulated_in_future<TFuture, TIn, TFunc>::value
        )
        ,
        TFuture<
            typename std::result_of<
                typename std::decay<TFunc>::type(TFuture<TIn>)
            >::type
        >
    >::type;

//! \brief Creates a future that becomes ready when the input future becomes ready.
//!
//! \par The resulting future contains the value returned by invoking the given
//! continuation function.
//!
//! \param executor The object that waits for the given future to become ready.
//! \param timeLimit The maximum time to wait for the given future to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<value> that contains the value returned by the given
//! continuation function.
template<template<typename> typename TFuture, class TIn, class TFunc>
cont_returns_value_t<TFuture, TIn,TFunc> then(std::shared_ptr<Executor> executor,
                                      std::chrono::microseconds timeLimit,
                                      TFuture<TIn> f,
                                      TFunc&& cont)
{
    using TOut = typename std::result_of<
            typename std::decay<TFunc>::type(TFuture<TIn>)
        >::type;

    std::promise<TOut> p;
    //cont_returns_value_t<TFuture, TIn, TFunc> result = p.get_future();
    auto result = p.get_future();

    executor->watch(std::make_unique<detail::FutureWithContinuation<TFuture, TIn, TOut, TFunc>>(
        std::move(timeLimit),
        std::move(f),
        std::move(p),
        std::forward<TFunc>(cont)
    ));

    return result;
}

//! \brief Creates a future that becomes ready when the input future becomes ready.
//!
//! \par The resulting future contains the value returned by invoking the given
//! continuation function.
//!
//! \param executor The object that waits for the given future to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<value> that contains the value returned by the given
//! continuation function.
template<template<typename> typename TFuture, class TIn, class TFunc>
cont_returns_value_t <TFuture, TIn, TFunc > 
    then(std::shared_ptr<Executor> executor,
        TFuture<TIn> f,
        TFunc&& cont)
{
    return then<TFuture, TIn, TFunc>(std::move(executor),
                            std::chrono::hours(1),
                            std::move(f),
                            std::forward<TFunc>(cont));
}

//! \brief Creates a future that becomes ready when the input future becomes ready.
//!
//! \par The resulting future contains the value returned by invoking the given
//! continuation function. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param timeLimit The maximum time to wait for the given future to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<value> that contains the value returned by the given
//! continuation function.
template<template<typename> typename TFuture, class TIn, class TFunc> 
cont_returns_value_t<TFuture, TIn, TFunc> 
    then(std::chrono::microseconds timeLimit,
                                      TFuture<TIn> f,
                                      TFunc&& cont)
{
    return then<TFuture, TIn, TFunc>(Default<Executor>(),
                            std::move(timeLimit),
                            std::move(f),
                            std::forward<TFunc>(cont));
}

//! \brief Creates a future that becomes ready when the input future becomes ready.
//!
//! \par The resulting future contains the value returned by invoking the given
//! continuation function. This function uses the default Executor
//! object to wait for the given futures to become ready. If there isn't any default
//! Executor object registered, this function's behavior is undefined.
//!
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<value> that contains the value returned by the given
//! continuation function.
template<template<typename> typename TFuture, class TIn, class TFunc>
cont_returns_value_t<TFuture, TIn, TFunc> 
    then(TFuture<TIn> f,
        TFunc&& cont)
{
    return then<TFuture, TIn, TFunc>(std::chrono::hours(1),
                            std::move(f),
                            std::forward<TFunc>(cont));
}

//! \brief SFINAE meta-type that resolves to the continuation's return future type.
template<template<typename> typename TFuture, class TIn, class TFunc>
using cont_returns_future_t =
    typename std::enable_if<
        detail::is_template<
            typename std::result_of<
                typename std::decay<TFunc>::type(TFuture<TIn>)
            >::type
        >::value &&
        is_res_encapsulated_in_future<TFuture, TIn, TFunc>::value
        ,
        typename std::result_of<
            typename std::decay<TFunc>::type(TFuture<TIn>)
        >::type
    >::type;

//! \brief Creates a future that becomes ready when both the input future and the
//! continuation future become ready.
//!
//! \par The resulting future contains the value contained in the future obtained
//! by invoking the given continuation function on the ready input future.
//!
//! \param executor The object that waits for the futures to become ready.
//! \param timeLimit The maximum time to wait for both futures to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<value> that contains the value contained in the future
//! returned by the given continuation function.
template<template<typename> typename TFuture, class TIn, class TFunc>
cont_returns_future_t<TFuture, TIn, TFunc> then(std::shared_ptr<Executor> executor,
                                       std::chrono::microseconds timeLimit,
                                       TFuture<TIn> f,
                                       TFunc&& cont)
{
    using TOut = typename detail::nth_template_param<
            0,
            typename std::result_of<
                typename std::decay<TFunc>::type(TFuture<TIn>)
            >::type
        >::type;

    std::promise<TOut> p;
    //cont_returns_future_t<TFuture, TIn, TFunc> result = p.get_future();
    auto result = p.get_future();

    executor->watch(std::make_unique<detail::FutureWithChaining<TFuture, TIn, TOut, TFunc>>(
        std::move(timeLimit),
        executor,
        std::move(f),
        std::move(p),
        std::forward<TFunc>(cont)
    ));

    return result;
}

//! \brief Creates a future that becomes ready when both the input future and the
//! continuation future become ready.
//!
//! \par The resulting future contains the value contained in the future obtained
//! by invoking the given continuation function on the ready input future.
//!
//! \param executor The object that waits for the futures to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa WaitableTimedOutException
//!
//! \return An std::future<value> that contains the value contained in the future
//! returned by the given continuation function.
template<template<typename> typename TFuture, class TIn, class TFunc>
cont_returns_future_t<TFuture, TIn, TFunc> then(std::shared_ptr<Executor> executor,
                                       TFuture<TIn> f,
                                       TFunc&& cont)
{
    return then<TFuture, TIn, TFunc>(std::move(executor),
                            std::chrono::hours(1),
                            std::move(f),
                            std::forward<TFunc>(cont));
}

//! \brief Creates a future that becomes ready when both the input future and the
//! continuation future become ready.
//!
//! \par The resulting future contains the value contained in the future obtained
//! by invoking the given continuation function on the ready input future. This
//! function uses the default Executor object to wait for the futures
//! to become ready. If there isn't any default Executor object registered,
//! this function's behavior is undefined.
//!
//! \param timeLimit The maximum time to wait for both futures to become ready.
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the futures to become ready exceeds the
//! given timeLimit, the resulting future becomes ready with an exception of type
//! WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<value> that contains the value contained in the future
//! returned by the given continuation function.
template<template<typename> typename TFuture, class TIn, class TFunc>
cont_returns_future_t<TFuture, TIn, TFunc> then(std::chrono::microseconds timeLimit,
                                       TFuture<TIn> f,
                                       TFunc&& cont)
{
    return then<TFuture, TIn, TFunc>(Default<Executor>(),
                            std::move(timeLimit),
                            std::move(f),
                            std::forward<TFunc>(cont));
}

//! \brief Creates a future that becomes ready when both the input future and the
//! continuation future become ready.
//!
//! \par The resulting future contains the value contained in the future obtained
//! by invoking the given continuation function on the ready input future. This
//! function uses the default Executor object to wait for the futures
//! to become ready. If there isn't any default Executor object registered,
//! this function's behavior is undefined.
//!
//! \param f The input future to wait and invoke the continuation function on.
//! \param cont The continuation function to invoke on the ready input future.
//!
//! \note If the total time for waiting the input future to become ready exceeds
//! a maximum threshold defined by the library (typically 1h), the resulting future
//! becomes ready with an exception of type WaitableTimedOutException.
//!
//! \sa Default, WaitableTimedOutException
//!
//! \return An std::future<value> that contains the value contained in the future
//! returned by the given continuation function.
template<template<typename> typename TFuture, class TIn, class TFunc>
cont_returns_future_t<TFuture, TIn, TFunc> then(TFuture<TIn> f,
                                       TFunc&& cont)
{
    return then<TFuture, TIn, TFunc>(std::chrono::hours(1),
                            std::move(f),
                            std::forward<TFunc>(cont));
}

} // namespace futures
} // namespace thousandeyes

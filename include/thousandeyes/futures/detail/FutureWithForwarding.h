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

#include <thousandeyes/futures/TimedWaitable.h>

namespace thousandeyes {
namespace futures {
namespace detail {

template<template<typename> typename TFuture, class T>
class FutureWithForwarding : public TimedWaitable {
public:
    FutureWithForwarding(std::chrono::microseconds waitLimit,
                         TFuture<T> f,
                         std::promise<T> p) :
        TimedWaitable(std::move(waitLimit)),
        f_(std::move(f)),
        p_(std::move(p))
    {}

    FutureWithForwarding(const FutureWithForwarding& o) = delete;
    FutureWithForwarding& operator=(const FutureWithForwarding& o) = delete;

    FutureWithForwarding(FutureWithForwarding&& o) = default;
    FutureWithForwarding& operator=(FutureWithForwarding&& o) = default;

    bool timedWait(const std::chrono::microseconds& timeout) override
    {
        return f_.wait_for(timeout) == std::future_status::ready;
    }

    void dispatch(std::exception_ptr err) override
    {
        if (err) {
            p_.set_exception(err);
            return;
        }

        try {
            p_.set_value(f_.get());
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    TFuture<T> f_;
    std::promise<T> p_;
};

// Partial specialization for void output type

template<template<typename> typename TFuture>
class FutureWithForwarding<TFuture, void> : public TimedWaitable {
public:
    FutureWithForwarding(std::chrono::microseconds waitLimit,
                         TFuture<void> f,
                         std::promise<void> p) :
        TimedWaitable(std::move(waitLimit)),
        f_(std::move(f)),
        p_(std::move(p))
    {}

    FutureWithForwarding(const FutureWithForwarding& o) = delete;
    FutureWithForwarding& operator=(const FutureWithForwarding& o) = delete;

    FutureWithForwarding(FutureWithForwarding&& o) = default;
    FutureWithForwarding& operator=(FutureWithForwarding&& o) = default;

    bool timedWait(const std::chrono::microseconds& timeout) override
    {
        return f_.wait_for(timeout) == std::future_status::ready;
    }

    void dispatch(std::exception_ptr err) override
    {
        if (err) {
            p_.set_exception(err);
            return;
        }

        try {
            f_.get();
            p_.set_value();
        }
        catch (...) {
            p_.set_exception(std::current_exception());
        }
    }

private:
    TFuture<void> f_;
    std::promise<void> p_;
};

} // namespace detail
} // namespace futures
} // namespace thousandeyes

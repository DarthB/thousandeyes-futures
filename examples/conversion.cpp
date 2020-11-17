/*
 * Copyright 2019 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Giannis Georgalis, https://github.com/ggeorgalis
 */

#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <memory>

#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/then.h>

using namespace std;
using namespace std::chrono;
using namespace thousandeyes::futures;

namespace {

template<class T>
future<T> getValueAsync(const T& value)
{
    return async(launch::async, [value]() {
        return value;
    });
}

template<class T>
shared_future<T> getValueAsyncShared(const T& value)
{
	return async(launch::async, [value]() {
		return value;
		});
}

} // namespace

int main(int argc, const char* argv[])
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);

    // test cont_returns_value_t
    auto f = then(getValueAsync(1821), [](future<int> f) {
        return to_string(f.get());
    });
    string result = f.get();
    cout << "Got result: " << result << endl;

    // test cont_returns_future_t
    auto f2 = then(getValueAsync(1821), [](future<int> f) -> future<string> {
        auto val = f.get();
        return async(launch::async, [val]() {
                return to_string(val);
            });
        });
	result = f2.get();
	cout << "Got result with internal future: " << result << endl;

    // test cont_returns_value_t with shared
    auto f3 = then(getValueAsyncShared(1821), 
        [](shared_future<int> f) -> std::string {
        return to_string(f.get());
    } );
	result = f3.get();
	cout << "Got shared result: " << result << endl;

	// test cont_returns_shared_future_t
	auto f4 = then(getValueAsync(1821), [](shared_future<int> f) -> shared_future<string> {
	auto val = f.get();
	    return async(launch::async, [val]() {
			return to_string(val);
			}).share();
		});
	result = f4.get();
	cout << "Got result with internal future: " << result << endl;

    executor->stop();
}

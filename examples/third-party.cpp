/*
 * Copyright 2019 ThousandEyes, Inc.
 *
 * Use of this source code is governed by an MIT-style
 * license that can be found in the LICENSE file or at
 * https://opensource.org/licenses/MIT.
 *
 * @author Tim Janus, https://github.com/darthb86
 */

#include <array>
#include <functional>
#include <future>
#include <iostream>
#include <numeric>
#include <string>
#include <sstream>
#include <memory>
#include <mutex>

#include <thousandeyes/futures/DefaultExecutor.h>
#include <thousandeyes/futures/all.h>
#include <thousandeyes/futures/then.h>

using namespace std;
using namespace std::chrono;
using namespace thousandeyes::futures;

//! A global mutex to lock the cout object for multithreading output
mutex g_coutMut;

//! \brief An error type indicating a license error of the third-party interface
struct tpi_license_error : public exception {};

//! \brief An example for a third party interface that needs special handling like handling license issues.
class third_party_interface {
public:
    void checkoutLicense() { hasLicense = true; }

    void setExampleString(string exampleString) {
        checkLicense();
        this->exampleString = exampleString;
    }

    string getExampleString() const {
        checkLicense();
        return exampleString;
    }

    void simulate() {
        checkLicense();
        this_thread::sleep_for(chrono::seconds(2));
    }

private:
    bool hasLicense;

    string exampleString;

    void checkLicense() const {
        if (!hasLicense) { throw tpi_license_error(); }
    }
};

class tpi_evaluate {
public:
    virtual shared_future<string> evaluate() = 0;
};

// imagine:
// class cloud_simulate : public tpi_evaluate { ... };

//! \brief A class that encapsulates an thread that manages a connection to a third-party interface
class local_worker_thread : public tpi_evaluate {
public:
    
    local_worker_thread() {}

    local_worker_thread(const local_worker_thread& other) = delete;
    local_worker_thread& operator=(const local_worker_thread& other) = delete;

    local_worker_thread(local_worker_thread&& other) {
    //    swap(this->taskMutex, other.taskMutex);
        swap(this->tasks, other.tasks);
    }

    local_worker_thread& operator=(local_worker_thread&& other) {
	//	swap(this->taskMutex, other.taskMutex);
		swap(this->tasks, other.tasks);
        return *this;
    }

    using packaged_task_t = packaged_task<void(third_party_interface& tpi)>;

    class task_wrapper_t {
    public:
        task_wrapper_t()
        {}

        task_wrapper_t(const task_wrapper_t&) = delete;
        task_wrapper_t& operator=(const task_wrapper_t&) = delete;

        task_wrapper_t(task_wrapper_t&& other) {
            swap(sharedFuture, other.sharedFuture);
            swap(baseTask, other.baseTask);
        }
        task_wrapper_t& operator=(task_wrapper_t&& other) {
            swap(sharedFuture, other.sharedFuture);
            swap(baseTask, other.baseTask);
            return *this;
        }

        task_wrapper_t(packaged_task<void(third_party_interface&)>&& baseTask) 
        {
            swap(this->baseTask, baseTask);
            sharedFuture = this->baseTask.get_future().share();
        }

        shared_future<void> getFuture() const { return sharedFuture; }

        void operator()(third_party_interface& tpi) {
            baseTask(tpi);
        }

    private:
        shared_future<void> sharedFuture;

        packaged_task_t baseTask;
    };

public:
    template <class F, class... Args>
    auto enqueueTask(F&& f, Args... args)
        -> std::shared_future<typename result_of<F(third_party_interface&, Args...)>::type>
    {
        auto task = packaged_task<void(third_party_interface& tpi)>(
            std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Args>(args)...)
        );

        task_wrapper_t wrap(move(task));
        auto reval = wrap.getFuture();

        {
            // add task
            lock_guard<mutex> lock(taskMutex);
            tasks.emplace_back(move(wrap));
        }

        return reval;
    }

    virtual shared_future<string> evaluate() override {
		// here would be the place to "transfer data" (we skip the first step of ipo here)
		auto enq = enqueueTask([](third_party_interface& tpi) {
            tpi.simulate();
            lock_guard<mutex> lock(g_coutMut);
			cout << "tid:" << this_thread::get_id() << " - simulate done." << endl;
			});

		auto futCosts = then(enq, [](shared_future<void> f) {
			double costs = 42; // retrieved after simulation...
			return costs;
			});

		auto futStr = then(futCosts, [](shared_future<double> c) {
			stringstream str;
			str << "The costs of this equipment configuration is '" << c.get() << "'";
			return str.str();
			});

		return futStr;
    }

    void worker_function() {
        third_party_interface tpi;
        while (true) {
            bool hasTask = false;
            task_wrapper_t curTask;
            {
                // fetch task:
                lock_guard<mutex> lock(taskMutex);
                if (!tasks.empty()) {
                    curTask = move(tasks.front());
                    tasks.erase(tasks.begin());
                    hasTask = true;
                }
            }
            
            if (hasTask) {
                try {
                    curTask(tpi);
                    hasTask = false;
                } catch (tpi_license_error&) {

                }
            } else {
                this_thread::sleep_for(chrono::milliseconds(50));
            }
        }
    }

private:

    vector<task_wrapper_t> tasks;

    mutex taskMutex;
};

class concurrent_evaluator 
    : public tpi_evaluate 
{
public:
    concurrent_evaluator() 
        : index(0) 
    {
		threads.emplace_back();
		threads.emplace_back();

        for (auto& worker : threads)
        {
            thread stdThread(std::bind(&local_worker_thread::worker_function, &worker));
            stdThread.detach();
        }
    }

    concurrent_evaluator(const concurrent_evaluator&) = delete;
    concurrent_evaluator& operator=(const concurrent_evaluator&) = delete;

    concurrent_evaluator(concurrent_evaluator&& other) {
        swap(index, other.index);
        swap(threads, other.threads);
    }

    concurrent_evaluator& operator=(concurrent_evaluator&& other) {
		swap(index, other.index);
		swap(threads, other.threads);
    }

    virtual shared_future<string> evaluate() override {
        auto res = threads[index].evaluate();
        index += 1;
        if (index > 1) index = 0;
        return res;
    }

private:
    int index;

    vector<local_worker_thread> threads;
};

int main(int argc, const char* argv[])
{
    auto executor = make_shared<DefaultExecutor>(milliseconds(10));
    Default<Executor>::Setter execSetter(executor);
    
	cout << "Start two concurrent tpi workers!" << endl;
    concurrent_evaluator evaluator;

    // simulate 5 times
    auto& coutMut = g_coutMut;
    vector<shared_future<void>> container;
    for (int i = 0; i < 5; ++i) {
        auto res = then(evaluator.evaluate(), [&coutMut,i](std::shared_future<string> f){
            lock_guard<mutex> lock(g_coutMut);
            cout << "Done (" << i + 1 << ")> " << f.get() << endl;
            return;
        });
        container.emplace_back(move(res));
    }

//     auto f = all(executor, move(container), [](shared_future<vector<shared_future<void>>> futures) {
//         cout << "Everything done!";
//     });
//     f.get();

    for (auto& f : container) { f.get(); } 
    cout << "Everything done." << endl;

    executor->stop();
}

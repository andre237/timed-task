#pragma ide diagnostic ignored "OCUnusedGlobalDeclarationInspection"

#ifndef TIMED_TASK_WRAPPER_H
#define TIMED_TASK_WRAPPER_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;

typedef chrono::steady_clock Clock;
typedef chrono::time_point<Clock, chrono::nanoseconds> time_point_ns;
typedef chrono::nanoseconds duration_ns;

enum TimeUnit: uint64_t {
    nanoseconds = 1,
    microseconds = nanoseconds * 1'000,
    milliseconds = microseconds * 1'000,
    seconds = milliseconds * 1'000,
    minutes = seconds * 60,
    hours = minutes * 60
};

class StatisticsCollector {
private:

    size_t samples = 0;
    uint64_t acummulation = 0;
    uint64_t compensation = 0;

    uint64_t maxVariance = 0;
    uint64_t minVariance = numeric_limits<uint64_t >::max();

    size_t toleraceExcessionCount = 0;
public:

    StatisticsCollector() = default;
    virtual ~StatisticsCollector() = default;

    void calcSleepCompensation(duration_ns amountSlept) {
        compensation += amountSlept.count();
    }

    void calcExecTimeError(time_point_ns start, time_point_ns end, const duration_ns expected) {
        auto execTime = (end - start);
        uint64_t error = (chrono::abs(execTime - expected).count()); // both upper or lower differences are considered error, hence abs()
        accumulate(error);
        considerVariance(error);

        if (execTime > (expected * 1.05)) { // 5% extra
            toleraceExcessionCount++;
        }
    }

    void accumulate(uint64_t amount) {
        acummulation += amount;
        samples++;
    }

    void considerVariance(uint64_t errorSample) {
        maxVariance = std::max(errorSample, maxVariance);
        minVariance = std::min(errorSample, minVariance);
    }

    void printResults(TimeUnit samplingUnit = TimeUnit::nanoseconds) {
        uint64_t deviation_average_ns = acummulation / samples;
        uint64_t compensation_average_ns = compensation / samples;

        long double convertedDeviation   = (double) deviation_average_ns / samplingUnit;
        long double convertedCompensation = (double) compensation_average_ns / samplingUnit;
        long double convertedMaxVariance = (double) maxVariance / samplingUnit;
        long double convertedMinVariance = (double) minVariance / samplingUnit;

        string unitName = "nanoseconds";

        switch (samplingUnit) {
            case nanoseconds:  break;
            case microseconds: unitName = "microseconds"; break;
            case milliseconds: unitName = "milliseconds"; break;
            case seconds:      unitName = "seconds"; break;
            case minutes:      unitName = "minutes"; break;
            case hours:        unitName = "hours"; break;
        }

        cout.precision(6);
        cout << "  --------------- // --------------\n";
        cout << "Samples taken: "        << samples << endl;
        cout << "Deviation average: "    << fixed << convertedDeviation << " " << unitName << endl;
        cout << "Compensation average: " << fixed << convertedCompensation << " " << unitName << endl;
        cout << "Max variance: "         << fixed << convertedMaxVariance << " " << unitName << endl;
        cout << "Min variance: "         << fixed << convertedMinVariance << " " << unitName << endl;
        cout << "Tolerance exceeded "    << toleraceExcessionCount << " times\n";
    }

};

class TimerTask {
public:
    TimerTask(uint64_t rate, TimeUnit unit, bool enableStatistics = true) :
            rate_(rate),
            ratio_(unit),
            statisticsEnabled(enableStatistics) {
        init();
    }

    TimerTask(const TimerTask&) = delete; // copy and assignment not allowed
    TimerTask& operator=(const TimerTask&) = delete;

    virtual ~TimerTask() {
        if (running_) // don't call again if stop() was explicitly called
            stop(); // if it goes out of scope, stop the timer
    }

    void setRate(uint64_t rate, TimeUnit unit, bool imediate = true) {
        this->rate_ = rate;
        this->ratio_ = unit;

        if (imediate) {
            restart(); // restart imediate
        } else {
            // todo wait the pending execution to restart | IDK how
        }
    }

    void stop() {
        if (workerThread_.joinable()) {
            {
                lock_guard<mutex> lock(mtx_);
                running_ = false;
            }

            cv_.notify_one(); // wake up immediately
            workerThread_.join();
        }

        if (statisticsEnabled) {
            collector.printResults(milliseconds);
        }
    }

private:
    void init() {
        if (rate_ != 0) { // avoid a non-sleeping timer
            workerThread_ = std::thread([&] {
                running_ = true;
                duration_ns full_sleep_time_nanos = std::chrono::nanoseconds(ratio_ * rate_);
                for (;;) {
                    // we need to measure the execution to ensure we keep the cadence between cycles
                    auto start = Clock::now();
                    doAction(); // call virtual function
                    auto stop = Clock::now();

                    auto sleepTime = full_sleep_time_nanos - (stop - start); // compensate doAction timing
                    if (sleepTime.count() < 0) {
                        sleepTime = full_sleep_time_nanos - (chrono::abs(sleepTime) % full_sleep_time_nanos);

                        // there's no reason to know how many cycles were exceeded so we
                        // only get the remainder to know how much was exceeded in the CURRENT cycle
                        // the compensation only applies to next cycle where the action has finished

                        /* x means the sleep time, already compensated
                         * the space between each | is considered a full cycle of the timer
                         *
                         * if the action takes more than one cycle,
                         * compensation will take place at the surpassed cycle
                         *  --->xxxx------>x----------->xxxx------->
                         * |-------|-------|-------|-------|-------|
                         * */
                    }

                    sleepTime -= duration_ns(50'000); // 50 µsec as offset for operations above and below

                    unique_lock<mutex> lock(mtx_); // usual proccess to sleep a condition variable
                    cout << sleepTime.count() << endl;
                    if (cv_.wait_for(lock, sleepTime, [this] { return !running_; })) {
                        break; // running == false, so exit this thread
                    }

                    collector.calcExecTimeError(start, Clock::now(), full_sleep_time_nanos);
                    collector.calcSleepCompensation(sleepTime);
                }
            });
        }
    }

    void restart() {
        stop(); // stop right now and start it again with the new rate
        init();
    }

    virtual void doAction() = 0;

    uint64_t rate_;
    TimeUnit ratio_;

    StatisticsCollector collector{};
    bool statisticsEnabled;

    bool running_{false};

    thread workerThread_;
    mutex mtx_;
    condition_variable cv_;
};

#endif //TIMED_TASK_WRAPPER_H

#pragma once

#include <chrono>

class Heartbeat
{
public:
    using Clock = std::chrono::system_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    explicit Heartbeat(int intervalMs)
        : mHeartbeatIntervalMs(intervalMs)
        , mLastTimePoint(std::chrono::milliseconds::zero())
    {
    }

    bool CheckAndReset()
    {
        auto now = Clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - mLastTimePoint);
        if (duration.count() >= mHeartbeatIntervalMs)
        {
            mLastTimePoint = now;
            return true;
        }
        return false;
    }

    void Reset()
    {
        mLastTimePoint = Clock::now();
    }

private:
    int mHeartbeatIntervalMs;
    TimePoint mLastTimePoint;
};

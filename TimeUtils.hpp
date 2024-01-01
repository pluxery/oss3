#include <iostream>
#include <chrono>
#include <thread>

class TimeUtils {
public:
    static char *TimeNow() {
        std::time_t time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        return std::ctime(&time);
    }

    static void Sleep(int sec) {
        std::this_thread::sleep_for(std::chrono::seconds(sec));
    }
};
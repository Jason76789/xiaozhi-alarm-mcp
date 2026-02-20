#pragma once

#include <string>
#include <vector>
#include <ctime>
#include "esp_timer.h"

#define MAX_ALARMS 10

// 闹钟数据结构
struct Alarm {
    int id;
    bool active;
    time_t time; // 触发时间的时间戳
    int repeat;
    int interval;
    std::string name;
};

class AlarmManager {
public:
    static AlarmManager* get_instance();

    void init();
    bool set_alarm(int delay, int hour, int minute, int repeat, int interval, const std::string& name);
    bool delete_alarm_by_id(int id);
    bool delete_alarm_by_keyword(const std::string& keyword);
    std::string query_all_alarms_json();
    int get_active_alarm_count();

private:
    AlarmManager();
    ~AlarmManager();

    void load_alarms();
    void save_alarms();
    void start_timer_for_alarm(Alarm* alarm);
    void stop_timer_for_alarm(Alarm* alarm);
    static void alarm_timer_callback(void* arg);
    void handle_triggered_alarm(Alarm* alarm);

    Alarm alarms_[MAX_ALARMS];
    esp_timer_handle_t alarm_timers_[MAX_ALARMS];

    static AlarmManager* instance_;
};

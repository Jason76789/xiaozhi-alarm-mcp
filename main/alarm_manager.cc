#include "alarm_manager.h"
#include "settings.h"
#include "esp_log.h"
#include "cJSON.h"
#include <time.h>
#include <cstring>
#include "application.h"
#include "assets/lang_config.h"

static const char *TAG = "ALARM_MANAGER";

AlarmManager* AlarmManager::instance_ = nullptr;

AlarmManager::AlarmManager() {
    memset(alarms_, 0, sizeof(alarms_));
    memset(alarm_timers_, 0, sizeof(alarm_timers_));
}

AlarmManager::~AlarmManager() {
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (alarm_timers_[i]) {
            esp_timer_stop(alarm_timers_[i]);
            esp_timer_delete(alarm_timers_[i]);
        }
    }
}

AlarmManager* AlarmManager::get_instance() {
    if (instance_ == nullptr) {
        instance_ = new AlarmManager();
        instance_->init();
    }
    return instance_;
}

void AlarmManager::init() {
    load_alarms();
}

void AlarmManager::load_alarms() {
    ESP_LOGI(TAG, "Loading alarms from NVS");
    Settings settings("alarm_clock");
    int active_count = 0;

    for (int i = 0; i < MAX_ALARMS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "alarm_%d", i);
        int is_active = settings.GetInt(key, 0);
        ESP_LOGD(TAG, "Alarm %d active status: %d", i, is_active);

        if (is_active == 1) { // Check if alarm is active
            alarms_[i].id = i;
            alarms_[i].active = true;
            
            snprintf(key, sizeof(key), "alarm_time_%d", i);
            alarms_[i].time = settings.GetInt(key, 0);

            snprintf(key, sizeof(key), "alarm_rpt_%d", i);
            alarms_[i].repeat = settings.GetInt(key, 0);

            snprintf(key, sizeof(key), "alarm_itv_%d", i);
            alarms_[i].interval = settings.GetInt(key, 0);

            snprintf(key, sizeof(key), "alarm_name_%d", i);
            alarms_[i].name = settings.GetString(key, "");

            ESP_LOGI(TAG, "Loaded alarm %d: name='%s', time=%lld, repeat=%d, interval=%d",
                    i, alarms_[i].name.c_str(), alarms_[i].time, alarms_[i].repeat, alarms_[i].interval);

            time_t now = time(NULL);
            ESP_LOGD(TAG, "Current time: %lld, Alarm time: %lld, Difference: %lld seconds",
                    now, alarms_[i].time, alarms_[i].time - now);

            if (alarms_[i].time > now) {
                start_timer_for_alarm(&alarms_[i]);
                ESP_LOGI(TAG, "Alarm %d scheduled for %lld", i, alarms_[i].time);
                active_count++;
            } else {
                // Handle overdue alarms (e.g., for repeating alarms)
                if (alarms_[i].repeat > 0) {
                    alarms_[i].time += alarms_[i].interval;
                    start_timer_for_alarm(&alarms_[i]);
                    ESP_LOGI(TAG, "Repeating alarm %d rescheduled for %lld", i, alarms_[i].time);
                    active_count++;
                } else {
                    ESP_LOGW(TAG, "Alarm %d is overdue and not repeating, marking as inactive", i);
                    alarms_[i].active = false; // Mark non-repeating overdue alarms as inactive
                }
            }
        }
    }
    ESP_LOGI(TAG, "Alarm loading complete. Active alarms: %d", active_count);
    save_alarms(); // Clean up inactive alarms from NVS
}

void AlarmManager::save_alarms() {
    Settings settings("alarm_clock", true);
    for (int i = 0; i < MAX_ALARMS; ++i) {
        char key[16];
        snprintf(key, sizeof(key), "alarm_%d", i);
        settings.SetInt(key, alarms_[i].active ? 1 : 0);

        if (alarms_[i].active) {
            snprintf(key, sizeof(key), "alarm_time_%d", i);
            settings.SetInt(key, alarms_[i].time);
            snprintf(key, sizeof(key), "alarm_rpt_%d", i);
            settings.SetInt(key, alarms_[i].repeat);
            snprintf(key, sizeof(key), "alarm_itv_%d", i);
            settings.SetInt(key, alarms_[i].interval);
            snprintf(key, sizeof(key), "alarm_name_%d", i);
            settings.SetString(key, alarms_[i].name.c_str());
        }
    }
}

bool AlarmManager::set_alarm(int delay, int hour, int minute, int repeat, int interval, const std::string& name) {
    ESP_LOGI(TAG, "AlarmManager::set_alarm called with: delay=%d, hour=%d, minute=%d, repeat=%d, interval=%d, name='%s'",
             delay, hour, minute, repeat, interval, name.c_str());

    int free_slot = -1;
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (!alarms_[i].active) {
            free_slot = i;
            break;
        }
    }

    if (free_slot == -1) {
        ESP_LOGE(TAG, "No free slots for new alarm");
        return false;
    }

    Alarm* new_alarm = &alarms_[free_slot];
    new_alarm->id = free_slot;
    new_alarm->active = true;
    new_alarm->repeat = (repeat <= 0) ? 1 : repeat;
    new_alarm->interval = interval;
    new_alarm->name = name;

    time_t now = time(NULL);
    
    if ((hour >= 0 && hour < 24) || (minute >= 0 && minute < 60)) {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        
        if (hour >= 0 && hour < 24) {
            timeinfo.tm_hour = hour;
        }
        if (minute >= 0 && minute < 60) {
            timeinfo.tm_min = minute;
        }
        timeinfo.tm_sec = 0;

        time_t target_time = mktime(&timeinfo);
        if (target_time <= now) {
            target_time += 86400; // if time is in the past, set for tomorrow
        }
        new_alarm->time = target_time;
    } else {
        new_alarm->time = now + delay;
    }

    start_timer_for_alarm(new_alarm);
    save_alarms();
    ESP_LOGI(TAG, "Set alarm %d: %s at %lld", new_alarm->id, new_alarm->name.c_str(), new_alarm->time);
    return true;
}

bool AlarmManager::delete_alarm_by_id(int id) {
    if (id < 0 || id >= MAX_ALARMS || !alarms_[id].active) {
        return false;
    }
    stop_timer_for_alarm(&alarms_[id]);
    alarms_[id].active = false;
    save_alarms();
    ESP_LOGI(TAG, "Deleted alarm %d", id);
    return true;
}

bool AlarmManager::delete_alarm_by_keyword(const std::string& keyword) {
    bool found = false;
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (alarms_[i].active && alarms_[i].name.find(keyword) != std::string::npos) {
            stop_timer_for_alarm(&alarms_[i]);
            alarms_[i].active = false;
            found = true;
            ESP_LOGI(TAG, "Deleted alarm %d by keyword '%s'", i, keyword.c_str());
        }
    }
    if (found) {
        save_alarms();
    }
    return found;
}

std::string AlarmManager::query_all_alarms_json() {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddTrueToObject(root, "success");
    cJSON *alarms_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "alarms", alarms_array);

    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (alarms_[i].active) {
            cJSON *alarm_obj = cJSON_CreateObject();
            cJSON_AddNumberToObject(alarm_obj, "id", alarms_[i].id);
            cJSON_AddStringToObject(alarm_obj, "name", alarms_[i].name.c_str());

            char time_str[32];
            struct tm timeinfo;
            localtime_r(&alarms_[i].time, &timeinfo);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
            cJSON_AddStringToObject(alarm_obj, "time", time_str);

            cJSON_AddNumberToObject(alarm_obj, "repeat", alarms_[i].repeat);
            cJSON_AddNumberToObject(alarm_obj, "interval", alarms_[i].interval);
            cJSON_AddItemToArray(alarms_array, alarm_obj);
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    std::string result(json_str);
    cJSON_free(json_str);
    cJSON_Delete(root);
    return result;
}

int AlarmManager::get_active_alarm_count() {
    int count = 0;
    time_t now = time(NULL);
    for (int i = 0; i < MAX_ALARMS; ++i) {
        if (alarms_[i].active && alarms_[i].time > now) {
            count++;
        }
    }
    ESP_LOGD(TAG, "Active alarms count: %d", count);
    return count;
}

void AlarmManager::start_timer_for_alarm(Alarm* alarm) {
    if (alarm->id < 0 || alarm->id >= MAX_ALARMS) return;

    stop_timer_for_alarm(alarm); // Stop previous timer if any

    time_t now = time(NULL);
    int64_t timeout_us = ((int64_t)alarm->time - now) * 1000000;

    if (timeout_us <= 0) {
        ESP_LOGW(TAG, "Alarm %d time is in the past, not starting timer.", alarm->id);
        // Handle overdue repeating alarm
        if (alarm->repeat > 0) {
            handle_triggered_alarm(alarm);
        }
        return;
    }

    esp_timer_create_args_t timer_args = {
        .callback = &alarm_timer_callback,
        .arg = alarm,
        .name = "alarm_timer"
    };

    esp_err_t err = esp_timer_create(&timer_args, &alarm_timers_[alarm->id]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer for alarm %d: %s", alarm->id, esp_err_to_name(err));
        return;
    }

    err = esp_timer_start_once(alarm_timers_[alarm->id], timeout_us);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start timer for alarm %d: %s", alarm->id, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Timer started for alarm %d, timeout %lld us", alarm->id, timeout_us);
    }
}

void AlarmManager::stop_timer_for_alarm(Alarm* alarm) {
    if (alarm->id < 0 || alarm->id >= MAX_ALARMS) return;

    if (alarm_timers_[alarm->id]) {
        esp_timer_stop(alarm_timers_[alarm->id]);
        esp_timer_delete(alarm_timers_[alarm->id]);
        alarm_timers_[alarm->id] = nullptr;
    }
}

void AlarmManager::alarm_timer_callback(void* arg) {
    Alarm* alarm = static_cast<Alarm*>(arg);
    ESP_LOGI(TAG, "Alarm %d triggered: %s", alarm->id, alarm->name.c_str());
    get_instance()->handle_triggered_alarm(alarm);
}

void AlarmManager::handle_triggered_alarm(Alarm* alarm) {
    // TODO: Implement the actual alarm action (e.g., play sound, show on display)
    ESP_LOGI(TAG, "!!!!!!!! ALARM TRIGGERED: %s !!!!!!!!", alarm->name.c_str());

    // 修复闹钟响铃无声音的问题
    // 1. 先调用 AbortSpeaking 方法清理当前可能存在的对话状态
    Application::GetInstance().AbortSpeaking(kAbortReasonNone);

    // 2. 播放一个本地提示音（使用 P3_EXCLAMATION 声音作为替代）
    // 注意：需要确保音频服务在播放前已初始化
    Application::GetInstance().PlaySound(Lang::Sounds::P3_EXCLAMATION);

    if (alarm->repeat > 1) {
        alarm->repeat--;
        alarm->time += alarm->interval;
        start_timer_for_alarm(alarm);
    } else {
        alarm->active = false;
    }
    save_alarms();
}

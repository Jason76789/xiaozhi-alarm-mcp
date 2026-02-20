#include <iostream>
#include <chrono>
#include <thread>
#include "main/application.h"
#include "main/alarm_manager.h"
#include "main/mcp_server.h"

int main() {
    std::cout << "Testing Xiaozhi Alarm System..." << std::endl;

    // 初始化 AlarmManager
    auto alarm_mgr = AlarmManager::get_instance();
    std::cout << "AlarmManager instance created" << std::endl;

    // 测试设置一个简单的闹钟（5秒后触发）
    std::cout << "Setting a 5-second alarm..." << std::endl;
    bool success = alarm_mgr->set_alarm(5, -1, -1, 1, 0, "Test Alarm");
    std::cout << "Set alarm result: " << (success ? "SUCCESS" : "FAILED") << std::endl;

    // 查询所有闹钟
    std::cout << "\nAll alarms:" << std::endl;
    std::string alarms_json = alarm_mgr->query_all_alarms_json();
    std::cout << alarms_json << std::endl;

    // 等待闹钟触发
    std::cout << "\nWaiting for alarm to trigger (10 seconds)..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));

    return 0;
}
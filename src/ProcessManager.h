#pragma once

#include <windows.h>
#include <string>
#include <filesystem>

namespace tcode {

class ProcessManager {
public:
    ProcessManager(const std::wstring& jarPath);
    ~ProcessManager();

    bool Start();
    void Stop();
    bool IsRunning();
    void Monitor(); // To be called periodically

private:
    std::wstring _jarPath;
    std::wstring _workingDir;
    PROCESS_INFORMATION _pi;
    bool _isStarted;
};

} // namespace tcode

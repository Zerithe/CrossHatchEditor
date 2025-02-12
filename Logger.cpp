#include "Logger.h"
#include <imgui.h>

Logger::Logger() {
    std::cout.rdbuf(this);  // Redirect std::cout to this class
}

Logger::~Logger() {
    std::cout.rdbuf(nullptr);  // Reset redirection
}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

int Logger::overflow(int c) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (c != EOF) {
        logBuffer.put(static_cast<char>(c));
    }
    return c;
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(logMutex);
    logBuffer.str("");
    logBuffer.clear();
}

void Logger::DrawImGuiLogger() {
    std::lock_guard<std::mutex> lock(logMutex);

    ImGui::Begin("Log Console");
    if (ImGui::Button("Clear")) {
        Clear();
    }

    ImGui::Separator();

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::TextUnformatted(logBuffer.str().c_str());

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}
#include "runner.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace runner {

    static RunnerConfig config_;
    static bool initialized_ = false;

    static std::chrono::steady_clock::time_point execute_start_;
    static std::chrono::steady_clock::time_point execute_end_;

    static finish_hook_t finish_hook_;
    static TestData input_data_;
    static TestData output_data_;

    const RunnerConfig& init(int argc, char* argv[]) {
        config_ = RunnerConfig{};
        finish_hook_ = nullptr;
        input_data_  = TestData{};
        output_data_ = TestData{};

        for(int i = 1; i < argc; i += 2) {
            std::string key = argv[i];
            if(i + 1 >= argc) break;
            std::string val = argv[i + 1];
            if(key == "--input")             config_.input_dir = val;
            else if(key == "--output")       config_.output_dir = val;
            else if(key == "--threads") {
                try { config_.thread_count = std::stoi(val); }
                catch(...) { config_.thread_count = 1; }
            }
            else if(key == "--monitor-mode") config_.monitor_mode = val;
        }

        if(!config_.output_dir.empty()) {
            std::filesystem::create_directories(config_.output_dir);
        }

        initialized_ = true;
        return config_;
    }

    const RunnerConfig& config() { return config_; }

    TestData& input()  { return input_data_; }
    TestData& output() { return output_data_; }

    void load_input() {
        if(config_.input_dir.empty()) return;
        std::filesystem::path path = std::filesystem::path(config_.input_dir) / "input.bin";
        input_data_ = TestData::load(path);
    }

    void begin_execute() {
        execute_start_ = std::chrono::steady_clock::now();
    }

    void end_execute() {
        execute_end_ = std::chrono::steady_clock::now();
    }

    double execute_time_ms() {
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(
            execute_end_ - execute_start_).count();
        return static_cast<double>(us) / 1000.0;
    }

    void next_pass(int& pass) {
        ++pass;
        if(pass == 1) begin_execute();      // warmup done -> start timer
        else if(pass == 2) end_execute();   // timed done   -> stop timer
    }

    void set_finish_hook(finish_hook_t hook) {
        finish_hook_ = std::move(hook);
    }

    void finish() {
        if(config_.output_dir.empty()) return;

        // 1. Persist the output map.
        try {
            output_data_.save(std::filesystem::path(config_.output_dir) / "output.bin");
        } catch(const std::exception& e) {
            std::cerr << "[runner] failed to write output.bin: " << e.what() << "\n";
        }

        // 2. Persist meta.bin (timing + parallel stats).
        Meta meta;
        meta.time_ms = execute_time_ms();
        if(finish_hook_) finish_hook_(meta);

        std::filesystem::path meta_path =
            std::filesystem::path(config_.output_dir) / "meta.bin";
        std::ofstream f(meta_path, std::ios::binary);
        if(f) {
            f.write(reinterpret_cast<const char*>(&meta), sizeof(Meta));
        }
    }

} // namespace runner

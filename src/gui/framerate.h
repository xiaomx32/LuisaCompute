//
// Created by Mike on 2021/11/30.
//

#pragma once

#include <core/dll_export.h>
#include <chrono>
#include <core/stl/vector.h>

namespace luisa::compute {

class LC_GUI_API Framerate {

public:
    using Clock = std::chrono::steady_clock;
    using Timepoint = Clock::time_point;

private:
    luisa::vector<double> _durations;
    luisa::vector<size_t> _frames;
    Timepoint _last;
    size_t _history_size;

public:
    explicit Framerate(size_t n = 5) noexcept;
    void clear() noexcept;
    void record(size_t frame_count = 1u) noexcept;
    [[nodiscard]] double duration() const noexcept;
    [[nodiscard]] double report() const noexcept;
};

}
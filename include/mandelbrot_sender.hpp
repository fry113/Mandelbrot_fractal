#pragma once

#include "mandelbrot_fractal_utils.hpp"
#include "types_sfml.hpp"
#include <print>

#include <stdexec/execution.hpp>

using namespace std::chrono_literals;
namespace ex = stdexec;

namespace mandelbrot {

static auto MakeComputeSender(RenderSettings settings, ViewPort viewport, FrameBuffer *fb) {
    static AvrTimeCounter time_counter;

    // функция для вычисления фрактала Мандельброта
    auto compute_mondelbrot = [settings, viewport](FrameBuffer *fb) -> FrameBuffer * {
        time_counter.Start();
        uint32_t height = fb->height;
        uint32_t width = fb->width;

        for (uint32_t y = 0; y < height; ++y) {
            size_t line = static_cast<size_t>(y) * width;
            for (uint32_t x = 0; x < width; ++x) {
                auto complex = Pixel2DToComplex(x, y, viewport, width, height);
                auto iterations = CalculateIterationsForPoint(complex, settings.max_iterations, settings.escape_radius);
                auto color = IterationsToColor(iterations, settings.max_iterations);
                size_t point_idx = (line + x) * FrameBuffer::BYTES_PER_PIXEL;

                fb->rgba[point_idx + 0] = color.r;
                fb->rgba[point_idx + 1] = color.g;
                fb->rgba[point_idx + 2] = color.b;
                fb->rgba[point_idx + 3] = 0xFF;
            }
        }
        return fb;
    };

    return ex::just(fb) | ex::then(compute_mondelbrot) | ex::then([](FrameBuffer *fb) {
               time_counter.End();
               if (time_counter.Count() % 10 == 0) {
                   std::println("\nAverage compute time: {} ms over {} frames", time_counter.GetAvr(),
                                time_counter.Count());
               }
               return fb;
           });
}

}  // namespace mandelbrot

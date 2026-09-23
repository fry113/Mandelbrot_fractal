#include <chrono>
#include <exception>
#include <memory>
#include <print>
#include <thread>
#include <utility>

#include <SFML/Graphics.hpp>

// для Linux надо использовать SFML/GLX из рабочих потоков (ASan ругается в X11 из контейнера)
#ifdef __linux__
#include <X11/Xlib.h>
#ifdef Complex
#undef Complex
#endif
#endif

#include <exec/any_sender_of.hpp>
#include <exec/repeat_effect_until.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

#include "mandelbrot_sender.hpp"
#include "sfml_display_sender.hpp"
#include "sfml_events_handler.hpp"
#include "types_sfml.hpp"

using namespace std::chrono_literals;
namespace ex = stdexec;

class WaitForFPS {
public:
    static constexpr unsigned int TARGET_FPS = 60;

    explicit WaitForFPS(FrameClock &frame_clock, unsigned int target_fps = TARGET_FPS)
        : frame_clock_(frame_clock), frame_time_(1s / target_fps) {}

    void operator()() {
        auto cur_frame_duration = frame_clock_.GetFrameTime();

        if (cur_frame_duration < frame_time_) {
            std::this_thread::sleep_for(frame_time_ - cur_frame_duration);
        }
        frame_clock_.Reset();
    }

private:
    FrameClock &frame_clock_;
    const std::chrono::nanoseconds frame_time_ = 1'000ns;
};

class MandelbrotApp {
public:
    MandelbrotApp() : compute_pool_{std::max(1u, std::thread::hardware_concurrency())}, sfml_thread_{1} {
        std::println("hardware_concurrency: {}\n", std::thread::hardware_concurrency());
    }

    void Run() {
        auto compute_sched = compute_pool_.get_scheduler();
        auto sfml_sched = sfml_thread_.get_scheduler();

        auto initialize = ex::on(sfml_sched, ex::just() | ex::then([this]() {
                                                 state_ = std::make_unique<SfmlState>(  //
                                                     RenderSettings{});  // RenderSettings{} подтянутся default settings
                                             }));
        ex::sync_wait(std::move(initialize));

        auto set_handler = [this]() {
            SfmlEventHandler sfml_handler{state_->window, state_->render_settings, state_->app_state};
            sfml_handler.GetHandle();
        };

        // clang-format off
        auto process_frame = ex::just() 
            | ex::continues_on(sfml_sched)
            | ex::then(set_handler) 
            | ex::continues_on(compute_sched) 
            | ex::let_value([this, sfml_sched]() -> AnySender {
                if (state_->app_state.should_exit || !state_->app_state.need_rerender) {
                    return AnySender{ex::just()};
                }
                state_->app_state.need_rerender = false;

                return AnySender{mandelbrot::MakeComputeSender(state_->render_settings, state_->app_state.viewport, &state_->fb) |
                                 ex::continues_on(sfml_sched) | render::MakeSfmlDisplaySender(*state_) |
                                 ex::then([](auto &&...) {})};
              })
            | ex::then(WaitForFPS{state_->frame_clock});
        // clang-format on

        auto repeated_pipeline = std::move(process_frame) | ex::then([this] { return state_->app_state.should_exit; }) |
                                 exec::repeat_effect_until();
        ex::sync_wait(std::move(repeated_pipeline));

        // все SFML/OpenGL ресурсы должны быть уничтожены в том же потоке, в котором они были созданы
        auto finalize = ex::on(sfml_sched, ex::just() | ex::then([this]() { state_.reset(); }));
        ex::sync_wait(std::move(finalize));
    }

private:  // types
    using FrameCompletionsSignature =
        ex::completion_signatures<ex::set_value_t(), ex::set_error_t(std::exception_ptr), ex::set_stopped_t()>;
    using AnySender = exec::any_receiver_ref<FrameCompletionsSignature>::any_sender<>;

private:
    std::unique_ptr<SfmlState> state_;

    exec::static_thread_pool compute_pool_;
    exec::static_thread_pool sfml_thread_;
};

int main() {
#ifdef __linux__
    // SFML/GLX используется из рабочих потоков, Xlib переведен в потокобезопасный режим
    // (иначе ASan ругается в X11 из контейнера)
    XInitThreads();
#endif

    std::println("=== Mandelbrot Fractal Renderer ===\n");
    std::println("Controls:");
    std::println("  Left Mouse Button  - Zoom In");
    std::println("  Right Mouse Button - Zoom Out");
    std::println("  X                  - Toggle Auto Zoom (infinite zoom to 'Seahorse Valley' point)");
    std::println("  C                  - Reset to Initial View");
    std::println("  Close Window       - Exit\n");

    try {
        MandelbrotApp app;
        app.Run();
    } catch (const std::exception &e) {
        std::println("Error: {}", e.what());
        return 1;
    }
    return 0;
}
#pragma once

#include <SFML/Graphics.hpp>
#include <stdexec/execution.hpp>

#include "types_core.hpp"

namespace ex = stdexec;

class SfmlEventHandler {
public:
    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        sf::RenderWindow &window_;
        RenderSettings render_settings_;
        AppState &state_;

        static constexpr float ZOOM_INTERVAL_MS = 100.0f;

        template <typename R>
        explicit OperationState(R &&r, sf::RenderWindow &window, RenderSettings render_settings, AppState &state)
            : receiver_{std::forward<R>(r)}, window_{window}, render_settings_{render_settings}, state_{state} {}

        void start() {
            try {
                HandleEvents();
                HandleAutoZoom();
                ex::set_value(std::move(receiver_));
            } catch (...) {
                ex::set_error(std::move(receiver_), std::current_exception());
            }
        }

        void getHandle() {
            HandleEvents();
            HandleAutoZoom();
        }

    private:
        void HandleEvents() {
            sf::Event event;
            while (window_.pollEvent(event)) {
                switch (event.type) {
                case sf::Event::Closed:
                    state_.should_exit = true;
                    break;
                case sf::Event::KeyPressed:
                    HandleKeyPress(event.key);
                    break;
                case sf::Event::MouseButtonPressed:
                    HandleMousePress(event.mouseButton);
                    break;
                case sf::Event::MouseButtonReleased:
                    HandleMouseRelease(event.mouseButton);
                    break;

                default:
                    break;
                }
            }
        }

        void HandleKeyPress(const sf::Event::KeyEvent &key) {
            // добавил для X11 проверку на Scancode и Keyboard
            const bool pressed_x = key.scancode == sf::Keyboard::Scancode::X || key.code == sf::Keyboard::Key::X;
            const bool pressed_c = key.scancode == sf::Keyboard::Scancode::C || key.code == sf::Keyboard::Key::C;
            const bool pressed_enter =
                key.scancode == sf::Keyboard::Scancode::Enter || key.code == sf::Keyboard::Key::Enter;

            // Toggle Auto Zoom (infinite zoom to 'Seahorse Valley' point)
            if (pressed_x) {
                state_.auto_zoom_enabled = !state_.auto_zoom_enabled;
                state_.need_rerender = true;
                state_.zoom_clock.restart();
                return;
            }

            // Reset to Initial View
            if (pressed_c) {
                state_.viewport = AppState::INITIAL_VIEWPORT;
                state_.auto_zoom_enabled = false;
                state_.need_rerender = true;
                return;
            }

            // для выхода из приложения при нажатии Enter (для ASan)
            if (pressed_enter) {
                state_.should_exit = true;
                return;
            }
        }

        void HandleMousePress(const sf::Event::MouseButtonEvent &mouse) {
            if (mouse.button == sf::Mouse::Left) {
                state_.left_mouse_pressed = true;
                ZoomToPoint(mouse.x, mouse.y, true);
            }
            if (mouse.button == sf::Mouse::Right) {
                state_.right_mouse_pressed = true;
                ZoomToPoint(mouse.x, mouse.y, false);
            }
        }

        void HandleMouseRelease(const sf::Event::MouseButtonEvent &mouse) {
            if (mouse.button == sf::Mouse::Left) {
                state_.left_mouse_pressed = false;
            } else if (mouse.button == sf::Mouse::Right) {
                state_.right_mouse_pressed = false;
            }
        }

        void HandleAutoZoom() {
            if (state_.auto_zoom_enabled && state_.zoom_clock.getElapsedTime().asMilliseconds() >= ZOOM_INTERVAL_MS) {
                // AutoZoomToPoint(true, 0.9);
                const auto pixel_x = static_cast<int>((AppState::AUTO_ZOOM_TARGET_X - state_.viewport.x_min) /
                                                      state_.viewport.width() * render_settings_.width);
                const auto pixel_y = static_cast<int>((AppState::AUTO_ZOOM_TARGET_Y - state_.viewport.y_min) /
                                                      state_.viewport.height() * render_settings_.height);
                ZoomToPoint(pixel_x, pixel_y, true);
            }
        }

        void ZoomToPoint(int pixel_x, int pixel_y, bool zoom_in, double factor = 0.8) {
            const double target_x = state_.viewport.x_min +
                                    (static_cast<double>(pixel_x) / render_settings_.width) * state_.viewport.width();
            const double target_y = state_.viewport.y_min +
                                    (static_cast<double>(pixel_y) / render_settings_.height) * state_.viewport.height();

            const double zoom_factor = zoom_in ? factor : (1.0 / factor);
            const double new_width = state_.viewport.width() * zoom_factor;
            const double new_height = state_.viewport.height() * zoom_factor;

            const double coef_x = (target_x - state_.viewport.x_min) / state_.viewport.width();
            const double coef_y = (target_y - state_.viewport.y_min) / state_.viewport.height();

            state_.viewport.x_min = target_x - new_width * coef_x;
            state_.viewport.y_min = target_y - new_height * coef_y;
            state_.viewport.x_max = state_.viewport.x_min + new_width;
            state_.viewport.y_max = state_.viewport.y_min + new_height;
            state_.need_rerender = true;
            state_.zoom_clock.restart();
        }
    };

    sf::RenderWindow &window_;
    RenderSettings render_settings_;
    AppState &state_;
    struct DummyReceiver {};

    SfmlEventHandler(sf::RenderWindow &window, RenderSettings render_settings, AppState &state)
        : window_{window}, render_settings_{render_settings}, state_{state} {}

    void getHandle() { OperationState<DummyReceiver>{DummyReceiver{}, window_, render_settings_, state_}.getHandle(); }

    template <typename Receiver>
    auto connect(Receiver &&receiver) {
        return OperationState<std::decay_t<Receiver>>{std::forward<Receiver>(receiver), window_, render_settings_,
                                                      state_};
    }

    auto get_completion_signatures() const {
        return ex::completion_signatures<ex::set_value_t(), ex::set_error_t(std::exception_ptr), ex::set_stopped_t()>{};
    }
};

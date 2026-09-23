#include <gtest/gtest.h>
#include <stdexec/execution.hpp>

#include <algorithm>
#include <exception>

#include "mandelbrot_sender.hpp"

struct TestComputeReceiver {
    using receiver_concept = ex::receiver_t;

    bool *value_set{};
    bool *error_set{};
    bool *stopped_set{};
    FrameBuffer *fb{};

    TestComputeReceiver(bool *value_set, bool *error_set, bool *stopped_set, FrameBuffer *fb)
        : value_set{value_set}, error_set{error_set}, stopped_set{stopped_set}, fb{fb} {}

    void set_value(FrameBuffer *result_fb) noexcept {
        if (value_set) {
            *value_set = true;
        }
    }

    void set_error(std::exception_ptr) noexcept {
        if (error_set) {
            *error_set = true;
        }
    }

    void set_stopped() noexcept {
        if (stopped_set) {
            *stopped_set = true;
        }
    }
};

TEST(FrameBuffer, MakeTest) {
    auto fb = FrameBuffer::Make(20, 50);

    EXPECT_EQ(fb.width, 20);
    EXPECT_EQ(fb.height, 50);
    EXPECT_EQ(fb.rgba.size(), 20 * 50 * FrameBuffer::BYTES_PER_PIXEL);
}

TEST(Pipelines, JustSenderTest) {
    RenderSettings settings{100, 100, 50, 2.0};
    bool value_set{false};
    bool error_set{false};
    bool stopped_set{false};

    auto fb = FrameBuffer::Make(settings.width, settings.height);
    auto sender = mandelbrot::MakeComputeSender(settings, AppState::INITIAL_VIEWPORT, &fb);
    auto op = sender.connect(std::move(TestComputeReceiver(&value_set, &error_set, &stopped_set, &fb)));
    op.start();

    EXPECT_TRUE(value_set);
    EXPECT_FALSE(error_set);
    EXPECT_FALSE(stopped_set);
}

TEST(Pipelines, MakeComputeTest) {
    RenderSettings settings{100, 100, 50, 2.0};
    bool value_set{false};
    bool error_set{false};
    bool stopped_set{false};

    auto fb = FrameBuffer::Make(settings.width, settings.height);
    auto sender = mandelbrot::MakeComputeSender(settings, AppState::INITIAL_VIEWPORT, &fb);
    auto op = sender.connect(std::move(TestComputeReceiver(&value_set, &error_set, &stopped_set, &fb)));
    op.start();

    EXPECT_EQ(fb.rgba.size(), settings.width * settings.height * FrameBuffer::BYTES_PER_PIXEL);
    for (size_t i = 0; i + FrameBuffer::BYTES_PER_PIXEL <= fb.rgba.size(); i += FrameBuffer::BYTES_PER_PIXEL) {
        EXPECT_EQ(fb.rgba[i + 3], 0xFF);
    }
}

TEST(Pipelines, ImageTest) {
    RenderSettings settings{100, 100, 50, 2.0};
    bool value_set{false};
    bool error_set{false};
    bool stopped_set{false};

    // clang-format off
    auto pipeline = ex::just() 
        | ex::then([&]() {
                auto fb = FrameBuffer::Make(settings.width, settings.height);
                auto sender = mandelbrot::MakeComputeSender(settings, AppState::INITIAL_VIEWPORT, &fb);
                auto op = sender.connect(std::move(TestComputeReceiver(&value_set, &error_set, &stopped_set, &fb)));
                op.start();
                return fb;
            }) 
        | ex::then([](FrameBuffer fb) {
                        bool has_colored_pixel =
                            std::any_of(fb.rgba.begin(), fb.rgba.end(), [](uint8_t value) { return value != 0x00 && value != 0xFF; });

                        EXPECT_TRUE(has_colored_pixel);
                        return fb.rgba.size();
                    });
    // clang-format on
    auto result = ex::sync_wait(std::move(pipeline));

    EXPECT_TRUE(value_set);
    EXPECT_FALSE(error_set);
    EXPECT_FALSE(stopped_set);
    ASSERT_TRUE(result.has_value());
    auto [pixel_count] = result.value();
    EXPECT_EQ(pixel_count, settings.width * settings.height * FrameBuffer::BYTES_PER_PIXEL);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

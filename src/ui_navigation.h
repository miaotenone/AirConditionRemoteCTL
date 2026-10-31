#pragma once
#include <stdint.h>

enum class ButtonPress { none, short_press, long_press };
enum class UiEvent { left, right, ok_short, ok_long };

// No Arduino or control dependencies: all time and sampled levels are explicit.
class ButtonState {
  public:
    void reset(bool pressed, uint32_t now_ms);
    ButtonPress sample(bool pressed, uint32_t now_ms, bool allow_long);

  private:
    bool raw_ = false, stable_ = false, ready_ = true;
    bool active_ = false, long_sent_ = false;
    uint32_t raw_since_ = 0, down_since_ = 0;
};

struct UiNavigation {
    uint8_t page = 0, detail = 0;
    bool automatic = true, diagnostics = false;
    void reset(uint32_t now_ms);
    void handle(UiEvent event, uint32_t now_ms);
    bool tick(uint32_t now_ms);
    void restart_timer(uint32_t now_ms);

  private:
    uint32_t last_switch_ = 0;
};

#include "ui_navigation.h"

static constexpr uint32_t DEBOUNCE_MS = 30;
static constexpr uint32_t LONG_PRESS_MS = 800;
static constexpr uint32_t PAGE_MS = 5000;

void ButtonState::reset(bool pressed, uint32_t now_ms) {
    raw_ = stable_ = pressed;
    ready_ = !pressed;
    active_ = long_sent_ = false;
    raw_since_ = down_since_ = now_ms;
}

ButtonPress ButtonState::sample(bool pressed, uint32_t now_ms, bool allow_long) {
    if (pressed != raw_) {
        raw_ = pressed;
        raw_since_ = now_ms;
    }
    if (raw_ != stable_ && (uint32_t)(now_ms - raw_since_) >= DEBOUNCE_MS) {
        stable_ = raw_;
        if (stable_) {
            down_since_ = now_ms;
            active_ = ready_;
            long_sent_ = false;
        } else {
            ready_ = true;
            if (active_) {
                active_ = false;
                if (long_sent_) return ButtonPress::none;
                // If a slow loop missed the held threshold, still never turn
                // a long OK into a short OK. Stop timing at the raw release.
                if (allow_long && (uint32_t)(raw_since_ - down_since_) >= LONG_PRESS_MS)
                    return ButtonPress::long_press;
                return ButtonPress::short_press;
            }
        }
    }
    if (stable_ && raw_ && active_ && allow_long && !long_sent_ &&
        (uint32_t)(now_ms - down_since_) >= LONG_PRESS_MS) {
        long_sent_ = true;
        return ButtonPress::long_press;
    }
    return ButtonPress::none;
}

void UiNavigation::reset(uint32_t now_ms) {
    page = detail = 0;
    automatic = true;
    diagnostics = false;
    restart_timer(now_ms);
}

void UiNavigation::restart_timer(uint32_t now_ms) {
    last_switch_ = now_ms;
}

void UiNavigation::handle(UiEvent event, uint32_t now_ms) {
    if (event == UiEvent::ok_long) {
        diagnostics = !diagnostics;
    } else if (event == UiEvent::ok_short) {
        if (!diagnostics) automatic = !automatic;
    } else {
        uint8_t& selected = diagnostics ? detail : page;
        const uint8_t count = diagnostics ? 5 : 4;
        selected = (uint8_t)((selected + (event == UiEvent::left ? count - 1 : 1)) % count);
        if (!diagnostics) automatic = false;
    }
    restart_timer(now_ms);
}

bool UiNavigation::tick(uint32_t now_ms) {
    if (!automatic || diagnostics || (uint32_t)(now_ms - last_switch_) < PAGE_MS) return false;
    page = (uint8_t)((page + 1) % 4);
    restart_timer(now_ms);
    return true;
}

#include <cstdio>
#include <cstdlib>
#include "ui_navigation.h"
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
int main() {
    ButtonState key;
    key.reset(false, 0);
    require(key.sample(true, 10, true) == ButtonPress::none, "raw press waits");
    require(key.sample(false, 20, true) == ButtonPress::none, "press bounce ignored");
    require(key.sample(true, 25, true) == ButtonPress::none, "new stable interval");
    require(key.sample(true, 54, true) == ButtonPress::none, "29ms not stable");
    require(key.sample(true, 55, true) == ButtonPress::none, "stable down no short yet");
    require(key.sample(false, 100, true) == ButtonPress::none, "release debounced");
    require(key.sample(true, 110, true) == ButtonPress::none, "release bounce");
    key.sample(false, 120, true);
    require(key.sample(false, 150, true) == ButtonPress::short_press, "one short on release");
    require(key.sample(false, 180, true) == ButtonPress::none, "no duplicate short");
    key.sample(true, 200, true);
    key.sample(true, 230, true);
    require(key.sample(true, 1029, true) == ButtonPress::none, "long threshold not early");
    require(key.sample(true, 1030, true) == ButtonPress::long_press, "one long at threshold");
    require(key.sample(true, 3000, true) == ButtonPress::none, "held long does not repeat");
    key.sample(false, 3010, true);
    require(key.sample(false, 3040, true) == ButtonPress::none, "long release suppresses short");
    key.reset(true, 0);
    require(key.sample(true, 5000, true) == ButtonPress::none, "startup held ignored");
    key.sample(false, 5010, true);
    require(key.sample(false, 5040, true) == ButtonPress::none, "startup release ignored");
    key.sample(true, 5100, false);
    key.sample(true, 5130, false);
    require(key.sample(true, 7000, false) == ButtonPress::none, "arrows have no hold repeat");
    key.sample(false, 7010, false);
    require(key.sample(false, 7040, false) == ButtonPress::short_press, "held arrow changes once");
    key.reset(false, UINT32_MAX - 100);
    key.sample(true, UINT32_MAX - 50, true);
    key.sample(true, UINT32_MAX - 20, true);
    require(key.sample(true, 779, true) == ButtonPress::long_press, "long survives millis wrap");
    key.reset(false, UINT32_MAX - 20);
    key.sample(true, UINT32_MAX - 10, true);
    key.sample(true, 19, true);
    key.sample(false, 100, true);
    require(key.sample(false, 130, true) == ButtonPress::short_press, "debounce survives wrap");
    key.reset(false, 0);
    key.sample(true, 10, true);
    key.sample(true, 40, true);
    key.sample(false, 850, true);
    require(key.sample(false, 880, true) == ButtonPress::long_press,
            "slow scan long release not short");
    UiNavigation nav;
    nav.reset(100);
    require(!nav.tick(5099), "auto interval complete before advance");
    require(nav.tick(5100) && nav.page == 1, "five second automatic advance");
    nav.handle(UiEvent::left, 5200);
    require(nav.page == 0 && !nav.automatic, "left fixes previous page");
    nav.handle(UiEvent::left, 5300);
    require(nav.page == 3, "left wraps");
    require(!nav.tick(50000) && nav.page == 3, "hold never advances");
    nav.handle(UiEvent::right, 50100);
    require(nav.page == 0, "right wraps");
    nav.handle(UiEvent::ok_short, 50200);
    require(nav.automatic && !nav.tick(55199), "resume starts a fresh five seconds");
    nav.handle(UiEvent::ok_long, 55200);
    require(nav.diagnostics && !nav.tick(100000), "diagnostics never auto advance");
    nav.handle(UiEvent::left, 100100);
    require(nav.detail == 4 && nav.page == 0, "detail wraps without changing overview");
    nav.handle(UiEvent::right, 100200);
    require(nav.detail == 0, "detail forward wraps");
    nav.handle(UiEvent::ok_short, 100300);
    require(nav.diagnostics && nav.automatic, "short ok in detail preserves return mode");
    nav.handle(UiEvent::ok_long, 100400);
    require(!nav.diagnostics && nav.page == 0 && nav.automatic, "exit restores overview");
    require(!nav.tick(105399) && nav.tick(105400), "exit resets auto timer");
    nav.handle(UiEvent::right, 105500);
    nav.handle(UiEvent::ok_long, 105600);
    nav.handle(UiEvent::ok_long, 105700);
    require(!nav.automatic && nav.page == 2, "detail preserves held overview");
    nav.reset(UINT32_MAX - 1000);
    require(!nav.tick(3998) && nav.tick(3999), "auto survives wrap");
    std::puts("PASS: button debounce, long press, startup guard, navigation and wrap");
}

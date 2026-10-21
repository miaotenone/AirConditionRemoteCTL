#include "link_guard.h"

enum LinkGuardAction link_guard_action(const LinkGuardInput& in) {
    unsigned long silent_ms = in.now_ms - in.last_healthy_ms;   // unsigned on purpose

    if (silent_ms < in.redial_after_ms) return LINK_GUARD_IDLE;

    // Checked before the dial spacing: once we are past the deadline the
    // dials have had their chance, whether or not one just went out.
    if (silent_ms >= in.reboot_after_ms) return LINK_GUARD_REBOOT;

    // 0 means "never dialled", which is also the correct answer the first time
    // round -- and unlike (now - 0) it does not have to reason about millis()
    // having wrapped since boot.
    if (in.last_redial_ms == 0) return LINK_GUARD_REDIAL;
    if (in.now_ms - in.last_redial_ms < in.redial_every_ms) return LINK_GUARD_IDLE;

    return LINK_GUARD_REDIAL;
}

#include "failsafe.h"

FailsafeAction failsafe_action(const FailsafeStatus& status, bool healthy, uint32_t now_ms,
                               uint32_t timeout_ms) {
    if (healthy) {
        if (!status.armed) return FailsafeAction::arm;
        if (status.tripped) return FailsafeAction::restore;
        return FailsafeAction::none;
    }
    if (!status.armed || status.tripped) return FailsafeAction::none;
    if (uint32_t(now_ms - status.last_contact_ms) < timeout_ms) return FailsafeAction::none;
    return FailsafeAction::turn_off;
}

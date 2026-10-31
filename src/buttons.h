#pragma once

// Initialize only after normal setup; boot/config/self-test keep screen ownership.
void buttons_begin();
void buttons_loop();

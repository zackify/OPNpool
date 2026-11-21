/**
 * @file opnpool_boost_switch.cpp
 * @brief Salt cell boost (super chlorinate) switch. See opnpool_boost_switch.h.
 */

#include <esphome/core/log.h>

#include "opnpool_boost_switch.h"
#include "opnpool_number.h"
#include "core/opnpool.h"
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wunused-parameter"

namespace esphome {
namespace opnpool {

constexpr char TAG[] = "opnpool_boost_switch";

void
OpnPoolBoostSwitch::dump_config()
{
    LOG_SWITCH("  ", "Boost Switch", this);
    ESP_LOGCONFIG(TAG, "    Last value: %s", last_.valid ? (last_.value ? "ON" : "OFF") : "Unknown");
}

void
OpnPoolBoostSwitch::write_state(bool value)
{
    if (!this->parent_) { ESP_LOGW(TAG, "Parent unknown"); return; }
    int const boost_byte = value ? (0x80 | this->parent_->get_boost_pending_hours()) : 0;
    opnpool_send_chlor_set(this->parent_, -1, boost_byte);
    // state is published from the controller's readback
}

void
OpnPoolBoostSwitch::publish_value_if_changed(bool const new_value)
{
    if (!last_.valid || last_.value != new_value) {
        this->publish_state(new_value);
        last_ = { .valid = true, .value = new_value };
    }
}

}  // namespace opnpool
}  // namespace esphome

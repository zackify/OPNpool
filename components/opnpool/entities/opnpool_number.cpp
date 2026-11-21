/**
 * @file opnpool_number.cpp
 * @brief Number entities (editable values) for the OPNpool component. See opnpool_number.h.
 */

#include <cmath>

#include <esp_system.h>
#include <esphome/core/log.h>

#include "opnpool_number.h"
#include "core/opnpool.h"
#include "ipc/ipc.h"
#include "pool_task/network_msg.h"
#include "core/opnpool_ids.h"
#include "utils/enum_helpers.h"
#include "core/poolstate.h"
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wunused-parameter"

namespace esphome {
namespace opnpool {

constexpr char TAG[] = "opnpool_number";

void
OpnPoolNumber::dump_config()
{
    LOG_NUMBER("  ", "Number", this);
    ESP_LOGCONFIG(TAG, "    ID: %u", static_cast<uint8_t>(id_));
    ESP_LOGCONFIG(TAG, "    Last value: %s", last_.valid ? std::to_string(last_.value).c_str() : "Unknown");
}

void
OpnPoolNumber::control(float value)
{
    switch (id_) {
        case number_id_t::CHLORINATOR_LEVEL_SET:
            this->control_chlorinator_level(value);
            break;
        case number_id_t::CHLORINATOR_BOOST_HOURS:
            this->control_chlorinator_boost_hours(value);
            break;
    }
}

void
opnpool_send_chlor_set(OpnPool * const parent, int const pool_pct, int const boost_byte)
{
    if (!parent || !parent->get_ipc()) { ESP_LOGW(TAG, "IPC unavailable"); return; }

    PoolState * const state_class_ptr = parent->get_opnpool_state();
    if (!state_class_ptr) { ESP_LOGW(TAG, "Pool state unknown"); return; }

    poolstate_t state;
    state_class_ptr->get(&state);

    datalink_addr_t const controller_addr = state.system.addr.value;
    if (!state.system.addr.valid || !controller_addr.is_controller()) {
        ESP_LOGW(TAG, "Controller address still unknown, cannot set chlorinator");
        return;
    }
    if (!state.chlor.setpt_spa.valid || !state.chlor.setpt_pool.valid) {
        ESP_LOGW(TAG, "Salt cell setpoint not read back from the controller yet, try again in a minute");
        return;
    }

    uint8_t const pool = pool_pct >= 0 ? static_cast<uint8_t>(pool_pct) : state.chlor.setpt_pool.value;
    uint8_t const cur_boost = (state.chlor.boost_hours.valid && state.chlor.boost_hours.value > 0)
                            ? static_cast<uint8_t>(0x80 | state.chlor.boost_hours.value) : uint8_t{0};
    uint8_t const boost = boost_byte >= 0 ? static_cast<uint8_t>(boost_byte) : cur_boost;

    ESP_LOGI(TAG, "Setting salt cell: pool=%u%% boost=0x%02X (spa byte 0x%02X unchanged)", pool, boost, state.chlor.setpt_spa.value);

    network_msg_t msg = {};
    msg.src = datalink_addr_t::easytouch_controller();
    msg.dst = controller_addr;
    msg.typ = network_msg_typ_t::CTRL_CHLOR_SET;
    msg.u.a5 = {
        .ctrl_chlor_set = {
            .spa = state.chlor.setpt_spa.value,
            .pool = pool,
            .boost = boost,
            .unknown = 0
        }
    };

    if (ipc_send_network_msg_to_pool_task(&msg, parent->get_ipc()) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send CHLOR_SET message to pool task");
    }
    // don't publish here: the controller's readback (action 0x19) publishes the confirmed value
}

/**
 * @brief Sets the salt cell pool output percent (same command as the remote); keeps a running boost unchanged.
 */
void
OpnPoolNumber::control_chlorinator_level(float value)
{
    if (std::isnan(value)) { return; }
    float const min_value = this->traits.get_min_value();
    float const max_value = this->traits.get_max_value();
    if (value < min_value) value = min_value;
    if (value > max_value) value = max_value;
    opnpool_send_chlor_set(this->parent_, static_cast<int>(std::lround(value)), -1);
}

/**
 * @brief Boost run hours (1..24). While boost is on this changes the remaining hours (as the remote does);
 * while off it only remembers the hours the next boost will start with.
 */
void
OpnPoolNumber::control_chlorinator_boost_hours(float value)
{
    if (std::isnan(value)) { return; }
    float const min_value = this->traits.get_min_value();
    float const max_value = this->traits.get_max_value();
    if (value < min_value) value = min_value;
    if (value > max_value) value = max_value;
    uint8_t const hours = static_cast<uint8_t>(std::lround(value));

    PoolState * const state_class_ptr = parent_ ? parent_->get_opnpool_state() : nullptr;
    bool boost_on = false;
    if (state_class_ptr) {
        poolstate_t state;
        state_class_ptr->get(&state);
        boost_on = state.chlor.boost_hours.valid && state.chlor.boost_hours.value > 0;
    }
    parent_->set_boost_pending_hours(hours);
    if (boost_on) {
        opnpool_send_chlor_set(this->parent_, -1, 0x80 | hours);  // readback publishes the new remaining hours
    } else {
        this->publish_value_if_changed(static_cast<float>(hours));
    }
}

void
OpnPoolNumber::publish_value_if_changed(float value)
{
    if (!last_.valid || last_.value != value) {
        this->publish_state(value);
        last_ = { .valid = true, .value = value };
    }
}

}  // namespace opnpool
}  // namespace esphome

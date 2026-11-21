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
    }
}

/**
 * @brief Sets the salt cell pool output percent by sending the controller the same command as the remote.
 *
 * Payload is [spa byte, pool %, 0, 0]. The spa byte is echoed from the controller's last readback so only
 * the pool percent changes. If we haven't seen a readback yet, nothing is sent (a request is already sent
 * periodically), so we never write a guessed spa byte.
 */
void
OpnPoolNumber::control_chlorinator_level(float value)
{
    if (!this->parent_ || !this->parent_->get_ipc()) { ESP_LOGW(TAG, "IPC unavailable"); return; }

    PoolState * const state_class_ptr = parent_->get_opnpool_state();
    if (!state_class_ptr) { ESP_LOGW(TAG, "Pool state unknown"); return; }

    poolstate_t state;
    state_class_ptr->get(&state);

    datalink_addr_t const controller_addr = state.system.addr.value;
    if (!state.system.addr.valid || !controller_addr.is_controller()) {
        ESP_LOGW(TAG, "Controller address still unknown, cannot set chlorinator level");
        return;
    }
    if (!state.chlor.setpt_spa.valid) {
        ESP_LOGW(TAG, "Salt cell setpoint not read back from the controller yet, try again in a minute");
        return;
    }
    if (std::isnan(value)) { return; }

    float const min_value = this->traits.get_min_value();
    float const max_value = this->traits.get_max_value();
    if (value < min_value) value = min_value;
    if (value > max_value) value = max_value;
    uint8_t const pool_pct = static_cast<uint8_t>(std::lround(value));

    ESP_LOGI(TAG, "Setting salt cell pool output to %u%% (spa byte 0x%02X unchanged)", pool_pct, state.chlor.setpt_spa.value);

    network_msg_t msg = {};
    msg.src = datalink_addr_t::easytouch_controller();
    msg.dst = controller_addr;
    msg.typ = network_msg_typ_t::CTRL_CHLOR_SET;
    msg.u.a5 = {
        .ctrl_chlor_set = {
            .spa = state.chlor.setpt_spa.value,
            .pool = pool_pct,
            .unknown = {0, 0}
        }
    };

    if (ipc_send_network_msg_to_pool_task(&msg, this->parent_->get_ipc()) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send CHLOR_SET message to pool task");
    }
    // don't publish here: the controller's readback (action 0x19) publishes the confirmed value
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

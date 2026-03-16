/**
 * @file opnpool_number.cpp
 * @brief Number entity implementation for OPNpool component.
 *
 * @details
 * This file implements the number entity interface for the OPNpool component, enabling
 * control of variable speed pump RPM from Home Assistant. It provides methods
 * to handle number value changes initiated by Home Assistant, constructs and sends
 * protocol messages to the pump over RS-485, and updates the state
 * based on pump feedback.
 *
 * @author Zach Silveira
 * @copyright Copyright (c) 2026
 * @license SPDX-License-Identifier: GPL-3.0-or-later
 */

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
    if (!this->parent_) { ESP_LOGW(TAG, "Parent unknown"); return; }

    PoolState * const state_class_ptr = parent_->get_opnpool_state();
    if (!state_class_ptr) { ESP_LOGW(TAG, "Pool state unknown"); return; }

    poolstate_t state;
    state_class_ptr->get(&state);

    // Clamp value to valid range (1000-3450 RPM)
    uint16_t rpm = static_cast<uint16_t>(value);
    if (rpm < 1000) rpm = 1000;
    if (rpm > 3450) rpm = 3450;

    ESP_LOGI(TAG, "Setting pump speed to %u RPM", rpm);

    network_msg_t msg;
    msg.src = datalink_addr_t::remote();
    msg.dst = datalink_addr_t::pump(datalink_pump_id_t::PRIMARY);
    msg.typ = network_msg_typ_t::PUMP_REG_SET;
    msg.u.a5 = {
        .pump_reg_set = {
            .address = network_pump_reg_addr_t::RPM,
            .operation = {network_pump_reg_operation_t::WRITE},
            .value = {
                .high = static_cast<uint8_t>((rpm >> 8) & 0xFF),
                .low = static_cast<uint8_t>(rpm & 0xFF)
            }
        }
    };

    ESP_LOGVV(TAG, "Sending PUMP_REG_SET command: addr=0x%02X, op=0x%02X, value=%u",
              static_cast<uint8_t>(msg.u.a5.pump_reg_set.address),
              msg.u.a5.pump_reg_set.operation.raw,
              msg.u.a5.pump_reg_set.value.to_uint16());

    if (ipc_send_network_msg_to_pool_task(&msg, this->parent_->get_ipc()) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send PUMP_REG_SET message to pool task");
    }
}

void
OpnPoolNumber::publish_value_if_changed(float value)
{
    if (!last_.valid || last_.value != value) {
        this->publish_state(value);

        last_ = {
            .valid = true,
            .value = value
        };

        ESP_LOGV(TAG, "Published pump speed: %.0f RPM", value);
    }
}

}  // namespace opnpool
}  // namespace esphome

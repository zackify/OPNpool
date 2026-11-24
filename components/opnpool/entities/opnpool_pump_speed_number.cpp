#include <cmath>

#include <esp_system.h>
#include <esphome/core/log.h>

#include "opnpool_pump_speed_number.h"
#include "core/opnpool.h"
#include "core/poolstate.h"
#include "ipc/ipc.h"
#include "pool_task/network_msg.h"
#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"
#pragma GCC diagnostic error "-Wunused-parameter"

namespace esphome {
namespace opnpool {

constexpr char TAG[] = "opnpool_pump";

uint16_t
OpnPoolPumpSpeedNumber::pct_to_rpm(float pct)
{
    if (pct < PCT_MIN) pct = PCT_MIN;
    if (pct > PCT_MAX) pct = PCT_MAX;
    return static_cast<uint16_t>(std::lround(RPM_MIN + (pct - PCT_MIN) * (RPM_MAX - RPM_MIN) / (PCT_MAX - PCT_MIN)));
}

float
OpnPoolPumpSpeedNumber::rpm_to_pct(uint16_t const rpm)
{
    float pct = PCT_MIN + (static_cast<float>(rpm) - RPM_MIN) * (PCT_MAX - PCT_MIN) / (RPM_MAX - RPM_MIN);
    pct = std::round(pct / 5.0f) * 5.0f;  // the pump only moves in 5 % steps
    if (pct < PCT_MIN) pct = PCT_MIN;
    if (pct > PCT_MAX) pct = PCT_MAX;
    return pct;
}

void
OpnPoolPumpSpeedNumber::dump_config()
{
    LOG_NUMBER("  ", "Pump speed", this);
    ESP_LOGCONFIG(TAG, "    Circuit: %u", circuit_);
}

void
OpnPoolPumpSpeedNumber::publish_rpm_if_changed(uint16_t const rpm)
{
    if (!last_valid_ || last_rpm_ != rpm) {
        last_valid_ = true;
        last_rpm_ = rpm;
        this->publish_state(rpm_to_pct(rpm));
    }
}

void
OpnPoolPumpSpeedNumber::control(float value)
{
    if (std::isnan(value)) { return; }
    if (!parent_ || !parent_->get_ipc()) { ESP_LOGW(TAG, "IPC unavailable"); return; }

    PoolState * const state_class_ptr = parent_->get_opnpool_state();
    if (!state_class_ptr) { ESP_LOGW(TAG, "Pool state unknown"); return; }
    poolstate_t state;
    state_class_ptr->get(&state);

    datalink_addr_t const controller_addr = state.system.addr.value;
    if (!state.system.addr.valid || !controller_addr.is_controller()) {
        ESP_LOGW(TAG, "Controller address still unknown, cannot set pump speed");
        return;
    }
    if (!state.pump_cfg.valid) {
        ESP_LOGW(TAG, "Pump config not read back from the controller yet, try again in a minute");
        return;
    }

    network_ctrl_pump_cfg_t cfg = state.pump_cfg.value;
    int slot = -1;
    for (int ii = 0; ii < 8; ii++) {
        if (cfg.slot[ii].circuit == circuit_) { slot = ii; break; }
    }
    if (slot < 0) {
        ESP_LOGW(TAG, "Circuit %u is not assigned to pump 1 on the controller; assign it with the remote first", circuit_);
        return;
    }

    uint16_t const rpm = pct_to_rpm(value);
    uint16_t const old_rpm = static_cast<uint16_t>(cfg.slot[slot].rpm_hi << 8 | cfg.rpm_lo[slot]);
    cfg.slot[slot].rpm_hi = static_cast<uint8_t>(rpm >> 8);
    cfg.rpm_lo[slot] = static_cast<uint8_t>(rpm & 0xFF);
    ESP_LOGI(TAG, "Setting circuit %u (slot %d): %.0f%% = %u RPM (was %u RPM)", circuit_, slot + 1, value, rpm, old_rpm);

    network_msg_t msg = {};
    msg.src = datalink_addr_t::easytouch_controller();
    msg.dst = controller_addr;
    msg.typ = network_msg_typ_t::CTRL_PUMP_CFG_SET;
    msg.u.a5.ctrl_pump_cfg = cfg;

    if (ipc_send_network_msg_to_pool_task(&msg, parent_->get_ipc()) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send PUMP_CFG_SET message to pool task");
    }
}

}  // namespace opnpool
}  // namespace esphome

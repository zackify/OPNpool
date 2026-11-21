/**
 * @file opnpool_number.h
 * @brief Number entities (editable values) for the OPNpool component.
 *
 * Currently one number: the salt cell pool output percent. Setting it sends the same A5 command
 * (action 0x99) that the pool remote sends; the pool controller stores the value, so a single
 * command per change is enough. State comes back from the controller's own readback (action 0x19).
 */

#pragma once

#include <esp_system.h>
#include <esp_types.h>
#include <esphome/core/component.h>
#include <esphome/components/number/number.h>

#include "core/opnpool_ids.h"

namespace esphome {
namespace opnpool {

class OpnPool;

/**
 * @brief Sends the salt cell command (action 0x99) to the controller, same as the remote.
 * @param pool_pct   New pool output percent, or -1 to keep the controller's current value.
 * @param boost_byte New boost byte (0 = off, 0x80|hours = on), or -1 to keep the current state.
 * Nothing is sent until the controller's salt cell readback has been seen (so the spa byte is never guessed).
 */
void opnpool_send_chlor_set(OpnPool * parent, int pool_pct, int boost_byte);

class OpnPoolNumber : public number::Number, public Component {
  public:
    OpnPoolNumber(OpnPool * parent, uint8_t id) : parent_{parent}, id_{static_cast<number_id_t>(id)} {}

    void dump_config();

    /// @brief Publishes a value to Home Assistant if it changed.
    void publish_value_if_changed(float const new_value);

  protected:
    /// @brief Called by ESPHome when the user sets a new value in Home Assistant.
    void control(float value) override;

    void control_chlorinator_level(float value);
    void control_chlorinator_boost_hours(float value);

    OpnPool * const   parent_;
    number_id_t const id_;

    struct last_t {
        bool  valid;
        float value;
    } last_ = { .valid = false, .value = 0.0f };
};

}  // namespace opnpool
}  // namespace esphome

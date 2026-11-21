/**
 * @file opnpool_boost_switch.h
 * @brief Salt cell boost (super chlorinate) switch for the OPNpool component.
 *
 * Turning it on sends the same A5 command (action 0x99) the remote sends, with the boost byte
 * 0x80 | hours (hours = the "Chlorinator Boost Hours" number, 1..24). Turning it off sends boost byte 0.
 * State comes back from the controller's readback (hours remaining > 0 means on).
 */

#pragma once

#include <esp_system.h>
#include <esp_types.h>
#include <esphome/core/component.h>
#include <esphome/components/switch/switch.h>

namespace esphome {
namespace opnpool {

class OpnPool;

class OpnPoolBoostSwitch : public switch_::Switch, public Component {
  public:
    explicit OpnPoolBoostSwitch(OpnPool * parent) : parent_{parent} {}

    void dump_config();
    void write_state(bool state) override;
    void publish_value_if_changed(bool const new_value);

  protected:
    OpnPool * const parent_;
    struct last_t { bool valid; bool value; } last_ = { .valid = false, .value = false };
};

}  // namespace opnpool
}  // namespace esphome

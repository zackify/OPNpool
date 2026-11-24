#pragma once

#include <esp_system.h>
#include <esp_types.h>
#include <esphome/core/component.h>
#include <esphome/components/number/number.h>

namespace esphome {
namespace opnpool {

class OpnPool;

/// Pump 1 speed for one Pentair circuit, shown in HA as the pump's 30..120 % scale (5 % steps) and sent to the
/// controller as RPM (30 % = 1000 RPM .. 120 % = 3450 RPM, linear). Uses the same pump config message (0x98) as the
/// remote, built from the controller's last readback (0x18) so the other circuits' speeds are left unchanged.
class OpnPoolPumpSpeedNumber : public number::Number, public Component {
  public:
    OpnPoolPumpSpeedNumber(OpnPool * parent, uint8_t circuit) : parent_{parent}, circuit_{circuit} {}

    void dump_config() override;

    uint8_t get_circuit() const { return circuit_; }
    void publish_rpm_if_changed(uint16_t const rpm);

    static constexpr float    PCT_MIN = 30.0f;
    static constexpr float    PCT_MAX = 120.0f;
    static constexpr uint16_t RPM_MIN = 1000;
    static constexpr uint16_t RPM_MAX = 3450;
    static uint16_t pct_to_rpm(float const pct);
    static float    rpm_to_pct(uint16_t const rpm);

  protected:
    void control(float value) override;

    OpnPool * const parent_;
    uint8_t const   circuit_;
    bool            last_valid_{false};
    uint16_t        last_rpm_{0};
};

}  // namespace opnpool
}  // namespace esphome

/**
 * @file opnpool_number.h
 * @brief Number entity interface for OPNpool component.
 *
 * @details
 * Declares the OpnPoolNumber class, which implements ESPHome's number interface
 * for controlling variable speed pump RPM settings.
 *
 * @author Zach Silveira
 * @copyright Copyright (c) 2026
 * @license SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <esp_system.h>
#include <esp_types.h>
#include <esphome/core/component.h>
#include <esphome/components/number/number.h>

#include "core/opnpool_ids.h"

namespace esphome {
namespace opnpool {

// Forward declarations
struct poolstate_t;
class OpnPool;

/**
 * @brief Number entity for OPNpool component.
 *
 * @details
 * Extends ESPHome's Number and Component classes to provide control over pump
 * speed (RPM). Sends commands to the pool pump via RS-485 and publishes
 * state updates only when the pump confirms the change.
 */
class OpnPoolNumber : public number::Number, public Component {
  public:
    /**
     * @brief Constructs an OpnPoolNumber entity.
     *
     * @param[in] parent Pointer to the parent OpnPool component.
     * @param[in] id     The number entity ID from number_id_t.
     */
    OpnPoolNumber(OpnPool* parent, uint8_t id) : parent_{parent}, id_{static_cast<number_id_t>(id)} {}

    /**
     * @brief Dumps the configuration and last known state of the number.
     *
     * Called by ESPHome during startup. Set logger for this module to INFO or
     * higher to see output.
     */
    void dump_config();

    /**
     * @brief Publishes the number value to Home Assistant if it has changed.
     *
     * @param[in] new_value The new RPM value.
     */
    void publish_value_if_changed(float const new_value);

  protected:
    /**
     * @brief Handles number value changes triggered by Home Assistant.
     *
     * @param[in] value The desired RPM value (1000-3450).
     */
    void control(float value) override;

    OpnPool * const              parent_;   ///< Parent OpnPool component.
    number_id_t const            id_;       ///< Number entity ID.

    /// @brief Tracks the last published value to avoid redundant updates.
    struct last_t {
        bool valid;     ///< True if a value has been published at least once.
        float value;    ///< The last published value.
    } last_ = {
        .valid = false,
        .value = 0.0f
    };
};

}  // namespace opnpool
}  // namespace esphome

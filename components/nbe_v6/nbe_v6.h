#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

#include <string>
#include <vector>
#include <queue>

namespace esphome {

namespace sensor {
class Sensor;
}
namespace binary_sensor {
class BinarySensor;
}
namespace text_sensor {
class TextSensor;
}
namespace switch_ {
class Switch;
}
namespace number {
class Number;
}
namespace button {
class Button;
}

namespace nbe_v6 {

enum State {
  STATE_IDLE,
  STATE_WAITING_RESPONSE,
};

enum PollFrameType {
  FRAME_NONE,
  FRAME_Z00,
  FRAME_Z01,
  FRAME_Z02,
  FRAME_Z03,
  FRAME_Z04,
  FRAME_Z06,
  FRAME_COMMAND,
};

struct QueuedCommand {
  std::string command;
  std::string description;
};

class NBEV6Component : public PollingComponent, public uart::UARTDevice {
 public:
  NBEV6Component() = default;

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  // Sensor setters
  void set_boiler_temperature_sensor(sensor::Sensor *s) { boiler_temp_sensor_ = s; }
  void set_chute_temperature_sensor(sensor::Sensor *s) { chute_temp_sensor_ = s; }
  void set_smoke_temperature_sensor(sensor::Sensor *s) { smoke_temp_sensor_ = s; }
  void set_boiler_return_temperature_sensor(sensor::Sensor *s) { boiler_return_temp_sensor_ = s; }
  void set_hot_water_temperature_sensor(sensor::Sensor *s) { hot_water_temp_sensor_ = s; }
  void set_outdoor_temperature_sensor(sensor::Sensor *s) { outdoor_temp_sensor_ = s; }
  void set_power_percent_sensor(sensor::Sensor *s) { power_percent_sensor_ = s; }
  void set_power_kw_sensor(sensor::Sensor *s) { power_kw_sensor_ = s; }
  void set_oxygen_sensor(sensor::Sensor *s) { oxygen_sensor_ = s; }
  void set_oxygen_desired_sensor(sensor::Sensor *s) { oxygen_desired_sensor_ = s; }
  void set_light_sensor(sensor::Sensor *s) { light_sensor_ = s; }
  void set_feeder_time_sensor(sensor::Sensor *s) { feeder_time_sensor_ = s; }
  void set_ignition_time_sensor(sensor::Sensor *s) { ignition_time_sensor_ = s; }
  void set_motor_runtime_hours_sensor(sensor::Sensor *s) { motor_runtime_hours_sensor_ = s; }
  void set_ignition_runtime_hours_sensor(sensor::Sensor *s) { ignition_runtime_hours_sensor_ = s; }
  void set_target_boiler_temp_sensor(sensor::Sensor *s) { target_boiler_temp_sensor_ = s; }

  // Text sensors
  void set_mode_text_sensor(text_sensor::TextSensor *s) { mode_text_sensor_ = s; }
  void set_alarm_text_sensor(text_sensor::TextSensor *s) { alarm_text_sensor_ = s; }
  void set_version_text_sensor(text_sensor::TextSensor *s) { version_text_sensor_ = s; }

  // Binary sensors
  void set_alarm_active_sensor(binary_sensor::BinarySensor *s) { alarm_active_sensor_ = s; }
  void set_fire_detected_sensor(binary_sensor::BinarySensor *s) { fire_detected_sensor_ = s; }

  // Switches
  void set_burner_switch(switch_::Switch *s) { burner_switch_ = s; }
  void set_autocalculation_switch(switch_::Switch *s) { autocalculation_switch_ = s; }

  // Numbers (Setpoints & Combustion Parameters)
  void set_boiler_temp_set_number(number::Number *n) { boiler_temp_set_number_ = n; }
  void set_feeder_capacity_number(number::Number *n) { feeder_capacity_number_ = n; }
  void set_blower_low_number(number::Number *n) { blower_low_number_ = n; }
  void set_blower_mid_number(number::Number *n) { blower_mid_number_ = n; }
  void set_blower_high_number(number::Number *n) { blower_high_number_ = n; }
  void set_feeder_low_number(number::Number *n) { feeder_low_number_ = n; }
  void set_feeder_high_number(number::Number *n) { feeder_high_number_ = n; }
  void set_feed_per_minute_number(number::Number *n) { feed_per_minute_number_ = n; }
  void set_min_power_number(number::Number *n) { min_power_number_ = n; }
  void set_max_power_number(number::Number *n) { max_power_number_ = n; }

  // Control actions
  void turn_burner_on();
  void turn_burner_off();
  void reset_alarm();
  void reset_ignition();
  void set_boiler_temp(float temp);
  void set_feeder_capacity(float capacity);
  void set_blower_low(float val);
  void set_blower_mid(float val);
  void set_blower_high(float val);
  void set_feeder_low(float val);
  void set_feeder_high(float val);
  void set_feed_per_minute(float val);
  void set_min_power(float val);
  void set_max_power(float val);
  void set_autocalculation(bool state);

  static uint8_t calc_checksum(const std::string &data);
  static bool verify_checksum(const std::string &data);
  static std::string add_checksum(const std::string &data);

 protected:
  void send_raw_frame(const std::string &frame, PollFrameType type, size_t expected_length, const std::string &desc);
  void queue_command(const std::string &cmd, const std::string &desc);
  void process_incoming_byte(uint8_t byte);
  void handle_completed_frame();
  void parse_frame_z00(const std::string &frame);
  void parse_frame_z01(const std::string &frame);
  void parse_frame_z02(const std::string &frame);
  void parse_frame_z03(const std::string &frame);
  void parse_frame_z04(const std::string &frame);
  void parse_frame_z06(const std::string &frame);
  void parse_command_ack(const std::string &frame);

  static std::string trim(const std::string &str);
  static bool parse_int_field(const std::string &frame, size_t offset, size_t len, int &val);
  static bool parse_float_field(const std::string &frame, size_t offset, size_t len, int decimals, float &val);

  State state_{STATE_IDLE};
  PollFrameType current_frame_type_{FRAME_NONE};
  std::string current_desc_{""};
  size_t expected_response_len_{0};
  uint32_t send_time_{0};
  uint32_t last_poll_time_{0};
  uint8_t retry_count_{0};

  std::string rx_buffer_{};
  std::queue<QueuedCommand> command_queue_{};

  uint32_t poll_cycle_{0};
  bool version_requested_{false};

  // Sensor pointers
  sensor::Sensor *boiler_temp_sensor_{nullptr};
  sensor::Sensor *chute_temp_sensor_{nullptr};
  sensor::Sensor *smoke_temp_sensor_{nullptr};
  sensor::Sensor *boiler_return_temp_sensor_{nullptr};
  sensor::Sensor *hot_water_temp_sensor_{nullptr};
  sensor::Sensor *outdoor_temp_sensor_{nullptr};
  sensor::Sensor *power_percent_sensor_{nullptr};
  sensor::Sensor *power_kw_sensor_{nullptr};
  sensor::Sensor *oxygen_sensor_{nullptr};
  sensor::Sensor *oxygen_desired_sensor_{nullptr};
  sensor::Sensor *light_sensor_{nullptr};
  sensor::Sensor *feeder_time_sensor_{nullptr};
  sensor::Sensor *ignition_time_sensor_{nullptr};
  sensor::Sensor *motor_runtime_hours_sensor_{nullptr};
  sensor::Sensor *ignition_runtime_hours_sensor_{nullptr};
  sensor::Sensor *target_boiler_temp_sensor_{nullptr};

  text_sensor::TextSensor *mode_text_sensor_{nullptr};
  text_sensor::TextSensor *alarm_text_sensor_{nullptr};
  text_sensor::TextSensor *version_text_sensor_{nullptr};

  binary_sensor::BinarySensor *alarm_active_sensor_{nullptr};
  binary_sensor::BinarySensor *fire_detected_sensor_{nullptr};

  switch_::Switch *burner_switch_{nullptr};
  switch_::Switch *autocalculation_switch_{nullptr};

  number::Number *boiler_temp_set_number_{nullptr};
  number::Number *feeder_capacity_number_{nullptr};
  number::Number *blower_low_number_{nullptr};
  number::Number *blower_mid_number_{nullptr};
  number::Number *blower_high_number_{nullptr};
  number::Number *feeder_low_number_{nullptr};
  number::Number *feeder_high_number_{nullptr};
  number::Number *feed_per_minute_number_{nullptr};
  number::Number *min_power_number_{nullptr};
  number::Number *max_power_number_{nullptr};
};

}  // namespace nbe_v6
}  // namespace esphome

#include "nbe_v6_switch.h"
#include "nbe_v6_number.h"
#include "nbe_v6_button.h"

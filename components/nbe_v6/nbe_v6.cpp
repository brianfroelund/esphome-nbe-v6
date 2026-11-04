#include "nbe_v6.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <algorithm>

namespace esphome {
namespace nbe_v6 {

static const char *const TAG = "nbe_v6";

static const char *const ALARM_STRINGS[] = {
    "OK",                                 // 0
    "Low boiler temperature",            // 1
    "Ignition failed",                   // 2
    "Max chute temperature exceeded",    // 3
    "Boiler temperature sensor error",   // 4
    "Light sensor error",                // 5
    "Chute temperature sensor error",    // 6
    "No fire",                           // 7
    "Motor output error",                // 8
    "Out of pellets",                    // 9
};

static const char *const MODE_STRINGS[] = {
    "Waiting",                           // 0
    "Waiting",                           // 1
    "Starting 1",                        // 2
    "Starting 1",                        // 3
    "Starting 2",                        // 4
    "Running",                           // 5
    "Paused",                            // 6
    "Making hotwater",                   // 7
    "Error - Low boiler temperature",   // 8
    "Stopped",                           // 9
    "Summer stop",                       // 10
    "Error - Temperature sensor hot",    // 11
    "Burner connector plug removed",     // 12
    "Error - Ignition fail",             // 13
    "Shut off",                          // 14
    "Error - Boiler sensor",             // 15
    "Error - Light sensor",              // 16
    "Error - Chute temperature sensor",  // 17
    "Error - Motor output",              // 18
    "Running on battery",                // 19
    "Error - Out of pellet",             // 20
};

uint8_t NBEV6Component::calc_checksum(const std::string &data) {
  uint8_t cs = 0;
  for (char c : data) {
    cs ^= static_cast<uint8_t>(c);
  }
  return cs;
}

bool NBEV6Component::verify_checksum(const std::string &data) {
  return calc_checksum(data) == 0;
}

std::string NBEV6Component::add_checksum(const std::string &data) {
  return data + static_cast<char>(calc_checksum(data));
}

std::string NBEV6Component::trim(const std::string &str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

bool NBEV6Component::parse_int_field(const std::string &frame, size_t offset, size_t len, int &val) {
  if (offset + len > frame.size())
    return false;
  std::string s = trim(frame.substr(offset, len));
  if (s.empty())
    return false;
  char *endptr = nullptr;
  long parsed = std::strtol(s.c_str(), &endptr, 10);
  if (endptr == s.c_str())
    return false;
  val = static_cast<int>(parsed);
  return true;
}

bool NBEV6Component::parse_float_field(const std::string &frame, size_t offset, size_t len, int decimals, float &val) {
  int raw_int = 0;
  if (!parse_int_field(frame, offset, len, raw_int))
    return false;
  val = (decimals == 0) ? static_cast<float>(raw_int) : (static_cast<float>(raw_int) / std::pow(10.0f, decimals));
  return true;
}

void NBEV6Component::parse_and_publish_sensor(sensor::Sensor *s, const std::string &frame, size_t offset, size_t len, int decimals) {
  if (s == nullptr)
    return;
  float val;
  if (parse_float_field(frame, offset, len, decimals, val)) {
    s->publish_state(val);
  }
}

void NBEV6Component::publish_number_if_changed(number::Number *n, float val) {
  if (n != nullptr && (!n->has_state() || n->state != val)) {
    n->publish_state(val);
  }
}

void NBEV6Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up NBE V6 pellet burner component...");
  while (this->available()) {
    this->read();
  }
}

void NBEV6Component::dump_config() {
  ESP_LOGCONFIG(TAG, "NBE V6 Pellet Burner Component:");
  LOG_UPDATE_INTERVAL(this);
}

void NBEV6Component::queue_poll(const std::string &cmd, PollFrameType type, size_t exp_len, const std::string &desc) {
  command_queue_.push({cmd, desc, type, exp_len});
}

void NBEV6Component::queue_command(const std::string &cmd, const std::string &desc) {
  command_queue_.push({cmd, desc, FRAME_COMMAND, 3});
}

void NBEV6Component::queue_param_write(const char *address, float val, float min_val, float max_val, int decimals, const char *desc) {
  float clamped = std::clamp(val, min_val, max_val);
  int raw = static_cast<int>(std::round(clamped * std::pow(10.0f, decimals)));
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%s%04d", address, raw);
  queue_command(std::string(buf), desc);
}

void NBEV6Component::update() {
  // On boot, query version string
  if (!version_requested_) {
    queue_poll("Z040000", FRAME_Z04, 11, "Read Version (Z04)");
    version_requested_ = true;
  }

  poll_cycle_++;

  // Always schedule primary telemetry (Z00)
  queue_poll("Z000000", FRAME_Z00, 101, "Telemetry (Z00)");

  // Round-robin secondary polls across cycles to prevent serial congestion
  if (poll_cycle_ % 3 == 1 && z06_supported_) {
    queue_poll("Z060000", FRAME_Z06, 91, "Extended Sensors (Z06)");
  } else if (poll_cycle_ % 3 == 2) {
    queue_poll("Z020000", FRAME_Z02, 41, "Runtime Counters (Z02)");
  } else if (poll_cycle_ % 6 == 0) {
    if (z01_supported_) queue_poll("Z010000", FRAME_Z01, 231, "Combustion Config (Z01)");
    if (z03_supported_) queue_poll("Z030000", FRAME_Z03, 101, "AutoCalc & Mid Blower (Z03)");
  }
}

void NBEV6Component::send_raw_frame(const QueuedCommand &cmd) {
  while (this->available()) {
    this->read();
  }

  rx_buffer_.clear();
  send_time_ = millis();
  state_ = STATE_WAITING_RESPONSE;

  std::string full_packet = add_checksum(cmd.command);
  ESP_LOGD(TAG, "Sending [%s]: %s (raw len: %u, expected reply: %u bytes)", cmd.description.c_str(), cmd.command.c_str(), full_packet.size(), cmd.expected_length);

  // Use write_array instead of write_str to ensure 0x00 checksum bytes are never truncated
  this->write_array(reinterpret_cast<const uint8_t *>(full_packet.data()), full_packet.size());
  this->flush();
}

void NBEV6Component::enter_drain_state() {
  state_ = STATE_DRAINING_ERROR;
  last_rx_time_ = millis();
}

void NBEV6Component::loop() {
  uint32_t now = millis();

  // Read available serial bytes
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    last_rx_time_ = now;
    if (state_ == STATE_WAITING_RESPONSE) {
      process_incoming_byte(byte);
    }
  }

  // Drain state: Wait for electrical noise or in-flight stove bytes to quiet down before retransmitting
  if (state_ == STATE_DRAINING_ERROR) {
    if (now - last_rx_time_ >= 200) {
      while (this->available()) {
        this->read();
      }
      state_ = STATE_IDLE;
      last_comm_time_ = now;
    }
    return;
  }

  // Check response timeout
  if (state_ == STATE_WAITING_RESPONSE && (now - send_time_ > 1500)) {
    ESP_LOGW(TAG, "Timeout waiting for [%s] (received %u / %u bytes)", current_cmd_.description.c_str(), rx_buffer_.size(), current_cmd_.expected_length);

    if (retry_count_ < 2) {
      retry_count_++;
      ESP_LOGI(TAG, "Retrying [%s] (attempt %u/2)...", current_cmd_.description.c_str(), retry_count_);
      enter_drain_state();
    } else {
      retry_count_ = 0;
      has_active_cmd_ = false;
      enter_drain_state();
    }
    return;
  }

  // Ready to send next command or re-send active command on retry
  if (state_ == STATE_IDLE && (now - last_comm_time_ >= 200)) {
    if (has_active_cmd_) {
      // Retrying current command
      send_raw_frame(current_cmd_);
    } else if (!command_queue_.empty()) {
      current_cmd_ = command_queue_.front();
      command_queue_.pop();
      has_active_cmd_ = true;
      retry_count_ = 0;
      send_raw_frame(current_cmd_);
    }
  }
}

void NBEV6Component::handle_error_response(char err_code) {
  last_comm_time_ = millis();
  has_active_cmd_ = false;
  retry_count_ = 0;

  if (err_code == '1') {
    ESP_LOGW(TAG, "Stove returned E1: Command [%s] not supported on this firmware version.", current_cmd_.description.c_str());
    if (current_cmd_.frame_type == FRAME_Z06) z06_supported_ = false;
    else if (current_cmd_.frame_type == FRAME_Z01) z01_supported_ = false;
    else if (current_cmd_.frame_type == FRAME_Z03) z03_supported_ = false;
    state_ = STATE_IDLE;
  } else {
    ESP_LOGW(TAG, "Stove returned E0: Checksum error on [%s].", current_cmd_.description.c_str());
    enter_drain_state();
  }
}

void NBEV6Component::process_incoming_byte(uint8_t byte) {
  rx_buffer_.push_back(static_cast<char>(byte));

  // Early detection of 3-byte Error response ("E0" / "E1" + checksum)
  // Prevents hanging on a 1.5s timeout when an older firmware rejects an extended poll
  if (rx_buffer_.size() == 3 && rx_buffer_[0] == 'E' && (rx_buffer_[1] == '0' || rx_buffer_[1] == '1')) {
    if (verify_checksum(rx_buffer_)) {
      handle_error_response(rx_buffer_[1]);
      return;
    }
  }

  if (rx_buffer_.size() >= current_cmd_.expected_length) {
    handle_completed_frame();
  }
}

void NBEV6Component::handle_completed_frame() {
  last_comm_time_ = millis();

  if (!verify_checksum(rx_buffer_)) {
    ESP_LOGE(TAG, "Checksum failed for [%s]! Size: %u, expected: %u", current_cmd_.description.c_str(), rx_buffer_.size(), current_cmd_.expected_length);
    if (retry_count_ < 2) {
      retry_count_++;
      ESP_LOGI(TAG, "Retrying [%s] due to checksum failure (attempt %u/2)...", current_cmd_.description.c_str(), retry_count_);
    } else {
      retry_count_ = 0;
      has_active_cmd_ = false;
    }
    enter_drain_state();
    return;
  }

  ESP_LOGV(TAG, "Valid frame received for [%s]", current_cmd_.description.c_str());

  switch (current_cmd_.frame_type) {
    case FRAME_Z00: parse_frame_z00(rx_buffer_); break;
    case FRAME_Z01: parse_frame_z01(rx_buffer_); break;
    case FRAME_Z02: parse_frame_z02(rx_buffer_); break;
    case FRAME_Z03: parse_frame_z03(rx_buffer_); break;
    case FRAME_Z04: parse_frame_z04(rx_buffer_); break;
    case FRAME_Z06: parse_frame_z06(rx_buffer_); break;
    case FRAME_COMMAND: parse_command_ack(rx_buffer_); break;
    default: break;
  }

  has_active_cmd_ = false;
  retry_count_ = 0;
  state_ = STATE_IDLE;
}

void NBEV6Component::parse_frame_z00(const std::string &frame) {
  parse_and_publish_sensor(power_percent_sensor_, frame, 0, 5, 0);
  parse_and_publish_sensor(power_kw_sensor_, frame, 5, 5, 1);
  parse_and_publish_sensor(boiler_temp_sensor_, frame, 10, 5, 1);
  parse_and_publish_sensor(chute_temp_sensor_, frame, 15, 5, 0);
  parse_and_publish_sensor(smoke_temp_sensor_, frame, 20, 5, 0);
  parse_and_publish_sensor(oxygen_sensor_, frame, 25, 5, 1);

  float light_val;
  if (parse_float_field(frame, 30, 5, 0, light_val)) {
    if (light_sensor_) light_sensor_->publish_state(light_val);
    if (fire_detected_sensor_) fire_detected_sensor_->publish_state(light_val > 5.0f);
  }

  parse_and_publish_sensor(feeder_time_sensor_, frame, 35, 10, 0);
  parse_and_publish_sensor(ignition_time_sensor_, frame, 45, 10, 0);

  int alarm_code;
  if (parse_int_field(frame, 55, 5, alarm_code)) {
    if (alarm_text_sensor_) {
      alarm_text_sensor_->publish_state((alarm_code >= 0 && alarm_code <= 9) ? ALARM_STRINGS[alarm_code] : "Unknown Alarm");
    }
    if (alarm_active_sensor_) {
      alarm_active_sensor_->publish_state(alarm_code != 0);
    }
  }

  float target_t;
  if (parse_float_field(frame, 60, 5, 0, target_t)) {
    if (target_boiler_temp_sensor_) target_boiler_temp_sensor_->publish_state(target_t);
    publish_number_if_changed(boiler_temp_set_number_, target_t);
  }

  parse_and_publish_sensor(oxygen_desired_sensor_, frame, 65, 5, 1);

  float feeder_cap;
  if (parse_float_field(frame, 70, 5, 0, feeder_cap)) {
    publish_number_if_changed(feeder_capacity_number_, feeder_cap);
  }

  int mode_code;
  if (parse_int_field(frame, 90, 5, mode_code)) {
    if (mode_text_sensor_) {
      mode_text_sensor_->publish_state((mode_code >= 0 && mode_code <= 20) ? MODE_STRINGS[mode_code] : "Unknown Mode");
    }
    if (burner_switch_) {
      burner_switch_->publish_state(mode_code != 9 && mode_code != 14);
    }
  }
}

void NBEV6Component::parse_frame_z01(const std::string &frame) {
  float val;
  if (parse_float_field(frame, 0 * 5, 5, 0, val)) publish_number_if_changed(blower_low_number_, val);
  if (parse_float_field(frame, 1 * 5, 5, 0, val)) publish_number_if_changed(blower_high_number_, val);
  if (parse_float_field(frame, 10 * 5, 5, 2, val)) publish_number_if_changed(feeder_low_number_, val);
  if (parse_float_field(frame, 11 * 5, 5, 1, val)) publish_number_if_changed(feeder_high_number_, val);
  if (parse_float_field(frame, 12 * 5, 5, 0, val)) publish_number_if_changed(feed_per_minute_number_, val);
  if (parse_float_field(frame, 37 * 5, 5, 0, val)) publish_number_if_changed(min_power_number_, val);
  if (parse_float_field(frame, 38 * 5, 5, 0, val)) publish_number_if_changed(max_power_number_, val);
}

void NBEV6Component::parse_frame_z02(const std::string &frame) {
  float val;
  if (motor_runtime_hours_sensor_ && parse_float_field(frame, 0, 10, 0, val)) {
    motor_runtime_hours_sensor_->publish_state(val / 3600.0f);
  }
  if (ignition_runtime_hours_sensor_ && parse_float_field(frame, 10, 10, 0, val)) {
    ignition_runtime_hours_sensor_->publish_state(val / 3600.0f);
  }
}

void NBEV6Component::parse_frame_z03(const std::string &frame) {
  int auto_calc;
  if (autocalculation_switch_ && parse_int_field(frame, 10 * 5, 5, auto_calc)) {
    autocalculation_switch_->publish_state(auto_calc == 1);
  }

  float mid_blower;
  if (parse_float_field(frame, 14 * 5, 5, 0, mid_blower)) {
    publish_number_if_changed(blower_mid_number_, mid_blower);
  }
}

void NBEV6Component::parse_frame_z04(const std::string &frame) {
  if (version_text_sensor_ && frame.size() >= 10) {
    std::string ver = trim(frame.substr(5, 5));
    version_text_sensor_->publish_state(ver);
    ESP_LOGI(TAG, "Detected NBE V6 chip version: %s", ver.c_str());
  }
}

void NBEV6Component::parse_frame_z06(const std::string &frame) {
  parse_and_publish_sensor(boiler_return_temp_sensor_, frame, 0, 5, 0);
  parse_and_publish_sensor(hot_water_temp_sensor_, frame, 5, 5, 0);
  parse_and_publish_sensor(outdoor_temp_sensor_, frame, 10, 5, 0);
}

void NBEV6Component::parse_command_ack(const std::string &frame) {
  if (frame.rfind("OK", 0) == 0) {
    ESP_LOGI(TAG, "Command [%s] acknowledged by stove (OK).", current_cmd_.description.c_str());
  } else if (frame.rfind("E0", 0) == 0) {
    ESP_LOGW(TAG, "Stove rejected [%s]: Checksum error (E0).", current_cmd_.description.c_str());
  } else if (frame.rfind("E1", 0) == 0) {
    ESP_LOGW(TAG, "Stove rejected [%s]: Parameter not supported (E1).", current_cmd_.description.c_str());
  } else {
    ESP_LOGW(TAG, "Stove unexpected response to [%s]: %s", current_cmd_.description.c_str(), frame.c_str());
  }
}

void NBEV6Component::turn_burner_on() { queue_command("V010000", "Turn Burner ON"); }
void NBEV6Component::turn_burner_off() { queue_command("V000000", "Turn Burner OFF"); }
void NBEV6Component::reset_alarm() { queue_command("V020000", "Reset Alarm"); }
void NBEV6Component::reset_ignition() { queue_command("D040000", "Reset Ignition Counter"); }

void NBEV6Component::set_boiler_temp(float temp) {
  queue_param_write("B01", temp, 40.0f, 85.0f, 0, "Set Boiler Temp");
}

void NBEV6Component::set_feeder_capacity(float capacity) {
  queue_param_write("F01", capacity, 400.0f, 8000.0f, 0, "Set Feeder Capacity");
}

void NBEV6Component::set_blower_low(float val) {
  queue_param_write("A00", val, 4.0f, 50.0f, 0, "Set Blower Low (10%)");
}

void NBEV6Component::set_blower_mid(float val) {
  queue_param_write("A06", val, 5.0f, 75.0f, 0, "Set Blower Mid (50%)");
}

void NBEV6Component::set_blower_high(float val) {
  queue_param_write("A01", val, 5.0f, 100.0f, 0, "Set Blower High (100%)");
}

void NBEV6Component::set_feeder_low(float val) {
  queue_param_write("B04", val, 0.5f, 25.0f, 2, "Set Feeder Low (10%)");
}

void NBEV6Component::set_feeder_high(float val) {
  queue_param_write("B05", val, 1.0f, 100.0f, 1, "Set Feeder High (100%)");
}

void NBEV6Component::set_feed_per_minute(float val) {
  queue_param_write("B06", val, 1.0f, 3.0f, 0, "Set Feeder Cycles / Min");
}

void NBEV6Component::set_min_power(float val) {
  queue_param_write("H02", val, 10.0f, 100.0f, 0, "Set Min Power");
}

void NBEV6Component::set_max_power(float val) {
  queue_param_write("H03", val, 10.0f, 100.0f, 0, "Set Max Power");
}

void NBEV6Component::set_autocalculation(bool state) {
  queue_command(state ? "H040001" : "H040000", "Set Auto Calculation");
}

// Switch Implementations
void NBEV6BurnerSwitch::write_state(bool state) {
  if (state) this->parent_->turn_burner_on();
  else this->parent_->turn_burner_off();
}

void NBEV6AutoCalcSwitch::write_state(bool state) {
  this->parent_->set_autocalculation(state);
  this->publish_state(state);
}

// Number Implementations
void NBEV6BoilerTempNumber::control(float value) {
  this->parent_->set_boiler_temp(value);
  this->publish_state(value);
}

void NBEV6FeederCapacityNumber::control(float value) {
  this->parent_->set_feeder_capacity(value);
  this->publish_state(value);
}

void NBEV6BlowerLowNumber::control(float value) {
  this->parent_->set_blower_low(value);
  this->publish_state(value);
}

void NBEV6BlowerMidNumber::control(float value) {
  this->parent_->set_blower_mid(value);
  this->publish_state(value);
}

void NBEV6BlowerHighNumber::control(float value) {
  this->parent_->set_blower_high(value);
  this->publish_state(value);
}

void NBEV6FeederLowNumber::control(float value) {
  this->parent_->set_feeder_low(value);
  this->publish_state(value);
}

void NBEV6FeederHighNumber::control(float value) {
  this->parent_->set_feeder_high(value);
  this->publish_state(value);
}

void NBEV6FeedPerMinuteNumber::control(float value) {
  this->parent_->set_feed_per_minute(value);
  this->publish_state(value);
}

void NBEV6MinPowerNumber::control(float value) {
  this->parent_->set_min_power(value);
  this->publish_state(value);
}

void NBEV6MaxPowerNumber::control(float value) {
  this->parent_->set_max_power(value);
  this->publish_state(value);
}

// Button Implementations
void NBEV6ResetAlarmButton::press_action() { this->parent_->reset_alarm(); }
void NBEV6ResetIgnitionButton::press_action() { this->parent_->reset_ignition(); }

}  // namespace nbe_v6
}  // namespace esphome

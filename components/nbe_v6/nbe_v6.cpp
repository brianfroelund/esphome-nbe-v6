#include "nbe_v6.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/number/number.h"

#include <cstdlib>
#include <cstdio>
#include <cmath>

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
  char cs = static_cast<char>(calc_checksum(data));
  return data + cs;
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
  if (decimals == 0) {
    val = static_cast<float>(raw_int);
  } else {
    val = static_cast<float>(raw_int) / std::pow(10.0f, decimals);
  }
  return true;
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

void NBEV6Component::update() {
  // If we haven't read version yet, prioritize reading Z04 on startup
  if (!version_requested_) {
    queue_command("Z040000", "Read Version (Z04)");
    queue_command("Z010000", "Combustion Config (Z01)");
    queue_command("Z030000", "AutoCalc & Mid Blower (Z03)");
    version_requested_ = true;
  }

  // Periodic poll schedule
  poll_cycle_++;

  // Fast cycle: always poll Z00 (real-time telemetry)
  queue_command("Z000000", "Telemetry (Z00)");

  // Medium cycle (~30s): poll Z06 (temperatures) and Z02 (runtimes)
  if (poll_cycle_ % 6 == 0) {
    queue_command("Z060000", "Extended Sensors (Z06)");
    queue_command("Z020000", "Runtime Counters (Z02)");
  }

  // Slow cycle (~60s): poll Z01 and Z03 to refresh configuration sliders
  if (poll_cycle_ % 12 == 0) {
    queue_command("Z010000", "Combustion Config (Z01)");
    queue_command("Z030000", "AutoCalc & Mid Blower (Z03)");
  }
}

void NBEV6Component::queue_command(const std::string &cmd, const std::string &desc) {
  command_queue_.push({cmd, desc});
}

void NBEV6Component::send_raw_frame(const std::string &frame, PollFrameType type, size_t expected_length, const std::string &desc) {
  while (this->available()) {
    this->read();
  }

  rx_buffer_.clear();
  current_frame_type_ = type;
  expected_response_len_ = expected_length;
  current_desc_ = desc;
  send_time_ = millis();
  state_ = STATE_WAITING_RESPONSE;

  std::string full_packet = add_checksum(frame);
  ESP_LOGD(TAG, "Sending frame [%s]: %s (len: %u, expected reply: %u bytes)", desc.c_str(), frame.c_str(), full_packet.size(), expected_length);

  this->write_str(full_packet.c_str());
  this->flush();
}

void NBEV6Component::loop() {
  uint32_t now = millis();

  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    process_incoming_byte(byte);
  }

  if (state_ == STATE_WAITING_RESPONSE) {
    if (now - send_time_ > 1500) {
      ESP_LOGW(TAG, "Timeout waiting for response to [%s] (received %u / %u bytes)", current_desc_.c_str(), rx_buffer_.size(), expected_response_len_);
      
      if (retry_count_ < 2) {
        retry_count_++;
        ESP_LOGI(TAG, "Retrying [%s] (attempt %u/2)...", current_desc_.c_str(), retry_count_);
        state_ = STATE_IDLE;
      } else {
        retry_count_ = 0;
        state_ = STATE_IDLE;
        current_frame_type_ = FRAME_NONE;
      }
    }
  }

  if (state_ == STATE_IDLE && !command_queue_.empty()) {
    if (now - last_poll_time_ >= 150) {
      QueuedCommand next_cmd = command_queue_.front();
      command_queue_.pop();

      PollFrameType ftype = FRAME_NONE;
      size_t exp_len = 0;

      if (next_cmd.command.rfind("Z00", 0) == 0) {
        ftype = FRAME_Z00;
        exp_len = 101;  // 100 data chars + 1 checksum
      } else if (next_cmd.command.rfind("Z01", 0) == 0) {
        ftype = FRAME_Z01;
        exp_len = 231;  // 46 fields * 5 = 230 + 1 checksum
      } else if (next_cmd.command.rfind("Z02", 0) == 0) {
        ftype = FRAME_Z02;
        exp_len = 41;   // 40 data chars + 1 checksum
      } else if (next_cmd.command.rfind("Z03", 0) == 0) {
        ftype = FRAME_Z03;
        exp_len = 101;  // 20 fields * 5 = 100 + 1 checksum
      } else if (next_cmd.command.rfind("Z04", 0) == 0) {
        ftype = FRAME_Z04;
        exp_len = 11;   // 10 data chars + 1 checksum
      } else if (next_cmd.command.rfind("Z06", 0) == 0) {
        ftype = FRAME_Z06;
        exp_len = 91;   // 90 data chars + 1 checksum
      } else {
        ftype = FRAME_COMMAND;
        exp_len = 3;    // "OK" + checksum
      }

      send_raw_frame(next_cmd.command, ftype, exp_len, next_cmd.description);
    }
  }
}

void NBEV6Component::process_incoming_byte(uint8_t byte) {
  if (state_ != STATE_WAITING_RESPONSE) {
    return;
  }

  rx_buffer_.push_back(static_cast<char>(byte));

  if (rx_buffer_.size() >= expected_response_len_) {
    handle_completed_frame();
  }
}

void NBEV6Component::handle_completed_frame() {
  last_poll_time_ = millis();
  state_ = STATE_IDLE;
  retry_count_ = 0;

  if (!verify_checksum(rx_buffer_)) {
    ESP_LOGE(TAG, "Checksum failed for [%s]! Size: %u, expected: %u", current_desc_.c_str(), rx_buffer_.size(), expected_response_len_);
    current_frame_type_ = FRAME_NONE;
    return;
  }

  ESP_LOGV(TAG, "Valid frame received for [%s]", current_desc_.c_str());

  switch (current_frame_type_) {
    case FRAME_Z00:
      parse_frame_z00(rx_buffer_);
      break;
    case FRAME_Z01:
      parse_frame_z01(rx_buffer_);
      break;
    case FRAME_Z02:
      parse_frame_z02(rx_buffer_);
      break;
    case FRAME_Z03:
      parse_frame_z03(rx_buffer_);
      break;
    case FRAME_Z04:
      parse_frame_z04(rx_buffer_);
      break;
    case FRAME_Z06:
      parse_frame_z06(rx_buffer_);
      break;
    case FRAME_COMMAND:
      parse_command_ack(rx_buffer_);
      break;
    default:
      break;
  }

  current_frame_type_ = FRAME_NONE;
}

void NBEV6Component::parse_frame_z00(const std::string &frame) {
  float val_f;
  int val_i;

  if (power_percent_sensor_ && parse_float_field(frame, 0, 5, 0, val_f))
    power_percent_sensor_->publish_state(val_f);

  if (power_kw_sensor_ && parse_float_field(frame, 5, 5, 1, val_f))
    power_kw_sensor_->publish_state(val_f);

  if (boiler_temp_sensor_ && parse_float_field(frame, 10, 5, 1, val_f))
    boiler_temp_sensor_->publish_state(val_f);

  if (chute_temp_sensor_ && parse_float_field(frame, 15, 5, 0, val_f))
    chute_temp_sensor_->publish_state(val_f);

  if (smoke_temp_sensor_ && parse_float_field(frame, 20, 5, 0, val_f))
    smoke_temp_sensor_->publish_state(val_f);

  if (oxygen_sensor_ && parse_float_field(frame, 25, 5, 1, val_f))
    oxygen_sensor_->publish_state(val_f);

  if (light_sensor_ && parse_float_field(frame, 30, 5, 0, val_f)) {
    light_sensor_->publish_state(val_f);
    if (fire_detected_sensor_)
      fire_detected_sensor_->publish_state(val_f > 5.0f);
  }

  if (feeder_time_sensor_ && parse_float_field(frame, 35, 10, 0, val_f))
    feeder_time_sensor_->publish_state(val_f);

  if (ignition_time_sensor_ && parse_float_field(frame, 45, 10, 0, val_f))
    ignition_time_sensor_->publish_state(val_f);

  if (parse_int_field(frame, 55, 5, val_i)) {
    if (alarm_text_sensor_) {
      if (val_i >= 0 && val_i <= 9) {
        alarm_text_sensor_->publish_state(ALARM_STRINGS[val_i]);
      } else {
        alarm_text_sensor_->publish_state("Unknown Alarm");
      }
    }
    if (alarm_active_sensor_) {
      alarm_active_sensor_->publish_state(val_i != 0);
    }
  }

  if (target_boiler_temp_sensor_ && parse_float_field(frame, 60, 5, 0, val_f))
    target_boiler_temp_sensor_->publish_state(val_f);

  if (boiler_temp_set_number_ && parse_float_field(frame, 60, 5, 0, val_f)) {
    if (!boiler_temp_set_number_->has_state() || boiler_temp_set_number_->state != val_f)
      boiler_temp_set_number_->publish_state(val_f);
  }

  if (oxygen_desired_sensor_ && parse_float_field(frame, 65, 5, 1, val_f))
    oxygen_desired_sensor_->publish_state(val_f);

  if (feeder_capacity_number_ && parse_float_field(frame, 70, 5, 0, val_f)) {
    if (!feeder_capacity_number_->has_state() || feeder_capacity_number_->state != val_f)
      feeder_capacity_number_->publish_state(val_f);
  }

  if (parse_int_field(frame, 90, 5, val_i)) {
    if (mode_text_sensor_) {
      if (val_i >= 0 && val_i <= 20) {
        mode_text_sensor_->publish_state(MODE_STRINGS[val_i]);
      } else {
        mode_text_sensor_->publish_state("Unknown Mode");
      }
    }
    if (burner_switch_) {
      bool is_on = (val_i != 9 && val_i != 14);
      burner_switch_->publish_state(is_on);
    }
  }
}

void NBEV6Component::parse_frame_z01(const std::string &frame) {
  // Frame Z01 layout: 46 fields of 5 chars
  // 0: blower_low (A00, 4..50%)
  // 1: blower_high (A01, 5..100%)
  // 10: feeder_low (B04, 0.5..25.0s, dec 2)
  // 11: feeder_high (B05, 1..100s, dec 1)
  // 12: feed_per_minute (B06, 1..3)
  // 37: min_power (H02, 10..100%)
  // 38: max_power (H03, 10..100%)
  float val_f;

  if (blower_low_number_ && parse_float_field(frame, 0 * 5, 5, 0, val_f)) {
    if (!blower_low_number_->has_state() || blower_low_number_->state != val_f)
      blower_low_number_->publish_state(val_f);
  }

  if (blower_high_number_ && parse_float_field(frame, 1 * 5, 5, 0, val_f)) {
    if (!blower_high_number_->has_state() || blower_high_number_->state != val_f)
      blower_high_number_->publish_state(val_f);
  }

  if (feeder_low_number_ && parse_float_field(frame, 10 * 5, 5, 2, val_f)) {
    if (!feeder_low_number_->has_state() || feeder_low_number_->state != val_f)
      feeder_low_number_->publish_state(val_f);
  }

  if (feeder_high_number_ && parse_float_field(frame, 11 * 5, 5, 1, val_f)) {
    if (!feeder_high_number_->has_state() || feeder_high_number_->state != val_f)
      feeder_high_number_->publish_state(val_f);
  }

  if (feed_per_minute_number_ && parse_float_field(frame, 12 * 5, 5, 0, val_f)) {
    if (!feed_per_minute_number_->has_state() || feed_per_minute_number_->state != val_f)
      feed_per_minute_number_->publish_state(val_f);
  }

  if (min_power_number_ && parse_float_field(frame, 37 * 5, 5, 0, val_f)) {
    if (!min_power_number_->has_state() || min_power_number_->state != val_f)
      min_power_number_->publish_state(val_f);
  }

  if (max_power_number_ && parse_float_field(frame, 38 * 5, 5, 0, val_f)) {
    if (!max_power_number_->has_state() || max_power_number_->state != val_f)
      max_power_number_->publish_state(val_f);
  }
}

void NBEV6Component::parse_frame_z02(const std::string &frame) {
  float val_f;
  if (motor_runtime_hours_sensor_ && parse_float_field(frame, 0, 10, 0, val_f)) {
    motor_runtime_hours_sensor_->publish_state(val_f / 3600.0f);
  }
  if (ignition_runtime_hours_sensor_ && parse_float_field(frame, 10, 10, 0, val_f)) {
    ignition_runtime_hours_sensor_->publish_state(val_f / 3600.0f);
  }
}

void NBEV6Component::parse_frame_z03(const std::string &frame) {
  // Frame Z03 layout: 20 fields of 5 chars
  // 10: autocalculation (H04, 0 or 1)
  // 14: blower_mid (A06, 5..75%)
  float val_f;
  int val_i;

  if (autocalculation_switch_ && parse_int_field(frame, 10 * 5, 5, val_i)) {
    autocalculation_switch_->publish_state(val_i == 1);
  }

  if (blower_mid_number_ && parse_float_field(frame, 14 * 5, 5, 0, val_f)) {
    if (!blower_mid_number_->has_state() || blower_mid_number_->state != val_f)
      blower_mid_number_->publish_state(val_f);
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
  float val_f;
  if (boiler_return_temp_sensor_ && parse_float_field(frame, 0, 5, 0, val_f)) {
    boiler_return_temp_sensor_->publish_state(val_f);
  }
  if (hot_water_temp_sensor_ && parse_float_field(frame, 5, 5, 0, val_f)) {
    hot_water_temp_sensor_->publish_state(val_f);
  }
  if (outdoor_temp_sensor_ && parse_float_field(frame, 10, 5, 0, val_f)) {
    outdoor_temp_sensor_->publish_state(val_f);
  }
}

void NBEV6Component::parse_command_ack(const std::string &frame) {
  if (frame.rfind("OK", 0) == 0) {
    ESP_LOGI(TAG, "Command [%s] successfully acknowledged by stove (OK).", current_desc_.c_str());
  } else if (frame.rfind("E0", 0) == 0) {
    ESP_LOGW(TAG, "Stove rejected command [%s]: Checksum error (E0).", current_desc_.c_str());
  } else if (frame.rfind("E1", 0) == 0) {
    ESP_LOGW(TAG, "Stove rejected command [%s]: Parameter not found / not supported (E1).", current_desc_.c_str());
  } else {
    ESP_LOGW(TAG, "Stove returned unexpected response to [%s]: %s", current_desc_.c_str(), frame.c_str());
  }
}

void NBEV6Component::turn_burner_on() {
  ESP_LOGI(TAG, "Action: Turn Burner ON (V01)");
  queue_command("V010000", "Turn Burner ON");
}

void NBEV6Component::turn_burner_off() {
  ESP_LOGI(TAG, "Action: Turn Burner OFF (V00)");
  queue_command("V000000", "Turn Burner OFF");
}

void NBEV6Component::reset_alarm() {
  ESP_LOGI(TAG, "Action: Reset Alarm (V02)");
  queue_command("V020000", "Reset Alarm");
}

void NBEV6Component::reset_ignition() {
  ESP_LOGI(TAG, "Action: Reset Ignition Counter (D04)");
  queue_command("D040000", "Reset Ignition Counter");
}

void NBEV6Component::set_boiler_temp(float temp) {
  int t = static_cast<int>(std::round(temp));
  if (t < 40) t = 40;
  if (t > 85) t = 85;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "B01%04d", t);
  queue_command(std::string(buf), "Set Boiler Temp");
}

void NBEV6Component::set_feeder_capacity(float capacity) {
  int cap = static_cast<int>(std::round(capacity));
  if (cap < 400) cap = 400;
  if (cap > 8000) cap = 8000;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "F01%04d", cap);
  queue_command(std::string(buf), "Set Feeder Capacity");
}

void NBEV6Component::set_blower_low(float val) {
  int v = static_cast<int>(std::round(val));
  if (v < 4) v = 4;
  if (v > 50) v = 50;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "A00%04d", v);
  queue_command(std::string(buf), "Set Blower Low (10%)");
}

void NBEV6Component::set_blower_mid(float val) {
  int v = static_cast<int>(std::round(val));
  if (v < 5) v = 5;
  if (v > 75) v = 75;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "A06%04d", v);
  queue_command(std::string(buf), "Set Blower Mid (50%)");
}

void NBEV6Component::set_blower_high(float val) {
  int v = static_cast<int>(std::round(val));
  if (v < 5) v = 5;
  if (v > 100) v = 100;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "A01%04d", v);
  queue_command(std::string(buf), "Set Blower High (100%)");
}

void NBEV6Component::set_feeder_low(float val) {
  int raw = static_cast<int>(std::round(val * 100.0f));
  if (raw < 50) raw = 50;
  if (raw > 2500) raw = 2500;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "B04%04d", raw);
  queue_command(std::string(buf), "Set Feeder Low (10%)");
}

void NBEV6Component::set_feeder_high(float val) {
  int raw = static_cast<int>(std::round(val * 10.0f));
  if (raw < 10) raw = 10;
  if (raw > 1000) raw = 1000;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "B05%04d", raw);
  queue_command(std::string(buf), "Set Feeder High (100%)");
}

void NBEV6Component::set_feed_per_minute(float val) {
  int v = static_cast<int>(std::round(val));
  if (v < 1) v = 1;
  if (v > 3) v = 3;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "B06%04d", v);
  queue_command(std::string(buf), "Set Feeder Cycles / Min");
}

void NBEV6Component::set_min_power(float val) {
  int v = static_cast<int>(std::round(val));
  if (v < 10) v = 10;
  if (v > 100) v = 100;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "H02%04d", v);
  queue_command(std::string(buf), "Set Min Power");
}

void NBEV6Component::set_max_power(float val) {
  int v = static_cast<int>(std::round(val));
  if (v < 10) v = 10;
  if (v > 100) v = 100;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "H03%04d", v);
  queue_command(std::string(buf), "Set Max Power");
}

void NBEV6Component::set_autocalculation(bool state) {
  std::string cmd = state ? "H040001" : "H040000";
  queue_command(cmd, "Set Auto Calculation");
}

void NBEV6BurnerSwitch::write_state(bool state) {
  if (state) {
    this->parent_->turn_burner_on();
  } else {
    this->parent_->turn_burner_off();
  }
}

void NBEV6AutoCalcSwitch::write_state(bool state) {
  this->parent_->set_autocalculation(state);
  this->publish_state(state);
}

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

void NBEV6ResetAlarmButton::press_action() {
  this->parent_->reset_alarm();
}

void NBEV6ResetIgnitionButton::press_action() {
  this->parent_->reset_ignition();
}

}  // namespace nbe_v6
}  // namespace esphome

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
  // If we haven't read version yet, prioritize reading Z04
  if (!version_requested_) {
    queue_command("Z040000", "Read Version (Z04)");
    version_requested_ = true;
  }

  // Periodic poll schedule
  poll_cycle_++;

  // Fast cycle: always poll Z00 (real-time telemetry)
  queue_command("Z000000", "Telemetry (Z00)");

  // Medium cycle (~30-60s depending on update interval): poll Z06 (temperatures) and Z02 (runtimes)
  if (poll_cycle_ % 6 == 0) {
    queue_command("Z060000", "Extended Sensors (Z06)");
    queue_command("Z020000", "Runtime Counters (Z02)");
  }
}

void NBEV6Component::queue_command(const std::string &cmd, const std::string &desc) {
  command_queue_.push({cmd, desc});
}

void NBEV6Component::send_raw_frame(const std::string &frame, PollFrameType type, size_t expected_length, const std::string &desc) {
  // Flush input before sending new request
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

  // Read available serial bytes
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);
    process_incoming_byte(byte);
  }

  // Check timeout if waiting for response
  if (state_ == STATE_WAITING_RESPONSE) {
    if (now - send_time_ > 1500) {
      ESP_LOGW(TAG, "Timeout waiting for response to [%s] (received %u / %u bytes)", current_desc_.c_str(), rx_buffer_.size(), expected_response_len_);
      
      if (retry_count_ < 2) {
        retry_count_++;
        ESP_LOGI(TAG, "Retrying [%s] (attempt %u/2)...", current_desc_.c_str(), retry_count_);
        state_ = STATE_IDLE;
        // The command will be retried in next idle loop if re-queued
      } else {
        retry_count_ = 0;
        state_ = STATE_IDLE;
        current_frame_type_ = FRAME_NONE;
      }
    }
  }

  // If idle and commands are queued, execute the next command
  // Enforce a small inter-frame spacing (150ms) to allow stove micro to settle
  if (state_ == STATE_IDLE && !command_queue_.empty()) {
    if (now - last_poll_time_ >= 150) {
      QueuedCommand next_cmd = command_queue_.front();
      command_queue_.pop();

      PollFrameType ftype = FRAME_NONE;
      size_t exp_len = 0;

      if (next_cmd.command.rfind("Z00", 0) == 0) {
        ftype = FRAME_Z00;
        exp_len = 101;  // 100 data chars + 1 checksum
      } else if (next_cmd.command.rfind("Z02", 0) == 0) {
        ftype = FRAME_Z02;
        exp_len = 41;   // 40 data chars + 1 checksum
      } else if (next_cmd.command.rfind("Z04", 0) == 0) {
        ftype = FRAME_Z04;
        exp_len = 11;   // 10 data chars + 1 checksum
      } else if (next_cmd.command.rfind("Z06", 0) == 0) {
        ftype = FRAME_Z06;
        exp_len = 91;   // 90 data chars + 1 checksum
      } else {
        // Parameter write or Action command (e.g., B010065, V010000, V000000, V020000)
        ftype = FRAME_COMMAND;
        exp_len = 3;    // "OK" + checksum (or error "E0"/"E1" + cs)
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
    case FRAME_Z02:
      parse_frame_z02(rx_buffer_);
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
  // Frame Z00 layout (lengths: 5,5,5,5,5,5,5,10,10,5,5,5,5,5,5,5,5,5 + 1 cs):
  // 0: power (%) [len 5]
  // 5: power_kW (kW, 1 dec) [len 5]
  // 10: boiler_temp (°C, 1 dec) [len 5]
  // 15: chute_temp (°C) [len 5]
  // 20: smoke_temp (°C) [len 5]
  // 25: oxygen (%, 1 dec) [len 5]
  // 30: light (%) [len 5]
  // 35: feeder_time (s) [len 10]
  // 45: ignition_time (s) [len 10]
  // 55: alarm code [len 5]
  // 60: boiler_temp_set (°C) [len 5]
  // 65: oxygen_desired (%, 1 dec) [len 5]
  // 70: feeder_capacity (g) [len 5]
  // 75: chimney_draught [len 5]
  // 80: reserved [len 5]
  // 85: reserved [len 5]
  // 90: mode code [len 5]
  // 95: model [len 5]

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
      // 9 is Stopped, 14 is Shut off
      bool is_on = (val_i != 9 && val_i != 14);
      burner_switch_->publish_state(is_on);
    }
  }
}

void NBEV6Component::parse_frame_z02(const std::string &frame) {
  // Frame Z02 layout (lengths: 10,10,10,10 + 1 cs):
  // 0: motor_time (seconds)
  // 10: el_time (seconds)
  // 20: motor_time_perm
  // 30: el_time_perm
  float val_f;
  if (motor_runtime_hours_sensor_ && parse_float_field(frame, 0, 10, 0, val_f)) {
    motor_runtime_hours_sensor_->publish_state(val_f / 3600.0f);
  }
  if (ignition_runtime_hours_sensor_ && parse_float_field(frame, 10, 10, 0, val_f)) {
    ignition_runtime_hours_sensor_->publish_state(val_f / 3600.0f);
  }
}

void NBEV6Component::parse_frame_z04(const std::string &frame) {
  // Frame Z04 layout (lengths: 5,5 + 1 cs):
  // 0: language
  // 5: version string (5 chars)
  if (version_text_sensor_ && frame.size() >= 10) {
    std::string ver = trim(frame.substr(5, 5));
    version_text_sensor_->publish_state(ver);
    ESP_LOGI(TAG, "Detected NBE V6 chip version: %s", ver.c_str());
  }
}

void NBEV6Component::parse_frame_z06(const std::string &frame) {
  // Frame Z06 layout (18 fields of 5 chars each + 1 cs):
  // 0: boiler_return_temp
  // 5: hotwater_temp
  // 10: outside_temp
  // 15: indoor_temp
  // 20: flow
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
  std::string cmd(buf);

  ESP_LOGI(TAG, "Setting target boiler temperature to %d °C (cmd: %s)", t, cmd.c_str());
  queue_command(cmd, "Set Boiler Temp");
}

void NBEV6Component::set_feeder_capacity(float capacity) {
  int cap = static_cast<int>(std::round(capacity));
  if (cap < 400) cap = 400;
  if (cap > 8000) cap = 8000;

  char buf[16];
  std::snprintf(buf, sizeof(buf), "F01%04d", cap);
  std::string cmd(buf);

  ESP_LOGI(TAG, "Setting feeder capacity to %d g (cmd: %s)", cap, cmd.c_str());
  queue_command(cmd, "Set Feeder Capacity");
}

}  // namespace nbe_v6
}  // namespace esphome

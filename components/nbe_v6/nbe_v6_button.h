#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"


namespace esphome {
namespace nbe_v6 {

class NBEV6ResetAlarmButton : public button::Button, public Parented<NBEV6Component> {
 protected:
  void press_action() override {
    this->parent_->reset_alarm();
  }
};

class NBEV6ResetIgnitionButton : public button::Button, public Parented<NBEV6Component> {
 protected:
  void press_action() override {
    this->parent_->reset_ignition();
  }
};

}  // namespace nbe_v6
}  // namespace esphome

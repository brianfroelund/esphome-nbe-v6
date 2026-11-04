#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"


namespace esphome {
namespace nbe_v6 {

class NBEV6BoilerTempNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override {
    this->parent_->set_boiler_temp(value);
    this->publish_state(value);
  }
};

class NBEV6FeederCapacityNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override {
    this->parent_->set_feeder_capacity(value);
    this->publish_state(value);
  }
};

}  // namespace nbe_v6
}  // namespace esphome

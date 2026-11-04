#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace nbe_v6 {

class NBEV6Component;

class NBEV6BurnerSwitch : public switch_::Switch, public Parented<NBEV6Component> {
 protected:
  void write_state(bool state) override;
};

class NBEV6AutoCalcSwitch : public switch_::Switch, public Parented<NBEV6Component> {
 protected:
  void write_state(bool state) override;
};

}  // namespace nbe_v6
}  // namespace esphome

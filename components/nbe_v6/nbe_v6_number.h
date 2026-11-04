#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

namespace esphome {
namespace nbe_v6 {

class NBEV6Component;

class NBEV6BoilerTempNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6FeederCapacityNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6BlowerLowNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6BlowerMidNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6BlowerHighNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6FeederLowNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6FeederHighNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6FeedPerMinuteNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6MinPowerNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

class NBEV6MaxPowerNumber : public number::Number, public Parented<NBEV6Component> {
 protected:
  void control(float value) override;
};

}  // namespace nbe_v6
}  // namespace esphome

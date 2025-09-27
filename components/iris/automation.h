#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/iris/iris.h"

namespace esphome {
namespace iris {

template<typename... Ts> class IrisSetCodeAction : public Action<Ts...> {
 public:
  IrisSetCodeAction(IrisComponent *iris) : iris_(iris) {}
  TEMPLATABLE_VALUE(uint16_t, code)

  void play(Ts... x) override {
    this->iris_->set_code(this->code_.value(x...));
  }

 protected:
  IrisComponent *iris_;
};

template<typename... Ts> class IrisSendCommandAction : public Action<Ts...> {
 public:
  IrisSendCommandAction(IrisComponent *iris) : iris_(iris) {}
  TEMPLATABLE_VALUE(IrisCommand, command)
  TEMPLATABLE_VALUE(IrisMode, mode)
  TEMPLATABLE_VALUE(uint32_t, repeat)

  void play(Ts... x) override {
    this->iris_->send_command(this->command_.value(x...), this->mode_.value(x...), this->repeat_.value(x...));
  }

 protected:
  IrisComponent *iris_;
};

}  // namespace iris
}  // namespace esphome

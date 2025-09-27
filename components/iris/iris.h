#pragma once

#include "esphome/core/preferences.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/remote_receiver/remote_receiver.h"

namespace esphome {
namespace iris {

enum IrisCommand : uint16_t {
    IRIS_POWER = 0x11,
    IRIS_BLUE = 0x21,
    IRIS_MAGENTA = 0x22,
    IRIS_RED = 0x23,
    IRIS_LIME = 0x24,
    IRIS_GREEN = 0x25,
    IRIS_AQUA = 0x26,
    IRIS_WHITE = 0x27,
    IRIS_MODE1 = 0x31,
    IRIS_MODE2 = 0x32,
    IRIS_MODE3 = 0x33,
    IRIS_MODE4 = 0x34,
    IRIS_BRIGHTNESS = 0x41,
    
    IRIS_MY = 0x1,
    IRIS_UP = 0x2,
    IRIS_MYUP = 0x3,
    IRIS_DOWN = 0x4,
    IRIS_MYDOWN = 0x5,
    IRIS_UPDOWN = 0x6,
    IRIS_MYUPDOWN = 0x7,
    IRIS_PROG = 0x8,
    IRIS_SUNFLAG = 0x9,
    IRIS_FLAG = 0xA,
    IRIS_STEPDOWN = 0xB,
    IRIS_TOGGLE = 0xC,
    IRIS_SENSOR = 0xE,
};

enum IrisMode : uint16_t {
    IRIS_POOL = 0x01,
    IRIS_SPA = 0x02,
    IRIS_POOLSPA = 0x03,
};

class IrisSensor {
 public:
  virtual void update_sunny(uint32_t address, bool value) {}
  virtual void update_windy(uint32_t address, bool value) {}
};

class IrisComponent : public Component, public remote_base::RemoteReceiverListener {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void dump_config() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
  void send_command(IrisCommand command, IrisMode mode, uint32_t repeat = 4);
  void set_code(uint16_t code);
  void set_tx(remote_transmitter::RemoteTransmitterComponent *tx) { this->tx_ = tx; }
  void set_rx(remote_receiver::RemoteReceiverComponent *rx) { this->rx_ = rx; }
  void set_address(uint32_t address) { this->address_ = address; }
  void add_sensor(IrisSensor *sensor) { this->sensors_.push_back(sensor); }
 protected:
  remote_transmitter::RemoteTransmitterComponent *tx_{nullptr};
  remote_receiver::RemoteReceiverComponent *rx_{nullptr};
  ESPPreferenceObject preferences_;
  uint32_t address_;
  uint16_t code_;
  std::vector<IrisSensor *> sensors_;
};

}  // namespace acurite
}  // namespace esphome

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "iris.h"

namespace esphome {
namespace iris {

static const char *const TAG = "iris";
static const int32_t SYMBOL = 640;

void IrisComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Iris:");
  ESP_LOGCONFIG(TAG, "  Name: %" PRIx32, this->address_);
  ESP_LOGCONFIG(TAG, "  Code: %" PRIu16, this->code_);
}

void IrisComponent::setup() {
  uint32_t type = fnv1_hash(std::string("Iris: ") + format_hex(this->address_));
  this->preferences_ = global_preferences->make_preference<uint16_t>(type);
  this->preferences_.load(&this->code_);
  this->rx_->register_listener(this);
}

void IrisComponent::set_code(uint16_t code) {
  ESP_LOGD(TAG, "Iris updating code to %" PRIu16 " from %" PRIu16, code, this->code_);
  this->code_ = code;
  this->preferences_.save(&this->code_);
}

void IrisComponent::send_command(IrisCommand command, IrisMode mode, uint32_t repeat) {
  uint8_t frame[8];
  frame[0] = 0x2D;     // PRE
  frame[1] = 0xD4;     // PRE
  //frame[2] = 0xF9;     // ID1
  frame[2] = this->address_ >> 8;    // remote address  
  //frame[3] = 0xCB;     // ID2
  frame[3] = this->address_;         // remote address
  frame[4] = 0x00;     // Blank Space
  //frame[5] = 0x11;     // Instruction
  frame[5] = command;           // which button did  you press? The 4 LSB will be the checksum
  //frame[6] = 0x03;     // Mode
  frame[6] = mode;     // Mode
  frame[7] = 0x00;     // checksum calculated from bits 0 - 6 , CheckSum8 2s Complement 0x100 - Sum Of Bytes (LAST 9 BITS)

  // Calculate 8-bit 2's complement checksum from bytes 0–6
  uint16_t sum = 0;
  for (int i = 0; i <= 6; i++) {
    sum += frame[i];
  }
  frame[7] = static_cast<uint8_t>(0x100 - (sum & 0xFF));


  // frame[0] = 0xA7;                   // encryption key. Doesn't matter much
  // frame[1] = command << 4;           // which button did  you press? The 4 LSB will be the checksum
  // frame[2] = this->code_ >> 8;       // rolling code (big endian)
  // frame[3] = this->code_;            // rolling code
  // frame[4] = this->address_ >> 16;   // remote address
  // frame[5] = this->address_ >> 8;    // remote address
  // frame[6] = this->address_;         // remote address

  
  //ESP_LOGD(TAG, "Iris sending 0x%" PRIX8 " repeated %" PRIu32 " times", command, repeat);
  ESP_LOGD(TAG, "Iris sending command: 0x%" PRIX16 ",mode: 0x%" PRIX16 ", address: 0x%" PRIX32 ", repeated %" PRIu32 " times", command, mode, this->address_, repeat + 1);

  
  // Optional: original Iris protocol — disabled for test mode
  /*
  // crc
  uint8_t crc = 0;
  for (uint8_t i = 0; i < 7; i++) {
    crc ^= frame[i];
    crc ^= frame[i] >> 4;
  }
  frame[1] |= crc & 0xF;

  // obfuscation
  for (uint8_t i = 1; i < 7; i++) {
    frame[i] ^= frame[i - 1];
  }

  // update code
  this->code_ += 1;
  this->preferences_.save(&this->code_);
  */

  // Prepare transmit
  auto call = this->tx_->transmit();
  remote_base::RemoteTransmitData *dst = call.get_data();

  repeat = 5;
  
  for (uint32_t i = 0; i < (repeat + 1); i++) {
    // Hardware sync: send 4 bytes of 0xAA (10101010)
    const uint8_t sync_bytes[] = {0xAA, 0xAA, 0xAA, 0xAA};
    for (uint8_t sync_byte : sync_bytes) {
      uint8_t byte = sync_byte;
      for (int bit = 0; bit < 8; bit++) {
        if (byte & 0x80) {
          dst->mark(105);   // 1 → HIGH 105 µs
        } else {
          dst->space(104);  // 0 → LOW 104 µs
        }
        byte <<= 1;
      }
    }

    // Optional: software sync — remove if not used
    // dst->item(4550, SYMBOL);     // mark then short space

    // Send frame bits: 1 = mark(105), 0 = space(104)
    for (uint8_t byte : frame) {
      for (uint8_t j = 0; j < 8; j++) {
        if ((byte & 0x80) != 0) {
          dst->mark(105);   // 1 → HIGH 105 µs
        } else {
          dst->space(104);  // 0 → LOW 104 µs
        }
        byte <<= 1;
      }
    }

    // // Optional: inter-frame silence
    //dst->space(10000);  // 10 ms gap before next repeat
  }

  // Send the pulse train
  call.perform();
}

bool IrisComponent::on_receive(remote_base::RemoteReceiveData data) {
  // Step 1: find sync pattern: four 0xAA bytes in a row
  // This is an alternating pattern: mark + space pulses: 1,0,1,0,1,0...
  // We'll look for a repeated pattern of alternating pulses.

  // You may need to adjust tolerance thresholds
  // For example:
  const uint32_t T1 = 105;  // high pulse duration for '1'
  const uint32_t T0 = 104;  // low duration for '0'
  const uint32_t TOL = 20;  // tolerance, µs

  // Find sync: look for alternating mark/space pulses ~105/104 µs
  // We may need to scan data until we detect 32 such pulses (4 bytes × 8 bits)
  // For simplicity, skip detailed sync logic — you'd write a custom matcher here.

  // After sync, read frame bytes:
  uint8_t frame[8] = {0};
  for (int b = 0; b < 8; b++) {
    uint8_t byte = 0;
    for (int bit = 0; bit < 8; bit++) {
      // Expect a mark or space of ~105 or ~104 µs to decide a '1' or '0'
      if (data.expect_mark(T1, TOL)) {
        // saw high pulse
        byte |= (1 << (7 - bit));  // set bit
        data.expect_space(T0, TOL);
      } else if (data.expect_space(T0, TOL)) {
        // saw low pulse => bit = 0
        data.expect_mark(T1, TOL);
      } else {
        return true;  // invalid signal
      }
    }
    frame[b] = byte;
  }

  // Step 2: verify checksum over frame[0]..frame[6]
  uint16_t sum = 0;
  for (int i = 0; i <= 6; i++) {
    sum += frame[i];
  }
  uint8_t expected = static_cast<uint8_t>(0x100 - (sum & 0xFF));
  if (expected != frame[7]) {
    ESP_LOGW(TAG, "Checksum mismatch: got 0x%02X, expected 0x%02X",
             frame[7], expected);
    return true;
  }

  // Step 3: extract values
  uint32_t address = (frame[2] << 8) | frame[3];
  uint16_t command = frame[5];
  uint16_t mode = frame[6];

  ESP_LOGD(TAG, "Decoded frame: address=0x%04X, command=0x%02X, mode=0x%02X",
           address, command, mode);

  // Optionally notify sensors or handle command
  for (auto *sensor : this->sensors_) {
    // e.g. sensor->update…
    // But in your case probably you want to map command/mode further
  }

  return true;
}


}  // namespace iris
}  // namespace esphome

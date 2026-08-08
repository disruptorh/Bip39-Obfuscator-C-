#ifndef BIP39_UI_APP_HPP_
#define BIP39_UI_APP_HPP_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bip39/wordlist.hpp"
#include "clipboard/secure_clipboard.hpp"
#include "secure_mem/secure_buffer.hpp"
#include "crypto/seed_transformer.hpp"

namespace ui {

class app {
 public:
  app() = default;
  ~app();

  app(const app&) = delete;
  app& operator=(const app&) = delete;

  bool init();
  void shutdown();
  void frame();
  const std::string& last_error() const { return last_error_; }

 private:
  std::string last_error_;
  bip39::wordlist wl_;
  clipboard::secure_clipboard clipboard_;

  secure_mem::buffer<char> input_seed_;
  secure_mem::buffer<char> input_secret_;
  secure_mem::buffer<char> input_salt_hex_;

  int kdf_version_ = 2; // Default to V2 Recommended
  secure_mem::secure_string output_seed_;

  struct copy_state {
    bool active = false;
    std::uint64_t expires_at_ms = 0;
  };
  copy_state copy_output_;
  copy_state copy_salt_;

  static constexpr std::size_t kInputCapacity = 4096;

  void render_main_screen();
  void begin_copy(copy_state& target, const char* text, std::size_t len, std::uint64_t now);
  void poll_copies(std::uint64_t now);
  void process_obfuscation();
  void generate_salt();
};

}  // namespace ui

#endif  // BIP39_UI_APP_HPP_

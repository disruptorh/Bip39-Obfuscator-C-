#include "ui/app.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <imgui.h>

namespace ui {

namespace {
std::uint64_t now_ms() {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

std::string bytes_to_hex(const uint8_t* data, size_t len) {
    static const char hex_chars[] = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < len; ++i) {
        out.push_back(hex_chars[data[i] >> 4]);
        out.push_back(hex_chars[data[i] & 0x0F]);
    }
    return out;
}

std::vector<uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> out;
    if (hex.length() % 2 != 0) return out;
    for (size_t i = 0; i < hex.length(); i += 2) {
        char buf[3] = {hex[i], hex[i+1], 0};
        out.push_back(static_cast<uint8_t>(std::strtol(buf, nullptr, 16)));
    }
    return out;
}
}  // namespace

app::~app() { shutdown(); }

bool app::init() {
  try {
    wl_ = bip39::wordlist::load(bip39::wordlist::default_path().c_str());
  } catch (const std::exception&) {
    try {
      wl_ = bip39::wordlist::load(nullptr);
    } catch (const std::exception& e) {
      last_error_ = e.what();
      return false;
    }
  }
  
  input_seed_.resize(kInputCapacity);
  input_secret_.resize(kInputCapacity);
  input_salt_hex_.resize(kInputCapacity);
  
  input_seed_.zero();
  input_secret_.zero();
  input_salt_hex_.zero();
  
  generate_salt();

  if (!clipboard_.init()) {
    last_error_ = "No X server available; clipboard disabled.";
  } else {
    if (const char* env = std::getenv("BIP39_CLIPBOARD_TIMEOUT_MS")) {
      const long v = std::strtol(env, nullptr, 10);
      if (v > 0) clipboard_.set_timeout_ms(static_cast<std::uint64_t>(v));
    }
  }
  return true;
}

void app::shutdown() {
  clipboard_.shutdown();
  input_seed_.release_and_zero();
  input_secret_.release_and_zero();
  input_salt_hex_.release_and_zero();
  output_seed_.wipe();
}

void app::frame() {
  const std::uint64_t now = now_ms();
  clipboard_.poll(now);
  poll_copies(now);
  render_main_screen();
}

void app::begin_copy(copy_state& target, const char* text, std::size_t len, std::uint64_t now) {
  copy_output_.active = false;
  copy_salt_.active = false;
  last_error_.clear();
  clipboard_.set_text(text, len);
  if (clipboard_.is_active()) {
    target.active = true;
    target.expires_at_ms = now + clipboard_.timeout_ms();
  } else {
    last_error_ = "No X server; failed to copy.";
  }
}

void app::poll_copies(std::uint64_t now) {
  copy_state* items[] = {&copy_output_, &copy_salt_};
  for (copy_state* item : items) {
    if (item->active && now >= item->expires_at_ms) item->active = false;
  }
}

void app::generate_salt() {
  auto salt = bip39_obfuscator::crypto::generate_salt();
  std::string hex = bytes_to_hex(salt.data(), salt.size());
  std::strncpy(input_salt_hex_.data(), hex.c_str(), kInputCapacity - 1);
}

void app::process_obfuscation() {
  try {
    bip39_obfuscator::crypto::TransformParams params;
    params.seed_phrase = input_seed_.data();
    params.secret = input_secret_.data();
    params.kdf_version = (kdf_version_ == 1) ? 
        bip39_obfuscator::crypto::KdfVersion::V1_SHA256 : 
        bip39_obfuscator::crypto::KdfVersion::V2_SCRYPT;
        
    if (params.kdf_version == bip39_obfuscator::crypto::KdfVersion::V2_SCRYPT) {
        params.salt = hex_to_bytes(input_salt_hex_.data());
    }
    
    output_seed_ = bip39_obfuscator::crypto::transform_seed(params, wl_);
    last_error_.clear();
  } catch (const std::exception& e) {
    last_error_ = e.what();
    output_seed_.wipe();
  }
}

void app::render_main_screen() {
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
  ImGui::Begin("BIP-39 Obfuscator", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize |
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

  ImGui::Text("BIP-39 Seedphrase Obfuscator (C++)");
  ImGui::Separator();
  
  if (!last_error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Error: %s", last_error_.c_str());
  }

  ImGui::Spacing();
  ImGui::Text("Seed Phrase:");
  ImGui::InputTextMultiline("##seed", input_seed_.data(), kInputCapacity, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 4));

  ImGui::Spacing();
  ImGui::Text("Secret (Password):");
  ImGui::InputText("##secret", input_secret_.data(), kInputCapacity, ImGuiInputTextFlags_Password);

  ImGui::Spacing();
  ImGui::Text("KDF Version:");
  ImGui::RadioButton("V2 Scrypt (Recommended)", &kdf_version_, 2);
  ImGui::SameLine();
  ImGui::RadioButton("V1 SHA-256 (Legacy)", &kdf_version_, 1);

  if (kdf_version_ == 2) {
      ImGui::Spacing();
      ImGui::Text("Salt (Hex):");
      ImGui::InputText("##salt", input_salt_hex_.data(), kInputCapacity);
      ImGui::SameLine();
      if (ImGui::Button("Regenerate Salt")) {
          generate_salt();
      }
      ImGui::SameLine();
      if (ImGui::Button(copy_salt_.active ? "Copied!##salt" : "Copy Salt")) {
          begin_copy(copy_salt_, input_salt_hex_.data(), std::strlen(input_salt_hex_.data()), now_ms());
      }
  }

  ImGui::Spacing();
  if (ImGui::Button("Obfuscate / De-obfuscate", ImVec2(-FLT_MIN, 40))) {
      process_obfuscation();
  }

  if (output_seed_.size() > 0) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
      
      ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Result:");
      ImGui::TextWrapped("%s", output_seed_.data());
      
      ImGui::Spacing();
      if (ImGui::Button(copy_output_.active ? "Copied!##output" : "Copy Result")) {
          begin_copy(copy_output_, output_seed_.data(), output_seed_.size(), now_ms());
      }
  }

  ImGui::End();
}

}  // namespace ui

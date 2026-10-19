#pragma once

#include <voicesmith/Header.h>

#include <voicesmith/fx/AudioEffect.h>

class SelectorEffect final : public AudioEffect {

public:

  // the first added effect is selected by default
  void add(const std::string& name, const std::shared_ptr<AudioEffect>& effect) {
    effects.emplace_back(name, effect);
  }

  void select(const std::string& name) {
    for (size_t i = 0; i < effects.size(); ++i) {
      if (effects[i].first == name) {
        selected = i;
        return;
      }
    }

    throw std::invalid_argument("Invalid effect name " + name + "!");
  }

  void reset(const float samplerate, const size_t blocksize, const size_t channels) override {
    for (auto& effect : effects) {
      effect.second->reset(samplerate, blocksize, channels);
    }
  }

  void apply(const uint64_t index, const std::span<const float> input, const std::span<float> output) override {
    if (effects.empty()) {
      std::memcpy(output.data(), input.data(), std::min(input.size(), output.size()) * sizeof(float));
      return;
    }

    effects[selected].second->apply(index, input, output);
  }

private:

  std::vector<std::pair<std::string, std::shared_ptr<AudioEffect>>> effects;
  std::atomic<size_t> selected {0};

};

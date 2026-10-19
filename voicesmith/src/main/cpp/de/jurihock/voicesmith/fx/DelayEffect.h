#pragma once

#include <voicesmith/Header.h>

#include <voicesmith/fx/AudioEffect.h>

class DelayEffect final : public AudioEffect {

public:

  void delay(const std::string& value);

  void reset(const float samplerate, const size_t blocksize, const size_t channels) override;
  void apply(const uint64_t index, const std::span<const float> input, const std::span<float> output) override;

private:

  struct {
    float samplerate = 0;
    size_t blocksize = 0;
  } config;

  struct {
    struct {
      const double max = 1;
      std::atomic<double> min {0.0};
    } delay;
  } params;

  // ring buffer holding the last maxdelay + 1 samples,
  // so that the delay of exactly maxdelay samples is addressable
  std::vector<float> buffer;
  size_t head = 0;

};

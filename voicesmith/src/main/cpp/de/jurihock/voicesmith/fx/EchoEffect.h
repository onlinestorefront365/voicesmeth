#pragma once

#include <voicesmith/Header.h>

#include <voicesmith/fx/AudioEffect.h>

class EchoEffect final : public AudioEffect {

public:

  // echo delay in milliseconds
  void time(const std::string& value);
  // feedback amount in percent
  void feedback(const std::string& value);
  // wet signal amount in percent
  void mix(const std::string& value);

  void reset(const float samplerate, const size_t blocksize, const size_t channels) override;
  void apply(const uint64_t index, const std::span<const float> input, const std::span<float> output) override;

private:

  struct {
    float samplerate = 0;
  } config;

  struct {
    const double maxtime = 1;
    const double maxfeedback = 0.9;
    std::atomic<double> time {0.25};
    std::atomic<double> feedback {0.4};
    std::atomic<double> mix {0.0};
  } params;

  std::vector<float> buffer;
  size_t head = 0;

};

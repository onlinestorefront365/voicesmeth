#include <voicesmith/fx/DelayEffect.h>

#include <voicesmith/Source.h>

void DelayEffect::delay(const std::string& value) {
  params.delay.min = std::stod(value) * 1e-3;
}

void DelayEffect::reset(const float samplerate, const size_t blocksize, const size_t channels) {
  config.samplerate = samplerate;
  config.blocksize = blocksize;

  const auto maxdelay = static_cast<size_t>(
    params.delay.max * samplerate);

  buffer.assign(maxdelay + 1, 0);
  head = 0;
}

void DelayEffect::apply(const uint64_t index, const std::span<const float> input, const std::span<float> output) {
  if (buffer.empty()) {
    std::memcpy(output.data(), input.data(), std::min(input.size(), output.size()) * sizeof(float));
    return;
  }

  const auto size = buffer.size();

  const auto delay = std::min(size - 1, static_cast<size_t>(
    std::clamp(params.delay.min.load(), 0.0, params.delay.max) * config.samplerate));

  for (size_t i = 0; i < input.size(); ++i) {
    buffer[head] = input[i];
    output[i] = buffer[(head + size - delay) % size];
    head = (head + 1) % size;
  }
}

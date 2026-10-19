#include <voicesmith/fx/EchoEffect.h>

#include <voicesmith/Source.h>

void EchoEffect::time(const std::string& value) {
  params.time = std::stod(value) * 1e-3;
}

void EchoEffect::feedback(const std::string& value) {
  params.feedback = std::stod(value) * 1e-2;
}

void EchoEffect::mix(const std::string& value) {
  params.mix = std::stod(value) * 1e-2;
}

void EchoEffect::reset(const float samplerate, const size_t blocksize, const size_t channels) {
  config.samplerate = samplerate;

  const auto maxdelay = static_cast<size_t>(
    params.maxtime * samplerate);

  buffer.assign(maxdelay + 1, 0);
  head = 0;
}

void EchoEffect::apply(const uint64_t index, const std::span<const float> input, const std::span<float> output) {
  const auto size = buffer.size();

  if (size < 2) {
    std::memcpy(output.data(), input.data(), std::min(input.size(), output.size()) * sizeof(float));
    return;
  }

  const auto delay = std::clamp<size_t>(static_cast<size_t>(
    std::clamp(params.time.load(), 0.0, params.maxtime) * config.samplerate), 1, size - 1);

  const auto feedback = static_cast<float>(
    std::clamp(params.feedback.load(), 0.0, params.maxfeedback));

  const auto mix = static_cast<float>(
    std::clamp(params.mix.load(), 0.0, 1.0));

  for (size_t i = 0; i < input.size(); ++i) {
    const auto echo = buffer[(head + size - delay) % size];

    buffer[head] = input[i] + feedback * echo;
    output[i] = input[i] + mix * echo;

    head = (head + 1) % size;
  }
}

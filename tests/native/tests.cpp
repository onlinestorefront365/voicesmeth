#include <voicesmith/Header.h>

#include <voicesmith/etc/Noise.h>
#include <voicesmith/etc/Oscillator.h>
#include <voicesmith/fx/DelayEffect.h>
#include <voicesmith/fx/NoiseEffect.h>
#include <voicesmith/fx/SelectorEffect.h>
#include <voicesmith/fx/SineEffect.h>
#include <voicesmith/fx/StereoChainEffect.h>
#include <voicesmith/fx/SweepEffect.h>

static int failures = 0;

#define CHECK(condition) \
  do { \
    if (!(condition)) { \
      std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition); \
      ++failures; \
    } \
  } while (false)

static void test_delay_maximum_is_in_bounds() {
  const float samplerate = 8000;
  const size_t blocksize = 64;

  DelayEffect delay;
  delay.reset(samplerate, blocksize, 1);
  delay.delay("1000"); // the maximum offered by the UI

  std::vector<float> input(blocksize, 1.f), output(blocksize);

  // run a bit longer than the delay line to wrap the ring buffer
  for (size_t i = 0; i < 2 * static_cast<size_t>(samplerate) / blocksize; ++i) {
    delay.apply(i, input, output);
  }

  CHECK(output.front() == 1.f);
}

static void test_delay_offset() {
  const float samplerate = 1000;
  const size_t blocksize = 10;

  DelayEffect delay;
  delay.reset(samplerate, blocksize, 1);
  delay.delay("25"); // 25 samples

  std::vector<float> output(blocksize);
  std::vector<float> impulse(blocksize, 0.f);
  impulse[0] = 1.f;
  std::vector<float> silence(blocksize, 0.f);

  std::vector<float> all;
  delay.apply(0, impulse, output);
  all.insert(all.end(), output.begin(), output.end());
  for (size_t i = 1; i < 6; ++i) {
    delay.apply(i, silence, output);
    all.insert(all.end(), output.begin(), output.end());
  }

  for (size_t i = 0; i < all.size(); ++i) {
    CHECK(all[i] == (i == 25 ? 1.f : 0.f));
  }
}

static void test_delay_zero_is_passthrough() {
  DelayEffect delay;
  delay.reset(1000, 4, 1);

  const std::vector<float> input {1, 2, 3, 4};
  std::vector<float> output(4);
  delay.apply(0, input, output);

  CHECK(output == input);
}

static void test_oscillator_amplitude_is_stable() {
  Oscillator<float> osc(440.f, 48000.f);

  float peak = 0;
  for (size_t i = 0; i < 48000 * 60; ++i) {
    peak = std::max(peak, std::abs(osc.sin()));
  }

  CHECK(peak > 0.999f && peak < 1.001f);
}

static void test_sine_frequency_per_channel() {
  const float samplerate = 48000;
  const size_t blocksize = 2 * 4800; // 0.1s stereo

  StereoChainEffect<SineEffect> sine;
  sine.reset(samplerate, blocksize, 2);

  std::vector<float> input(blocksize), output(blocksize);
  sine.apply(0, input, output);

  // 0.1s of 440Hz contains 44 periods, hence ~88 zero crossings per channel
  for (size_t channel = 0; channel < 2; ++channel) {
    size_t crossings = 0;
    for (size_t i = 1; i < blocksize / 2; ++i) {
      crossings += (output[(i - 1) * 2 + channel] < 0) != (output[i * 2 + channel] < 0);
    }
    CHECK(crossings >= 86 && crossings <= 90);
  }
}

static void test_noise_channels_differ() {
  StereoChainEffect<NoiseEffect> noise;
  noise.reset(48000, 2 * 256, 2);

  std::vector<float> input(2 * 256), output(2 * 256);
  noise.apply(0, input, output);

  size_t equal = 0;
  for (size_t i = 0; i < 256; ++i) {
    equal += output[i * 2] == output[i * 2 + 1];
    CHECK(std::abs(output[i * 2]) <= 0.25f);
  }

  CHECK(equal < 8);
}

static void test_sweep_is_bounded_and_nonzero() {
  StereoChainEffect<SweepEffect> sweep;
  sweep.reset(48000, 2 * 4800, 2);

  std::vector<float> input(2 * 4800), output(2 * 4800);

  float peak = 0;
  for (size_t i = 0; i < 100; ++i) {
    sweep.apply(i, input, output);
    for (const auto x : output) {
      peak = std::max(peak, std::abs(x));
    }
  }

  CHECK(peak > 0.24f && peak < 0.26f);
}

static void test_selector() {
  SelectorEffect selector;
  selector.add("sine", std::make_shared<SineEffect>());
  selector.add("noise", std::make_shared<NoiseEffect>());
  selector.reset(48000, 16, 1);

  std::vector<float> input(16, 0.f), output(16, 0.f);

  selector.select("noise");
  selector.apply(0, input, output);
  CHECK(std::any_of(output.begin(), output.end(), [](float x) { return x != 0.f; }));

  bool thrown = false;
  try { selector.select("nonsense"); } catch (const std::invalid_argument&) { thrown = true; }
  CHECK(thrown);
}

int main() {
  test_delay_maximum_is_in_bounds();
  test_delay_offset();
  test_delay_zero_is_passthrough();
  test_oscillator_amplitude_is_stable();
  test_sine_frequency_per_channel();
  test_noise_channels_differ();
  test_sweep_is_bounded_and_nonzero();
  test_selector();

  if (failures) {
    std::fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
  }

  std::puts("all tests passed");
  return 0;
}

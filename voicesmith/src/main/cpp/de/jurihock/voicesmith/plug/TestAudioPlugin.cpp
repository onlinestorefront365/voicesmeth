#include <voicesmith/plug/TestAudioPlugin.h>

#include <voicesmith/Source.h>

#include <voicesmith/io/AudioSink.h>
#include <voicesmith/io/AudioSource.h>

TestAudioPlugin::TestAudioPlugin(jna_callback* callback) :
  callback(callback) {
  state.voice = std::make_shared<StereoChainEffect<DelayEffect, PitchTimbreShiftEffect, EchoEffect>>();
  state.effects = std::make_shared<SelectorEffect>();
  state.effects->add("voice", state.voice);
  state.effects->add("noise", std::make_shared<StereoChainEffect<NoiseEffect>>());
  state.effects->add("sine", std::make_shared<StereoChainEffect<SineEffect>>());
  state.effects->add("sweep", std::make_shared<StereoChainEffect<SweepEffect>>());
}

TestAudioPlugin::~TestAudioPlugin() {
  stop();
}

void TestAudioPlugin::setup(const std::optional<int> input,
                            const std::optional<int> output,
                            const std::optional<float> samplerate,
                            const std::optional<size_t> blocksize,
                            const std::optional<size_t> channels) {
  config.input = input;
  config.output = output;
  config.samplerate = samplerate;
  config.blocksize = blocksize;
  config.channels = channels;
}

void TestAudioPlugin::set(const std::string& param,
                          const std::string& value) {
  if (param == "mode") {
    state.effects->select(value);
  }
  else if (param == "delay") {
    state.voice->get<DelayEffect>([value](auto effect){
      effect->delay(value);
    });
  }
  else if (param == "pitch") {
    state.voice->get<PitchTimbreShiftEffect>([value](auto effect){
      effect->pitch(value);
    });
  }
  else if (param == "timbre") {
    state.voice->get<PitchTimbreShiftEffect>([value](auto effect){
      effect->timbre(value);
    });
  }
  else if (param == "echotime") {
    state.voice->get<EchoEffect>([value](auto effect){
      effect->time(value);
    });
  }
  else if (param == "echofeedback") {
    state.voice->get<EchoEffect>([value](auto effect){
      effect->feedback(value);
    });
  }
  else if (param == "echomix") {
    state.voice->get<EchoEffect>([value](auto effect){
      effect->mix(value);
    });
  }
  else {
    throw std::invalid_argument("Invalid plugin parameter " + param + "!");
  }
}

void TestAudioPlugin::start() {
  if (state.pipeline != nullptr) {
    return;
  }

  auto source = std::make_shared<AudioSource>(config.input, config.samplerate, config.blocksize, config.channels);
  auto sink = std::make_shared<AudioSink>(config.output, config.samplerate, config.blocksize, config.channels);
  auto pipe = std::make_shared<AudioPipeline>(source, sink, state.effects);

  pipe->subscribe([&](const AudioEventCode code, const std::string& data){
    callback(!code, data.c_str());
  });

  // a failing open() must not leave a half initialized pipeline behind
  pipe->open();

  state.pipeline = pipe;

  try {
    pipe->start();
  }
  catch (...) {
    stop();
    throw;
  }
}

void TestAudioPlugin::stop() {
  if (state.pipeline == nullptr) {
    return;
  }

  state.pipeline->stop();
  state.pipeline->close();
  state.pipeline = nullptr;
}

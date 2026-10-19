#pragma once

#include <voicesmith/Header.h>

template<typename T>
class Oscillator final {

public:

  Oscillator() :
    samplerate(0),
    omega(0),
    phasor(0) {
  }

  Oscillator(const T frequency, const T samplerate) :
    samplerate(samplerate),
    omega(std::polar<T>(T(1), pi * frequency / samplerate)),
    phasor(1) {
  }

  Oscillator(const Oscillator<T>& other) :
    samplerate(other.samplerate),
    omega(other.omega),
    phasor(other.phasor) {
  }

  Oscillator<T>& operator=(const Oscillator<T>& other) {
    if (this != &other) {
      samplerate = other.samplerate;
      omega = other.omega;
      phasor = other.phasor;
    }
    return *this;
  }

  std::complex<T> operator()() {
    phasor *= omega;
    return renormalize();
  }

  std::complex<T> operator()(const T frequency) {
    omega = std::polar<T>(T(1), pi * frequency / samplerate);
    phasor *= omega;
    return renormalize();
  }

  T cos() {
    return (*this)().real();
  }

  T cos(const T frequency) {
    return (*this)(frequency).real();
  }

  T sin() {
    return (*this)().imag();
  }

  T sin(const T frequency) {
    return (*this)(frequency).imag();
  }

private:

  // keep the phasor on the unit circle to avoid amplitude drift
  std::complex<T>& renormalize() {
    phasor *= (T(3) - std::norm(phasor)) / T(2);
    return phasor;
  }

  const T pi = T(2) * std::acos(T(-1));

  T samplerate;

  std::complex<T> omega;
  std::complex<T> phasor;

};

package de.jurihock.voicesmith

import android.content.pm.PackageManager
import android.content.pm.PackageManager.NameNotFoundException
import android.os.Bundle
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.Dp
import de.jurihock.voicesmith.etc.Game
import de.jurihock.voicesmith.etc.Log
import de.jurihock.voicesmith.etc.Preferences
import de.jurihock.voicesmith.etc.Vibrator
import de.jurihock.voicesmith.io.AudioDevices
import de.jurihock.voicesmith.io.selectChannels
import de.jurihock.voicesmith.io.selectInputDevice
import de.jurihock.voicesmith.io.selectMode
import de.jurihock.voicesmith.io.selectOutputDevice
import de.jurihock.voicesmith.service.AudioServiceActivity
import de.jurihock.voicesmith.ui.IntParameterScreen
import de.jurihock.voicesmith.ui.BigToggleButtonScreen
import de.jurihock.voicesmith.ui.DeviceSelectorScreen
import de.jurihock.voicesmith.ui.MainTheme
import de.jurihock.voicesmith.ui.UI

class MainActivity : AudioServiceActivity() {

  private val preferences by lazy { Preferences(this) }
  private val devices by lazy { AudioDevices(this) }
  private val game by lazy { Game(this) }
  private val vibrator by lazy { Vibrator(this) }

  private val channels = mutableIntStateOf(1)
  private val mode = mutableIntStateOf(0)
  private val delay = mutableIntStateOf(0)
  private val pitch = mutableIntStateOf(0)
  private val timbre = mutableIntStateOf(0)
  private val echotime = mutableIntStateOf(250)
  private val echofeedback = mutableIntStateOf(40)
  private val echomix = mutableIntStateOf(0)
  private val state = mutableStateOf(false)

  private val modeTitles = intArrayOf(
    R.string.mode_voice, R.string.mode_noise, R.string.mode_sine, R.string.mode_sweep)

  private fun sync() {
    channels.intValue = preferences.channels
    mode.intValue = preferences.mode
    delay.intValue = preferences.delay
    pitch.intValue = preferences.pitch
    timbre.intValue = preferences.timbre
    echotime.intValue = preferences.echotime
    echofeedback.intValue = preferences.echofeedback
    echomix.intValue = preferences.echomix
  }

  override fun onCreate(savedInstanceState: Bundle?) {
    super.onCreate(savedInstanceState)
    try {
      // https://issuetracker.google.com/issues/246845196
      val flags = PackageManager.PackageInfoFlags.of(0)
      val info = packageManager.getPackageInfo(packageName, flags)
      val version = info.versionName
      title = getString(R.string.title_with_version).format(version)
    } catch (exception: NameNotFoundException) {
      Log.e("Unable to determine the package version!", exception)
    }
    try {
      setContent {
        MainTheme {
          Scaffold(
            modifier = Modifier.fillMaxSize(),
            topBar = {
              DeviceSelectorScreen(
                modifier = Modifier.padding(Dp(UI.PADDING)),
                textInput = getString(R.string.input),
                textOutput = getString(R.string.output),
                textMono = getString(R.string.mono),
                textStereo = getString(R.string.stereo),
                channels = channels,
                onSelectInputDevice = { onSelectInputDevice() },
                onSelectOutputDevice = { onSelectOutputDevice() },
                onSelectChannels = { onSelectChannels() })
            },
            bottomBar = {
              BigToggleButtonScreen(
                modifier = Modifier.padding(Dp(UI.PADDING)),
                textOn = getString(R.string.start), textOff = getString(R.string.stop), value = state,
                onToggle = { onStartStopAudioService() })
            }) { padding ->
            Column(modifier = Modifier.padding(padding).padding(Dp(UI.PADDING))) {
              Spacer(modifier = Modifier.weight(1f))
              OutlinedButton(modifier = Modifier.fillMaxWidth(), onClick = { onSelectMode() }) {
                Text(text = getString(modeTitles[mode.intValue.coerceIn(0, modeTitles.size - 1)]))
              }
              Spacer(modifier = Modifier.height(Dp(UI.PADDING)))
              IntParameterScreen(
                name = getString(R.string.delay), unit = getString(R.string.milliseconds), value = delay,
                min = 0, max = 1000, inc = 50,
                onChange = {
                  delay.intValue = it
                  preferences.delay = it
                })
              Spacer(modifier = Modifier.height(Dp(UI.PADDING)))
              IntParameterScreen(
                name = getString(R.string.pitch), unit = getString(R.string.semitones), value = pitch,
                min = -12, max = +12, inc = 1,
                onChange = {
                  pitch.intValue = it
                  preferences.pitch = it
                })
              Spacer(modifier = Modifier.height(Dp(UI.PADDING)))
              IntParameterScreen(
                name = getString(R.string.timbre), unit = getString(R.string.semitones), value = timbre,
                min = -12, max = +12, inc = 1,
                onChange = {
                  timbre.intValue = it
                  preferences.timbre = it
                })
              Spacer(modifier = Modifier.height(Dp(UI.PADDING)))
              IntParameterScreen(
                name = getString(R.string.echo_time), unit = getString(R.string.milliseconds), value = echotime,
                min = 50, max = 1000, inc = 50,
                onChange = {
                  echotime.intValue = it
                  preferences.echotime = it
                })
              Spacer(modifier = Modifier.height(Dp(UI.PADDING)))
              IntParameterScreen(
                name = getString(R.string.echo_feedback), unit = getString(R.string.percent), value = echofeedback,
                min = 0, max = 90, inc = 5,
                onChange = {
                  echofeedback.intValue = it
                  preferences.echofeedback = it
                })
              Spacer(modifier = Modifier.height(Dp(UI.PADDING)))
              IntParameterScreen(
                name = getString(R.string.echo_mix), unit = getString(R.string.percent), value = echomix,
                min = 0, max = 100, inc = 5,
                onChange = {
                  echomix.intValue = it
                  preferences.echomix = it
                })
              Spacer(modifier = Modifier.weight(1f))
            }
          }
        }
      }
    } finally {
      sync()
    }
  }

  override fun onAudioServiceStarted() {
    state.value = true
    game.on()
    vibrator.on()
  }

  override fun onAudioServiceStopped() {
    state.value = false
    game.off()
    vibrator.off()
  }

  override fun onAudioServiceFailed() {
    state.value = false
    game.off()
    vibrator.error()
  }

  private fun onSelectInputDevice() {
    devices.selectInputDevice(preferences.input) {
      preferences.input = it
    }
  }

  private fun onSelectOutputDevice() {
    devices.selectOutputDevice(preferences.output) {
      preferences.output = it
    }
  }

  private fun onSelectMode() {
    devices.selectMode(preferences.mode) {
      mode.intValue = it
      preferences.mode = it
    }
  }

  private fun onSelectChannels() {
    devices.selectChannels(preferences.channels) {
      channels.intValue = it
      preferences.channels = it
    }
  }

}


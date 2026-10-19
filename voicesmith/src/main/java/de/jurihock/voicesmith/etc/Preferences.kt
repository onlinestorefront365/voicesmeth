package de.jurihock.voicesmith.etc

import android.content.Context
import android.content.SharedPreferences.OnSharedPreferenceChangeListener
import androidx.preference.PreferenceManager
import de.jurihock.voicesmith.io.AudioFeatures

class Preferences(context: Context) {

  private val preferences = PreferenceManager.getDefaultSharedPreferences(context)
  private val features by lazy { AudioFeatures(context) }

  var input: Int
    get() { return preferences.getInt(::input.name, 0) }
    set(value) { preferences.edit().putInt(::input.name, value).apply() }

  var output: Int
    get() { return preferences.getInt(::output.name, 0) }
    set(value) { preferences.edit().putInt(::output.name, value).apply() }

  val samplerate: Int
    get() { return features.samplerate }

  val blocksize: Int
    get() { return features.blocksize }

  var channels: Int
    get() { return preferences.getInt(::channels.name, 1) }
    set(value) { preferences.edit().putInt(::channels.name, value).apply() }

  val modeNames = arrayOf("voice", "noise", "sine", "sweep")

  val modeName: String
    get() { return modeNames[mode.coerceIn(0, modeNames.size - 1)] }

  var mode: Int
    get() { return preferences.getInt(::mode.name, 0) }
    set(value) { preferences.edit().putInt(::mode.name, value).apply() }

  var echotime: Int
    get() { return preferences.getInt(::echotime.name, 250) }
    set(value) { preferences.edit().putInt(::echotime.name, value).apply() }

  var echofeedback: Int
    get() { return preferences.getInt(::echofeedback.name, 40) }
    set(value) { preferences.edit().putInt(::echofeedback.name, value).apply() }

  var echomix: Int
    get() { return preferences.getInt(::echomix.name, 0) }
    set(value) { preferences.edit().putInt(::echomix.name, value).apply() }

  var delay: Int
    get() { return preferences.getInt(::delay.name, 0) }
    set(value) { preferences.edit().putInt(::delay.name, value).apply() }

  var pitch: Int
    get() { return preferences.getInt(::pitch.name, 0) }
    set(value) { preferences.edit().putInt(::pitch.name, value).apply() }

  var timbre: Int
    get() { return preferences.getInt(::timbre.name, 0) }
    set(value) { preferences.edit().putInt(::timbre.name, value).apply() }

  fun register(listener: OnSharedPreferenceChangeListener) {
    preferences.registerOnSharedPreferenceChangeListener(listener)
  }

  fun unregister(listener: OnSharedPreferenceChangeListener) {
    preferences.unregisterOnSharedPreferenceChangeListener(listener)
  }

}

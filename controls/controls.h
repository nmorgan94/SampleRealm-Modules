/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 controls
  vendor:             SampleRealm
  version:            1.0.0
  name:               Controls
  description:        What any plugin's editor is built from: the parameter layout made from the specs, the labelled slider and the level meter. The editor module adds the header, presets, overlays and activation.
  dependencies:       juce_audio_processors, juce_gui_basics
  minimumCppStandard: 20

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <functional>
#include <span>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <samplerealm/ParameterSpec.h>
#include <samplerealm/PeakMeter.h>

#include "ParameterLayout.h"
#include "LabeledSlider.h"
#include "LevelMeter.h"

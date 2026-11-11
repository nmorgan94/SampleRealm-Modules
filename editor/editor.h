/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 editor
  vendor:             SampleRealm
  version:            1.0.0
  name:               Editor
  description:        The parts every plugin's editor shares: the header, presets, overlays, the activation bar, the labelled slider and the level meter, with the parameter layout made from the specs.
  dependencies:       juce_audio_processors, juce_gui_basics, licensing
  minimumCppStandard: 20

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <licensing/licensing.h>
#include <samplerealm/ParameterSpec.h>
#include <samplerealm/PeakMeter.h>

#include "ParameterLayout.h"
#include "EditorFonts.h"
#include "LabeledSlider.h"
#include "LevelMeter.h"
#include "ModalOverlay.h"
#include "PromptOverlay.h"
#include "Manual.h"
#include "HoverTip.h"
#include "PresetManager.h"
#include "PresetActions.h"
#include "PresetBar.h"
#include "HeaderBar.h"
#include "About.h"
#include "ActivationActions.h"
#include "ActivationBar.h"

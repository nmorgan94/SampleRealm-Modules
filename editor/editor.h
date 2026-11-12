/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 editor
  vendor:             SampleRealm
  version:            1.0.0
  name:               Editor
  description:        The header, presets and overlays, on top of the controls module.
  dependencies:       juce_audio_processors, juce_gui_basics, controls
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
#include <controls/controls.h>

#include "EditorFonts.h"
#include "ModalOverlay.h"
#include "PromptOverlay.h"
#include "Manual.h"
#include "HoverTip.h"
#include "PresetManager.h"
#include "PresetActions.h"
#include "PresetBar.h"
#include "HeaderBar.h"
#include "About.h"

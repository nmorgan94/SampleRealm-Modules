/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 updates
  vendor:             SampleRealm
  version:            1.0.0
  name:               Updates
  description:        Asks the website for this plugin's latest version, and tells the user when theirs is older.
  dependencies:       juce_events, juce_gui_basics
  minimumCppStandard: 20

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "VersionOrder.h"
#include "UpdateCheck.h"
#include "UpdateActions.h"

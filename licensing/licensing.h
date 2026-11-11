/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 licensing
  vendor:             SampleRealm
  version:            1.0.0
  name:               Licensing
  description:        Derives this machine's code, verifies a signed licence file offline, and takes one from the activation page.
  dependencies:       juce_core
  minimumCppStandard: 20

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <juce_core/juce_core.h>

#include "TrustedKeys.h"
#include "Base64Url.h"
#include "MachineCode.h"
#include "LicenceVerifier.h"
#include "LicenceFile.h"
#include "Activation.h"
#include "LoopbackReceiver.h"

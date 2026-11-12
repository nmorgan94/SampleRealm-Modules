/*******************************************************************************
 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 activation
  vendor:             SampleRealm
  version:            1.0.0
  name:               Activation
  description:        The activation bar and what its buttons do, for a plugin that asks for a licence.
  dependencies:       editor, licensing
  minimumCppStandard: 20

 END_JUCE_MODULE_DECLARATION
*******************************************************************************/

#pragma once

#include <editor/editor.h>
#include <licensing/licensing.h>

#include "ActivationActions.h"
#include "ActivationBar.h"

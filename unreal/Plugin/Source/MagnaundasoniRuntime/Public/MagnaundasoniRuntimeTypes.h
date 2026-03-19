// Copyright Project Magnaundasoni. All Rights Reserved.

#pragma once

/**
 * MagnaundasoniRuntimeTypes.h  (MagnaundasoniRuntime module)
 *
 * Convenience re-export: any file that includes this header will also get all
 * of the shared acoustic types defined in the base Magnaundasoni module
 * (FMagBandArray, FMagAcousticResult, EMagQualityLevel, EMagImportanceClass, …).
 *
 * Runtime-module code should include this header instead of reaching across to
 * the Magnaundasoni module's own include path directly.
 */
#include "MagnaundasoniTypes.h"  // base module's types (on the include path via dependency)

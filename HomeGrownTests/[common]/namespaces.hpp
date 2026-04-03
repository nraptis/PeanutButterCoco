//
//  namespaces.hpp
//  PeanutButterArchiver
//
//  Created by Magneto on 4/2/26.
//

#ifndef namespaces_h
#define namespaces_h

#include <stdio.h>
#include <vector>
#include <string>

#include "Repair_Workflow.hpp"
#include "Knobs.hpp"
#include "Primatives.hpp"

#include "BundleRequest.hpp"
#include "DecodeRequest.hpp"
#include "RepairRequest.hpp"
#include "Bundle_Execution.hpp"
#include "Decode_Execution.hpp"
#include "ArchiveHeader.hpp"
#include "ArchiveLayoutConfig.hpp"
#include "FormatUtilities.hpp"
#include "MockFileSystem.hpp"
#include "MockHardDrive.hpp"

using namespace std;
using namespace peanutbutter;
using namespace peanutbutter::memory_layout;
using namespace peanutbutter::repair_workflow;
using namespace knobs;

using peanutbutter::BundleExecutionResultV2;
using peanutbutter::BundleRequestV2;
using peanutbutter::DecodeExecutionResultV2;
using peanutbutter::DecodeIntentV2;
using peanutbutter::DecodeRequestV2;
using peanutbutter::RepairCoveragePresetV2;
using peanutbutter::RepairRequestV2;
using peanutbutter::RuntimeEventKindV2;
using peanutbutter::RuntimeEventV2;
using peanutbutter::StrengthPresetV2;



#endif /* namespaces_h */

// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Vehicle/AgentVehicleControl.h"
#include "Walker/WalkerControl.h"
#include "AgentControl.generated.h"


USTRUCT(BlueprintType)
struct CARLA_API FSingleAgentControl {
  GENERATED_BODY()

  UPROPERTY(Category = "Single Agent Control", EditAnywhere)
  FAgentVehicleControl VehicleControl;

  UPROPERTY(Category = "Single Agent Control", EditAnywhere)
  FWalkerControl WalkerControl;

};

USTRUCT(BlueprintType)
struct CARLA_API FAgentControl {
  GENERATED_BODY()

  UPROPERTY(Category = "Agent Control", EditAnywhere)
  TMap<uint32, FSingleAgentControl> SingleAgentControls;
};

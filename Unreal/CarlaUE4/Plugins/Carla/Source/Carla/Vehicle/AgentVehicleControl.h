// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once
#include "VehicleControl.h"
#include "AgentVehicleControl.generated.h"

USTRUCT(BlueprintType)
struct CARLA_API FAgentVehicleControl
{
  GENERATED_BODY()

  UPROPERTY(Category = "Agent Vehicle Control", EditAnywhere)
  FVehicleControl VehicleControl;

  UPROPERTY(Category = "Agent Vehicle Control", EditAnywhere)
  bool bTeleport = false;

  UPROPERTY(Category = "Agent Vehicle Control", EditAnywhere)
  FVector TeleportLocation;

  UPROPERTY(Category = "Agent Vehicle Control", EditAnywhere)
  FRotator TeleportOrientation;
};

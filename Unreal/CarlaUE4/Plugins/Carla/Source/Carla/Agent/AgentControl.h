// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "AgentControl.generated.h"

USTRUCT(BlueprintType)
struct CARLA_API FAgentControl
{
  GENERATED_BODY()

  UPROPERTY(Category = "Agent Control", EditAnywhere)
  int id;

  UPROPERTY(Category = "Agent Control", EditAnywhere)
  TArray<FVector> Waypoints;

  UPROPERTY(Category = "Agent Control", EditAnywhere)
  TArray<float> WaypointTimes;
};

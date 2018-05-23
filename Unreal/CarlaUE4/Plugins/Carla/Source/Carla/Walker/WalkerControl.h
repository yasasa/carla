// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "WalkerControl.generated.h"

USTRUCT(BlueprintType)
struct CARLA_API FWalkerControl
{
  GENERATED_BODY()

  UPROPERTY(Category = "Walker Control", EditAnywhere)
  TArray<FVector> waypoints;

  UPROPERTY(Category = "Walker Control", EditAnywhere)
  TArray<float> waypoint_times;
};

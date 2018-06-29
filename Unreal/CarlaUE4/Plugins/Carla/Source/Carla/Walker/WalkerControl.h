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
  TArray<FVector> Points;

  UPROPERTY(Category = "Walker Control", EditAnywhere)
  TArray<float> Times;

  UPROPERTY(Category = "Walker Control", EditAnywhere)
  bool bReset;
};

// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Carla.h"
#include "WalkerAgentComponent.h"
#include "Walker/WalkerAIController.h"

UWalkerAgentComponent::UWalkerAgentComponent(const FObjectInitializer &ObjectInitializer)
  : Super(ObjectInitializer) {}

void UWalkerAgentComponent::BeginPlay()
{
  Walker = Cast<ACharacter>(GetOwner());
  checkf(Walker != nullptr, TEXT("UWalkerAgentComponent can only be attached to ACharacter"));

  Super::BeginPlay();
}

void UWalkerAgentComponent::ApplyAIControl(const FSingleAgentControl &Control)
{
  auto controller = Cast<AWalkerAIController>(Walker->GetController());
  controller->SetControl(Control);
}

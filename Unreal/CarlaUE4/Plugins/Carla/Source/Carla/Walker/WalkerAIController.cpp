// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#include "Carla.h"
#include "WalkerAIController.h"

#include "Navigation/CrowdFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "WheeledVehicle.h"
#include "WheeledVehicleMovementComponent.h"
#include "AI/Navigation/NavigationSystem.h"

#ifdef CARLA_AI_WALKERS_EXTRA_LOG
#include <DrawDebugHelpers.h>
#define LOG_AI_WALKER(Verbosity, Text) UE_LOG(LogCarla, Verbosity, TEXT("Walker %s " Text), *GetPawn()->GetName());
#define EXTRA_LOG_ONLY(predicate) predicate
#else
#define LOG_AI_WALKER(Verbosity, Text)
#define EXTRA_LOG_ONLY(predicate)
#endif // CARLA_AI_WALKERS_EXTRA_LOG


static constexpr float UPDATE_TIME_IN_SECONDS = 0.6f;
static constexpr float PREVISION_TIME_IN_SECONDS = 5.0f;
static constexpr float WALKER_SIGHT_RADIUS = 100.0f;
static constexpr float WALKER_SPEED_DAMPING = 4.0f;
static constexpr float WALKER_PERIPHERAL_VISION_ANGLE_IN_DEGREES = 60.0f;
static constexpr float WALKER_MAX_TIME_PAUSED = 5.0f;
static constexpr float VEHICLE_SAFETY_RADIUS = 600.0f;

// =============================================================================
// -- PawnPath -----------------------------------------------------------------
// =============================================================================

class PawnPath {
  private:
  static FVector GetLocation(const AActor& Actor)
  {
    const FVector& Location = Actor.GetActorLocation();
    return { Location.X, Location.Y, 0.0f };
  }

  static FVector GetForwardVector(const AActor& Actor)
  {
    return Actor.GetTransform().GetRotation().GetForwardVector();
  }

  static float GetForwardSpeed(const AWheeledVehicle& Vehicle)
  {
    const auto* MovementComponent = Vehicle.GetVehicleMovementComponent();
    check(nullptr != MovementComponent);
    return MovementComponent->GetForwardSpeed();
  }

#ifdef CARLA_AI_WALKERS_EXTRA_LOG
  static FVector GetPointForDrawing(const FVector& Location)
  {
    return { Location.X, Location.Y, 50.0f };
  }
#endif // CARLA_AI_WALKERS_EXTRA_LOG

  /// Helper constructor for vehicles.
  explicit PawnPath(const FVector& Location, const FVector& ForwardDirection, const float Speed)
      : Start(Location - ForwardDirection * VEHICLE_SAFETY_RADIUS)
      , End(Location + ForwardDirection * (VEHICLE_SAFETY_RADIUS + Speed * PREVISION_TIME_IN_SECONDS))
  {
  }

  private:
  explicit PawnPath(const APawn& Walker)
      : Start(GetLocation(Walker))
      , End(GetLocation(Walker) + GetForwardVector(Walker) * WALKER_SPEED_DAMPING * WALKER_SIGHT_RADIUS)
  {
  }

  explicit PawnPath(const AWheeledVehicle& Vehicle)
      : PawnPath(GetLocation(Vehicle), GetForwardVector(Vehicle), GetForwardSpeed(Vehicle))
  {
  }

  static bool Intersect(const PawnPath& Lhs, const PawnPath& Rhs, UWorld* EXTRA_LOG_ONLY(World))
  {
    EXTRA_LOG_ONLY(Lhs.DrawDebugArrow(World));
    EXTRA_LOG_ONLY(Rhs.DrawDebugArrow(World));
    FVector IntersectionPoint;
    const bool bIntersect = FMath::SegmentIntersection2D(
        Lhs.Start, Lhs.End,
        Rhs.Start, Rhs.End,
        IntersectionPoint);

#ifdef CARLA_AI_WALKERS_EXTRA_LOG
    if (bIntersect) {
      DrawDebugPoint(World, GetPointForDrawing(IntersectionPoint), 10.0f, FColor::Red, false, 2.0f);
    }
#endif // CARLA_AI_WALKERS_EXTRA_LOG
    return bIntersect;
  }

#ifdef CARLA_AI_WALKERS_EXTRA_LOG
  void DrawDebugArrow(UWorld* World) const
  {
    DrawDebugDirectionalArrow(World, GetPointForDrawing(Start), GetPointForDrawing(End), 60.0f, FColor::Red, false, 1.0f);
  }
#endif // CARLA_AI_WALKERS_EXTRA_LOG

  public:
  /// Check if the paths of @a Walker and @a Vehicle intersect.
  ///
  /// It checks if within the update time the vehicle will cross the straight
  /// line of @a Walker sight radius on its forward direction.
  static bool Intersect(const APawn& Walker, const AWheeledVehicle& Vehicle)
  {
    return Intersect(PawnPath(Walker), PawnPath(Vehicle), Walker.GetWorld());
  }

  private:
  FVector Start;

  FVector End;
};

// =============================================================================
// -- Other static functions ---------------------------------------------------
// =============================================================================

static bool IntersectsWithVehicle(const APawn& Self, const TArray<AActor*>& Actors)
{
  for (auto* Actor : Actors) {
    const auto* Vehicle = Cast<AWheeledVehicle>(Actor);
    if ((Vehicle != nullptr) && PawnPath::Intersect(Self, *Vehicle)) {
      return true;
    }
  }
  return false;
}

// =============================================================================
// -- AWalkerAIController ------------------------------------------------------
// =============================================================================

AWalkerAIController::AWalkerAIController(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UCrowdFollowingComponent>(TEXT("PathFollowingComponent")))
{
  PrimaryActorTick.bCanEverTick = true;
  PrimaryActorTick.TickInterval = UPDATE_TIME_IN_SECONDS;

  auto Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception Component"));
  check(Perception != nullptr);

  SightConfiguration = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfiguration"));
  SightConfiguration->SightRadius = WALKER_SIGHT_RADIUS;
  SightConfiguration->LoseSightRadius = (WALKER_SIGHT_RADIUS + 100.0f);
  SightConfiguration->PeripheralVisionAngleDegrees = WALKER_PERIPHERAL_VISION_ANGLE_IN_DEGREES;
  SightConfiguration->DetectionByAffiliation.bDetectEnemies = true;
  SightConfiguration->DetectionByAffiliation.bDetectNeutrals = true;
  SightConfiguration->DetectionByAffiliation.bDetectFriendlies = true;

  Perception->ConfigureSense(*SightConfiguration);
  Perception->SetDominantSense(SightConfiguration->GetSenseImplementation());
  Perception->OnPerceptionUpdated.AddDynamic(this, &AWalkerAIController::SenseActors);
  SetPerceptionComponent(*Perception);
  TimeInState = 0.0f;
}

void AWalkerAIController::Possess(APawn* aPawn)
{
  Super::Possess(aPawn);
  check(aPawn != nullptr);
  aPawn->OnTakeAnyDamage.AddDynamic(this, &AWalkerAIController::OnPawnTookDamage);
}

void AWalkerAIController::Tick(float DeltaSeconds)
{
  Super::Tick(DeltaSeconds);
  TimeInState += DeltaSeconds;
  if (Status != EWalkerStatus::RunOver) {
    switch (GetMoveStatus()) {
    default:
      break;
    case EPathFollowingStatus::Idle:
      //case EPathFollowingStatus::Waiting: //<-- incomplete path
      if(IsClientControlled()){
        LOG_AI_WALKER(Log, "Retrying movement");
        RetryMovement();
      }else {
        ChangeStatus(EWalkerStatus::Stuck);
      }
      break;
    case EPathFollowingStatus::Paused:
      if (TimeInState > WALKER_MAX_TIME_PAUSED) {
        LOG_AI_WALKER(Log, "is paused, trying resume movement");
        TryResumeMovement();
      }
      break;
    };
  }
}

FPathFollowingRequestResult AWalkerAIController::MoveTo(
    const FAIMoveRequest& MoveRequest,
    FNavPathSharedPtr* OutPath)
{
#ifdef CARLA_AI_WALKERS_EXTRA_LOG
  UE_LOG(LogCarla, Log, TEXT("Walker %s requested move from (%s) to (%s)"),
      *GetPawn()->GetName(),
      *GetPawn()->GetActorLocation().ToString(),
      *MoveRequest.GetGoalLocation().ToString());
#endif // CARLA_AI_WALKERS_EXTRA_LOG

  ChangeStatus(EWalkerStatus::Moving);
  return Super::MoveTo(MoveRequest, OutPath);
}

void AWalkerAIController::OnMoveCompleted(
    const FAIRequestID RequestID,
    const FPathFollowingResult& Result)
{
  Super::OnMoveCompleted(RequestID, Result);
#ifdef CARLA_AI_WALKERS_EXTRA_LOG
  UE_LOG(LogCarla, Log, TEXT("Walker %s completed move at (%s), with status %s"),
      *GetPawn()->GetName(),
      *GetPawn()->GetActorLocation().ToString(),
      *Result.ToString());
#endif // CARLA_AI_WALKERS_EXTRA_LOG
  if (IsClientControlled() && !ControlWaypoints.IsEmpty()) {
     SetNavWaypoint();
  } else {
    ChangeStatus(EWalkerStatus::MoveCompleted);
  }
}

void AWalkerAIController::RetryMovement(){
  SetNavWaypoint();
}

bool AWalkerAIController::SetNavWaypoint()
{
  if(ControlWaypoints.IsEmpty()){
    return false;
  }

  TPair<float, FVector> Target;
  ControlWaypoints.Dequeue(Target);
  auto TimeToNavigate = Target.Key;
  auto Waypoint = Target.Value;

  if(TimeToNavigate <= 0){
    bool success = GetPawn()->SetActorLocation(Waypoint);
    if(success){

      if(ControlWaypoints.Dequeue(Target)){
        TimeToNavigate = Target.Key;
        Waypoint = Target.Value;
      }
      else{
        return true;
      }

    }else{
      UE_LOG(LogCarla, Warning, TEXT("Failed to teleport %s."), *GetPawn()->GetName());
    }
  }

  // Set the navigation command.
  FNavPathSharedPtr OutPath = MakeShareable(new FNavigationPath());
  FPathFindingQuery Query;
  FAIMoveRequest MoveReq(Waypoint);
  MoveReq.SetUsePathfinding(true);
  MoveReq.SetAcceptanceRadius(0.01f);
  MoveReq.SetNavigationFilter(DefaultNavigationFilterClass);
  MoveReq.SetReachTestIncludesAgentRadius(false);
  MoveReq.SetCanStrafe(true);
  MoveReq.SetProjectGoalLocation(true);
  MoveReq.SetAllowPartialPath(false);

  BuildPathfindingQuery(MoveReq, Query);
  FindPathForMoveRequest(MoveReq, Query, OutPath);
  const bool Success = MoveToLocation(Waypoint, 0.01f, false, true, true, true, nullptr, true) == EPathFollowingRequestResult::RequestSuccessful;
  if(!Success)
    UE_LOG(LogCarla, Warning, TEXT("Waypoint Set Failed: %s"), *GetPawn()->GetName());

  // Get distance, and set appropriate speed
  const float Distance
      = OutPath->GetLength();
  float Speed = Distance / TimeToNavigate;

  if(Speed > 200){
    Speed = 200;
  } else if (Speed < 50){
    Speed = 50;
  }
  static_cast<UCharacterMovementComponent *>(GetPawn()->GetMovementComponent())->MaxWalkSpeed = Speed;

  return Success;
}

void AWalkerAIController::SenseActors(TArray<AActor*> Actors)
{
  const auto* aPawn = GetPawn();
  if ((Status == EWalkerStatus::Moving) && (aPawn != nullptr) && IntersectsWithVehicle(*aPawn, Actors) && !IsClientControlled()) {
    TryPauseMovement();
  }
}

void AWalkerAIController::TrySetMovement(bool paused)
{
  if (paused)
    TryPauseMovement(false);
  else
    TryResumeMovement();
}

void AWalkerAIController::ChangeStatus(EWalkerStatus status)
{
  if (status == Status)
    return;
  //switch (status) {  }
  TimeInState = 0.0f;
  Status = status;
}

void AWalkerAIController::TryResumeMovement()
{
  if (Status != EWalkerStatus::Moving) {
    auto MoveRequestID = GetCurrentMoveRequestID();
    if (MoveRequestID == FAIRequestID()) { // equals invalid request.
      LOG_AI_WALKER(Error, "has invalid move ID");
    } else {
      if (!ResumeMove(MoveRequestID)) {
        LOG_AI_WALKER(Error, "is unable to resume movement");
      } else {
        LOG_AI_WALKER(Log, "resuming movement");
        ChangeStatus(EWalkerStatus::Moving);
      }
    }
  }
}

void AWalkerAIController::TryPauseMovement(const bool bItWasRunOver)
{
  if ((Status != EWalkerStatus::Paused) && (Status != EWalkerStatus::RunOver)) {
    auto MoveRequestID = GetCurrentMoveRequestID();
    if (MoveRequestID == FAIRequestID()) { // equals invalid request.
      LOG_AI_WALKER(Error, "has invalid move ID");
    } else {
      if (!PauseMove(MoveRequestID)) {
        LOG_AI_WALKER(Error, "is unable to pause movement");
      } else {
        LOG_AI_WALKER(Log, "paused");
        ChangeStatus(bItWasRunOver ? EWalkerStatus::RunOver : EWalkerStatus::Paused);
      }
    }
  }
}

void AWalkerAIController::OnPawnTookDamage(
    AActor* DamagedActor,
    float Damage,
    const UDamageType* DamageType,
    AController* InstigatedBy,
    AActor* DamageCauser)
{
  LOG_AI_WALKER(Warning, "has been run over");
  constexpr bool bItWasRunOver = true;
  TryPauseMovement(bItWasRunOver);
  ChangeStatus(EWalkerStatus::RunOver);
}

void AWalkerAIController::SetControl(const FSingleAgentControl& Control)
{
  const FWalkerControl &WalkerControl = Control.WalkerControl;
  if(WalkerControl.bReset){
    ControlWaypoints.Empty();
  }

  for(int i = 0; i < WalkerControl.Points.Num(); i++){

    auto Time = WalkerControl.Times[i];
    auto Point = WalkerControl.Points[i];

    TPair<float, FVector> Waypoint(Time, Point);
    ControlWaypoints.Enqueue(Waypoint);
  }

  if(!bClientControlled || WalkerControl.bReset){
    bClientControlled = SetNavWaypoint();
  }
}
#undef EXTRA_LOG_ONLY
#undef LOG_AI_WALKER
#undef CARLA_AI_WALKERS_EXTRA_LOG

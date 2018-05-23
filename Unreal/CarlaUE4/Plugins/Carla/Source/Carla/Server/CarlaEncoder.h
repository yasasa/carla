// Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once

#include "Agent/AgentComponentVisitor.h"

#include "Sensor/SensorDataView.h"
#include "Vehicle/VehicleControl.h"
#include "Agent/AgentControl.h"

#include <carla/carla_server.h>

/// Encodes Unreal classes to CarlaServer API. To be used by FCarlaServer only.
class FCarlaEncoder : private IAgentComponentVisitor
{
public:

  static TUniquePtr<const char[]> Encode(const FString &String);

  static void Encode(
    const TArray<APlayerStart *> &AvailableStartSpots,
    TArray<carla_transform> &Data);

  static void Encode(
    const TArray<USensorDescription *> &SensorDescriptions,
    TArray<carla_sensor_definition> &Data,
    TArray<TUniquePtr<const char[]>> &SensorNamesMemory);

  static void Encode(
      const ACarlaPlayerState &PlayerState,
      carla_measurements &Data);

  static void Encode(
      const TArray<const UAgentComponent *> &Agents,
      TArray<carla_agent> &Data);

  static void Encode(const FSensorDataView &SensorData, carla_sensor_data &Data)
  {
    Data.id = SensorData.GetSensorId();
    Data.header = SensorData.GetHeader().GetData();
    Data.header_size = SensorData.GetHeader().GetSize();
    Data.data = SensorData.GetData().GetData();
    Data.data_size = SensorData.GetData().GetSize();
  }

  static void Decode(const carla_request_new_episode &Data, FString &IniFile)
  {
    IniFile = FString(Data.ini_file_length, ANSI_TO_TCHAR(Data.ini_file));
  }

  static void Decode(const carla_control &Data, FVehicleControl &VehicleControl, FAgentControl &AgentControl)
  {
    VehicleControl.Steer = Data.steer;
    VehicleControl.Throttle = Data.throttle;
    VehicleControl.Brake = Data.brake;
    VehicleControl.bHandBrake = Data.hand_brake;
    VehicleControl.bReverse = Data.reverse;

    AgentControl.id = Data.agent_control.id;
    for(size_t i = 0; i < Data.agent_control.number_of_waypoints; i++){
      const carla_vector3d *waypoint = Data.agent_control.waypoints;
      AgentControl.Waypoints.Add(FVector(waypoint->x, waypoint->y, waypoint->z));
      AgentControl.WaypointTimes.Add(*(Data.agent_control.waypoint_times+i));
    }

    delete[] Data.agent_control.waypoints;
    delete[] Data.agent_control.waypoint_times;
  }

private:

  static void Encode(const UAgentComponent &AgentComponent, carla_agent &Data);

  FCarlaEncoder(carla_agent &Data);

  virtual void Visit(const UTrafficSignAgentComponent &) override;

  virtual void Visit(const UVehicleAgentComponent &) override;

  virtual void Visit(const UWalkerAgentComponent &) override;

  carla_agent &Data;
};

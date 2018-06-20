#!/usr/bin/env python3

# Copyright (c) 2017 Computer Vision Center (CVC) at the Universitat Autonoma de
# Barcelona (UAB).
#
# This work is licensed under the terms of the MIT license.
# For a copy, see <https://opensource.org/licenses/MIT>.
"""Connects with a CARLA simulator and displays the available start positions
for the current map."""

from __future__ import print_function

import argparse
import logging
import sys
import time
from math import atan2, sin, cos
import numpy as np

import pygame

from carla.client import make_carla_client
from carla.planner.map import CarlaMap
from carla.settings import CarlaSettings
from carla.tcp import TCPConnectionError

running = True
WINDOW_HEIGHT = 550
WINDOW_WIDTH = 650

PEDESTRIAN_COLOR = (102, 153, 51)
PEDESTRIAN_WIDTH = 12
PEDESTRIAN_HEIGHT = 12

VEHICLE_COLOR = (153, 51, 0)
VEHICLE_HEIGHT = 8.
VEHICLE_WIDTH = 16.
VEHICLE_ANGLE = atan2(VEHICLE_HEIGHT/2, VEHICLE_WIDTH/2)
VEHICLE_DIAG = (VEHICLE_WIDTH**2 + VEHICLE_HEIGHT**2)**(0.5)


def manage_events(events):
    global running
    for event in events:
        if event.type == pygame.QUIT:
            logging.debug("User quit")
            running = False
            sys.exit(1)


def setup_game_window(scene):
    img = pygame.image.load('carla/planner/%s.png' % scene.map_name)
    width, height = img.get_size()
    height_scl = height / WINDOW_HEIGHT
    width_scl = width / WINDOW_WIDTH

    if not (height_scl and width_scl):
        raise RuntimeError("Can't convert image to screen size")

    height /= height_scl
    width /= width_scl

    dim = (width, height)
    scale = (width_scl, height_scl)

    screen = pygame.display.set_mode(dim)
    img = pygame.transform.scale(img, dim)

    return (screen, img, dim, scale)


def get_agent_rect(location, town_map, height, width, scale):
    pixel_coords = town_map.convert_to_pixel(
        [location.x, location.y, location.z])
    width /= scale[0]
    height /= scale[1]
    pedestrian_rect = pygame.Rect(pixel_coords[0] / scale[0] - width / 2,
                                  pixel_coords[1] / scale[1] - height / 2,
                                  width, height)
    return pedestrian_rect


def play_frame(client, town_map, screen, dim, scale):
    measurements, _ = client.read_data()
    pedestrians = filter(lambda x: x.HasField('pedestrian'),
                         measurements.non_player_agents)
    vehicles = filter(lambda x: x.HasField('vehicle'),
                      measurements.non_player_agents)
    print(vehicles[0])
    for pedestrian in pedestrians:
        pedestrian = pedestrian.pedestrian
        pygame.draw.ellipse(screen, PEDESTRIAN_COLOR,
                            get_agent_rect(pedestrian.transform.location,
                                           town_map, PEDESTRIAN_HEIGHT,
                                           PEDESTRIAN_WIDTH, scale))
    for vehicle in vehicles:
        vehicle = vehicle.vehicle
        vehicle_yaw = vehicle.transform.rotation.yaw
        location = vehicle.transform.location
        coords = town_map.convert_to_pixel([location.x, location.y, location.z])

        vehicle_left = [
            (VEHICLE_DIAG / 2.) * cos(np.deg2rad(vehicle_yaw) - VEHICLE_ANGLE),
            (VEHICLE_DIAG / 2.) * sin(np.deg2rad(vehicle_yaw) - VEHICLE_ANGLE)
        ]
        vehicle_right = [
            (VEHICLE_DIAG / 2.) * cos(np.deg2rad(vehicle_yaw) + VEHICLE_ANGLE),
            (VEHICLE_DIAG / 2.) * sin(np.deg2rad(vehicle_yaw) + VEHICLE_ANGLE)
        ]
        vehicle_polygon = [
                            (coords[0]/scale[0] + vehicle_left[0], coords[1]/scale[1] + vehicle_left[1]),
                            (coords[0]/scale[0] + vehicle_right[0], coords[1]/scale[1] + vehicle_right[1]),
                            (coords[0]/scale[0] - vehicle_left[0], coords[1]/scale[1] - vehicle_left[1]),
                            (coords[0]/scale[0] - vehicle_right[0], coords[1]/scale[1] - vehicle_right[1]),
                          ]
        pygame.draw.polygon(screen, VEHICLE_COLOR, vehicle_polygon)


def start_game(args):
    pygame.init()
    with make_carla_client(args.host, args.port) as client:
        settings = CarlaSettings()
        settings.set(
            SynchronousMode=False,
            SendNonPlayerAgentsInfo=True,
            NumberOfVehicles=3,
            NumberOfPedestrians=40,
            WeatherId=1,
            QualityLevel="Low")
        settings.randomize_seeds()

        scene = client.load_settings(settings)
        screen, img, dim, scale = setup_game_window(scene)
        town_map = CarlaMap(scene.map_name, 0.1653, 50)
        client.start_episode(7)

        while running:
            manage_events(pygame.event.get())
            screen.blit(img, (0, 0))
            play_frame(client, town_map, screen, dim, scale)
            pygame.display.flip()


def main():
    argparser = argparse.ArgumentParser(description=__doc__)
    argparser.add_argument(
        '-v',
        '--verbose',
        action='store_true',
        dest='debug',
        help='print debug information')
    argparser.add_argument(
        '--host',
        metavar='H',
        default='localhost',
        help='IP of the host server (default: localhost)')
    argparser.add_argument(
        '-p',
        '--port',
        metavar='P',
        default=2000,
        type=int,
        help='TCP port to listen to (default: 2000)')
    argparser.add_argument(
        '-pos',
        '--positions',
        metavar='P',
        default='all',
        help='Indices of the positions that you want to plot on the map. '
        'The indices must be separated by commas (default = all positions)')
    argparser.add_argument(
        '--no-labels',
        action='store_true',
        help='do not display position indices')

    args = argparser.parse_args()

    log_level = logging.DEBUG if args.debug else logging.INFO
    logging.basicConfig(format='%(levelname)s: %(message)s', level=log_level)

    logging.info('listening to server %s:%s', args.host, args.port)

    while True:
        try:

            start_game(args)
            print('Done.')
            return

        except TCPConnectionError as error:
            logging.error(error)
            time.sleep(1)
        except RuntimeError as error:
            logging.error(error)
            break


if __name__ == '__main__':

    try:
        main()
    except KeyboardInterrupt:
        print('\nCancelled by user. Bye!')

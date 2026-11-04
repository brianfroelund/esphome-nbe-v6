import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_RUNNING,
)
from . import NBEV6Component, CONF_NBE_V6_ID

DEPENDENCIES = ["nbe_v6"]

CONF_ALARM_ACTIVE = "alarm_active"
CONF_FIRE_DETECTED = "fire_detected"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NBE_V6_ID): cv.use_id(NBEV6Component),
        cv.Optional(CONF_ALARM_ACTIVE): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_PROBLEM,
        ),
        cv.Optional(CONF_FIRE_DETECTED): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_RUNNING,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NBE_V6_ID])

    binary_sensors_map = [
        (CONF_ALARM_ACTIVE, hub.set_alarm_active_sensor),
        (CONF_FIRE_DETECTED, hub.set_fire_detected_sensor),
    ]
    for key, setter in binary_sensors_map:
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(setter(sens))

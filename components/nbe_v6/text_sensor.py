import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from . import NBEV6Component, CONF_NBE_V6_ID

DEPENDENCIES = ["nbe_v6"]

CONF_MODE = "mode"
CONF_ALARM = "alarm"
CONF_VERSION = "version"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NBE_V6_ID): cv.use_id(NBEV6Component),
        cv.Optional(CONF_MODE): text_sensor.text_sensor_schema(
            icon="mdi:fire-alert",
        ),
        cv.Optional(CONF_ALARM): text_sensor.text_sensor_schema(
            icon="mdi:alert-circle-outline",
        ),
        cv.Optional(CONF_VERSION): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:chip",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NBE_V6_ID])

    if CONF_MODE in config:
        s = await text_sensor.new_text_sensor(config[CONF_MODE])
        cg.add(hub.set_mode_text_sensor(s))

    if CONF_ALARM in config:
        s = await text_sensor.new_text_sensor(config[CONF_ALARM])
        cg.add(hub.set_alarm_text_sensor(s))

    if CONF_VERSION in config:
        s = await text_sensor.new_text_sensor(config[CONF_VERSION])
        cg.add(hub.set_version_text_sensor(s))

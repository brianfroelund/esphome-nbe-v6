import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
)
from . import NBEV6Component, CONF_NBE_V6_ID, nbe_v6_ns

DEPENDENCIES = ["nbe_v6"]

CONF_RESET_ALARM = "reset_alarm"
CONF_RESET_IGNITION = "reset_ignition"

NBEV6ResetAlarmButton = nbe_v6_ns.class_("NBEV6ResetAlarmButton", button.Button, cg.Parented.template_(NBEV6Component))
NBEV6ResetIgnitionButton = nbe_v6_ns.class_("NBEV6ResetIgnitionButton", button.Button, cg.Parented.template_(NBEV6Component))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NBE_V6_ID): cv.use_id(NBEV6Component),
        cv.Optional(CONF_RESET_ALARM): button.button_schema(
            NBEV6ResetAlarmButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:bell-cancel-outline",
        ),
        cv.Optional(CONF_RESET_IGNITION): button.button_schema(
            NBEV6ResetIgnitionButton,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:counter",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NBE_V6_ID])

    if CONF_RESET_ALARM in config:
        b = await button.new_button(config[CONF_RESET_ALARM])
        await cg.register_parented(b, hub)

    if CONF_RESET_IGNITION in config:
        b = await button.new_button(config[CONF_RESET_IGNITION])
        await cg.register_parented(b, hub)

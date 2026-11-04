import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    DEVICE_CLASS_SWITCH,
)
from . import NBEV6Component, CONF_NBE_V6_ID, nbe_v6_ns

DEPENDENCIES = ["nbe_v6"]

CONF_BURNER = "burner"
CONF_AUTOCALCULATION = "autocalculation"

NBEV6BurnerSwitch = nbe_v6_ns.class_("NBEV6BurnerSwitch", switch.Switch, cg.Parented.template_(NBEV6Component))
NBEV6AutoCalcSwitch = nbe_v6_ns.class_("NBEV6AutoCalcSwitch", switch.Switch, cg.Parented.template_(NBEV6Component))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NBE_V6_ID): cv.use_id(NBEV6Component),
        cv.Optional(CONF_BURNER): switch.switch_schema(
            NBEV6BurnerSwitch,
            device_class=DEVICE_CLASS_SWITCH,
            icon="mdi:fire",
        ),
        cv.Optional(CONF_AUTOCALCULATION): switch.switch_schema(
            NBEV6AutoCalcSwitch,
            icon="mdi:calculator-variant",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NBE_V6_ID])

    if CONF_BURNER in config:
        sw = await switch.new_switch(config[CONF_BURNER])
        await cg.register_parented(sw, hub)
        cg.add(hub.set_burner_switch(sw))

    if CONF_AUTOCALCULATION in config:
        sw = await switch.new_switch(config[CONF_AUTOCALCULATION])
        await cg.register_parented(sw, hub)
        cg.add(hub.set_autocalculation_switch(sw))

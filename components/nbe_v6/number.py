import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
)
from . import NBEV6Component, CONF_NBE_V6_ID, nbe_v6_ns

DEPENDENCIES = ["nbe_v6"]

CONF_BOILER_TEMP_SET = "boiler_temp_set"
CONF_FEEDER_CAPACITY = "feeder_capacity"

NBEV6BoilerTempNumber = nbe_v6_ns.class_("NBEV6BoilerTempNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6FeederCapacityNumber = nbe_v6_ns.class_("NBEV6FeederCapacityNumber", number.Number, cg.Parented.template_(NBEV6Component))

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NBE_V6_ID): cv.use_id(NBEV6Component),
        cv.Optional(CONF_BOILER_TEMP_SET): number.number_schema(
            NBEV6BoilerTempNumber,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            icon="mdi:thermometer",
        ),
        cv.Optional(CONF_FEEDER_CAPACITY): number.number_schema(
            NBEV6FeederCapacityNumber,
            unit_of_measurement="g",
            icon="mdi:scale",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NBE_V6_ID])

    if CONF_BOILER_TEMP_SET in config:
        conf = config[CONF_BOILER_TEMP_SET]
        num = await number.new_number(
            conf,
            min_value=40.0,
            max_value=85.0,
            step=1.0,
        )
        await cg.register_parented(num, hub)
        cg.add(hub.set_boiler_temp_set_number(num))

    if CONF_FEEDER_CAPACITY in config:
        conf = config[CONF_FEEDER_CAPACITY]
        num = await number.new_number(
            conf,
            min_value=400.0,
            max_value=8000.0,
            step=10.0,
        )
        await cg.register_parented(num, hub)
        cg.add(hub.set_feeder_capacity_number(num))

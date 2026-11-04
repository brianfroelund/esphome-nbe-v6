import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_SECOND,
)
from . import NBEV6Component, CONF_NBE_V6_ID, nbe_v6_ns

DEPENDENCIES = ["nbe_v6"]

CONF_BOILER_TEMP_SET = "boiler_temp_set"
CONF_FEEDER_CAPACITY = "feeder_capacity"
CONF_BLOWER_LOW = "blower_low"
CONF_BLOWER_MID = "blower_mid"
CONF_BLOWER_HIGH = "blower_high"
CONF_FEEDER_LOW = "feeder_low"
CONF_FEEDER_HIGH = "feeder_high"
CONF_FEED_PER_MINUTE = "feed_per_minute"
CONF_MIN_POWER = "min_power"
CONF_MAX_POWER = "max_power"

NBEV6BoilerTempNumber = nbe_v6_ns.class_("NBEV6BoilerTempNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6FeederCapacityNumber = nbe_v6_ns.class_("NBEV6FeederCapacityNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6BlowerLowNumber = nbe_v6_ns.class_("NBEV6BlowerLowNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6BlowerMidNumber = nbe_v6_ns.class_("NBEV6BlowerMidNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6BlowerHighNumber = nbe_v6_ns.class_("NBEV6BlowerHighNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6FeederLowNumber = nbe_v6_ns.class_("NBEV6FeederLowNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6FeederHighNumber = nbe_v6_ns.class_("NBEV6FeederHighNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6FeedPerMinuteNumber = nbe_v6_ns.class_("NBEV6FeedPerMinuteNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6MinPowerNumber = nbe_v6_ns.class_("NBEV6MinPowerNumber", number.Number, cg.Parented.template_(NBEV6Component))
NBEV6MaxPowerNumber = nbe_v6_ns.class_("NBEV6MaxPowerNumber", number.Number, cg.Parented.template_(NBEV6Component))

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
        cv.Optional(CONF_BLOWER_LOW): number.number_schema(
            NBEV6BlowerLowNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:fan",
        ),
        cv.Optional(CONF_BLOWER_MID): number.number_schema(
            NBEV6BlowerMidNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:fan",
        ),
        cv.Optional(CONF_BLOWER_HIGH): number.number_schema(
            NBEV6BlowerHighNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:fan",
        ),
        cv.Optional(CONF_FEEDER_LOW): number.number_schema(
            NBEV6FeederLowNumber,
            unit_of_measurement=UNIT_SECOND,
            icon="mdi:timer-outline",
        ),
        cv.Optional(CONF_FEEDER_HIGH): number.number_schema(
            NBEV6FeederHighNumber,
            unit_of_measurement=UNIT_SECOND,
            icon="mdi:timer-outline",
        ),
        cv.Optional(CONF_FEED_PER_MINUTE): number.number_schema(
            NBEV6FeedPerMinuteNumber,
            unit_of_measurement="pulses/min",
            icon="mdi:repeat",
        ),
        cv.Optional(CONF_MIN_POWER): number.number_schema(
            NBEV6MinPowerNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:gauge-low",
        ),
        cv.Optional(CONF_MAX_POWER): number.number_schema(
            NBEV6MaxPowerNumber,
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:gauge-full",
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NBE_V6_ID])

    number_defs = [
        (CONF_BOILER_TEMP_SET, 40.0, 85.0, 1.0, hub.set_boiler_temp_set_number),
        (CONF_FEEDER_CAPACITY, 400.0, 8000.0, 10.0, hub.set_feeder_capacity_number),
        (CONF_BLOWER_LOW, 4.0, 50.0, 1.0, hub.set_blower_low_number),
        (CONF_BLOWER_MID, 5.0, 75.0, 1.0, hub.set_blower_mid_number),
        (CONF_BLOWER_HIGH, 5.0, 100.0, 1.0, hub.set_blower_high_number),
        (CONF_FEEDER_LOW, 0.5, 25.0, 0.1, hub.set_feeder_low_number),
        (CONF_FEEDER_HIGH, 1.0, 100.0, 0.5, hub.set_feeder_high_number),
        (CONF_FEED_PER_MINUTE, 1.0, 3.0, 1.0, hub.set_feed_per_minute_number),
        (CONF_MIN_POWER, 10.0, 100.0, 1.0, hub.set_min_power_number),
        (CONF_MAX_POWER, 10.0, 100.0, 1.0, hub.set_max_power_number),
    ]

    for key, min_val, max_val, step, setter in number_defs:
        if key in config:
            conf = config[key]
            num = await number.new_number(
                conf,
                min_value=min_val,
                max_value=max_val,
                step=step,
            )
            await cg.register_parented(num, hub)
            cg.add(setter(num))

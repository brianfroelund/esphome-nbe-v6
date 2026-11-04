import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_HOUR,
    UNIT_KILOWATT,
    UNIT_PERCENT,
    UNIT_SECOND,
)
from . import NBEV6Component, CONF_NBE_V6_ID

DEPENDENCIES = ["nbe_v6"]

CONF_BOILER_TEMPERATURE = "boiler_temperature"
CONF_CHUTE_TEMPERATURE = "chute_temperature"
CONF_SMOKE_TEMPERATURE = "smoke_temperature"
CONF_BOILER_RETURN_TEMPERATURE = "boiler_return_temperature"
CONF_HOT_WATER_TEMPERATURE = "hot_water_temperature"
CONF_OUTDOOR_TEMPERATURE = "outdoor_temperature"
CONF_POWER_PERCENT = "power_percent"
CONF_POWER_KW = "power_kw"
CONF_OXYGEN = "oxygen"
CONF_OXYGEN_DESIRED = "oxygen_desired"
CONF_LIGHT = "light"
CONF_FEEDER_TIME = "feeder_time"
CONF_IGNITION_TIME = "ignition_time"
CONF_MOTOR_RUNTIME_HOURS = "motor_runtime_hours"
CONF_IGNITION_RUNTIME_HOURS = "ignition_runtime_hours"
CONF_TARGET_BOILER_TEMPERATURE = "target_boiler_temperature"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_NBE_V6_ID): cv.use_id(NBEV6Component),
        cv.Optional(CONF_BOILER_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CHUTE_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SMOKE_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_BOILER_RETURN_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HOT_WATER_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OUTDOOR_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER_PERCENT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_POWER_KW): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOWATT,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_POWER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OXYGEN): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_OXYGEN_DESIRED): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LIGHT): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_FEEDER_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_IGNITION_TIME): sensor.sensor_schema(
            unit_of_measurement=UNIT_SECOND,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_MOTOR_RUNTIME_HOURS): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_IGNITION_RUNTIME_HOURS): sensor.sensor_schema(
            unit_of_measurement=UNIT_HOUR,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_DURATION,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_TARGET_BOILER_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NBE_V6_ID])

    sensors_map = [
        (CONF_BOILER_TEMPERATURE, hub.set_boiler_temperature_sensor),
        (CONF_CHUTE_TEMPERATURE, hub.set_chute_temperature_sensor),
        (CONF_SMOKE_TEMPERATURE, hub.set_smoke_temperature_sensor),
        (CONF_BOILER_RETURN_TEMPERATURE, hub.set_boiler_return_temperature_sensor),
        (CONF_HOT_WATER_TEMPERATURE, hub.set_hot_water_temperature_sensor),
        (CONF_OUTDOOR_TEMPERATURE, hub.set_outdoor_temperature_sensor),
        (CONF_POWER_PERCENT, hub.set_power_percent_sensor),
        (CONF_POWER_KW, hub.set_power_kw_sensor),
        (CONF_OXYGEN, hub.set_oxygen_sensor),
        (CONF_OXYGEN_DESIRED, hub.set_oxygen_desired_sensor),
        (CONF_LIGHT, hub.set_light_sensor),
        (CONF_FEEDER_TIME, hub.set_feeder_time_sensor),
        (CONF_IGNITION_TIME, hub.set_ignition_time_sensor),
        (CONF_MOTOR_RUNTIME_HOURS, hub.set_motor_runtime_hours_sensor),
        (CONF_IGNITION_RUNTIME_HOURS, hub.set_ignition_runtime_hours_sensor),
        (CONF_TARGET_BOILER_TEMPERATURE, hub.set_target_boiler_temp_sensor),
    ]

    for key, setter in sensors_map:
        if key in config:
            s = await sensor.new_sensor(config[key])
            cg.add(setter(s))

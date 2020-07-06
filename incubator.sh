#!/bin/zsh

# REQUIRE: mosquitto client commands (mosquitto_pub and mosquitto_sub)

# Edit below for your MQTT environment
readonly PUB_CLIENT=$(which mosquitto_pub)
readonly SUB_CLIENT=$(which mosquitto_sub)
readonly BROKER_URL=mqtt_server_url
readonly BROKER_PORT=1883
readonly BROKER_TIMEOUT=3
readonly MQTT_USER=myname
readonly MQTT_PASSWD=mypassword
readonly PUB_TOPIC=/incoming/topic/on/incubator
readonly SUB_TOPIC=/outgoing/topic/on/incubator

# Exit if not found mosquitto client commands
if [ -z "${PUB_CLIENT}" ] ; then ; echo "Error: not found path for mosquitto_pub" ; exit ; fi
if [ -z "${SUB_CLIENT}" ] ; then ; echo "Error: not found path for mosquitto_sub" ; exit ; fi

# Subscribe response from incubator before publishing command
SUB_COMMAND="${SUB_CLIENT} -h ${BROKER_URL} -p ${BROKER_PORT} -C 1 -W ${BROKER_TIMEOUT} -u ${MQTT_USER} -P ${MQTT_PASSWD} -t ${SUB_TOPIC} &"
eval ${SUB_COMMAND}

if [ $# -ge 2 ] && ( [ $1 = "set" ] || [ $1 = "get" ] ) ; then
  PUB_COMMAND="${PUB_CLIENT} -h ${BROKER_URL} -p ${BROKER_PORT} -u ${MQTT_USER} -P ${MQTT_PASSWD} -t ${PUB_TOPIC} -m \"$1 $2\""
  echo ">>> \"$1 $2\""
elif test $# -eq 1; then
  PUB_COMMAND="${PUB_CLIENT} -h ${BROKER_URL} -p ${BROKER_PORT} -u ${MQTT_USER} -P ${MQTT_PASSWD} -t ${PUB_TOPIC} -m \"$1\""
  echo ">>> \"$1\""
else
  echo "Error: less or more than of arguments specified"
  exit
fi

# Publish command
eval ${PUB_COMMAND}

# Wait mosquitto_sub
wait

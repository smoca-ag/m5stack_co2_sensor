#!/bin/bash
rm $(dirname "$0")/generated/*

$(dirname "$0")/lwip-code-generator/LwipMibCompiler $(dirname "$0")/sensorhub.mib $(dirname "$0")/generated /usr/share/snmp/mibs

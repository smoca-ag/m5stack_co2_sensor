#!/bin/bash

$(dirname "$0")/mib2LwipC/LwipMibCompiler $(dirname "$0")/co2sensor-mib.mib $(dirname "$0")/generated /usr/share/snmp/mibs

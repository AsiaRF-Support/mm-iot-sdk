#!/bin/bash


# Script: check_and_program.sh
# Use: ./check_and_program.sh <device> <country>


# FIle directory
DEVICE_LIST="./config/devices.lst"
COUNTRY_LIST="./config/countries.lst"

show_supported_lists() {
  echo ""
  echo "📋 Valid device list (devices.lst):"
  cat "$DEVICE_LIST"
  echo ""
  echo "📋 Valid country list (countries.lst):"
  cat "$COUNTRY_LIST"
  echo ""
}



# Parm check
if [[ $# -lt 2 ]]; then
  echo "Error: Please enter two parameters. Format: $0 <device> <country>" >&2
  show_supported_lists
  exit 1
fi

DEVICE="$1"
COUNTRY="$2"

# Verify device
if ! grep -Fxq "$DEVICE" "$DEVICE_LIST"; then
  echo "❌ Error: Device '$DEVICE' is not supported in list." >&2
  exit 2
fi

# Verify country
if ! grep -Fxq "$COUNTRY" "$COUNTRY_LIST"; then
  echo "❌ Error: Counrty '$COUNTRY' is not supported in list(only support US, JP, EU)." >&2
  exit 3
fi

# Turn country to lower letter(support macOS & Linux)
COUNTRY_LOWER=$(echo "$COUNTRY" | tr '[:upper:]' '[:lower:]')

# Verify config file exist
CONFIG_FILE="./config/config-iot-${COUNTRY_LOWER}.hjson"
if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "❌ Error: file $CONFIG_FILE not found." >&2
  exit 4
fi

# Start command
echo "✅ Verified, start write config..."
CMD="pipenv run tools/platform/program-configstore.py -H localhost -p $DEVICE write-json $CONFIG_FILE"
echo "Start cmd: $CMD"
eval "$CMD"

# Check command result
if [[ $? -eq 0 ]]; then
  echo "✅ Write config success."
  exit 0
else
  echo "❌ Write failed."
  exit 5
fi


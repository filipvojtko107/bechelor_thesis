#!/bin/sh

. ./globals.sh

# Smazani skupiny a uzivatele
userdel $USER_NAME

# Smazani resources
rm -rf $WS_RESOURCES_DIR

# Smazani adresare pro docasne soubory
rm -rf $WS_TEMPORARY_FILES_DIR

# Smazani spousteciho skriptu
rm "${SERVICE_SCRIPT_DIR}/${SERVICE_SCRIPT_NAME}"

# Smazeni adresase s konfiguraci
rm -rf $WS_CONFIG_DIR

# Smazani demona
rm -f "${WS_DAEMON_BIN_PATH}/${WS_DAEMON_NAME}"


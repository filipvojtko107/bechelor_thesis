#!/bin/sh

. ./globals.sh

# Param: File path
function setFileOwner()
{
	chown $USER_NAME:$GROUP_NAME $1
}

# First param: File path; Second param: File permissions
function setFilePermissions()
{
	chmod $2 $1
	setFileOwner $1
}


# Vytvoreni skupiny a uzivatele
useradd $USER_NAME


# Nakopirovani resources
mkdir -p $WS_RESOURCES_DIR
setFilePermissions $WS_RESOURCES_DIR 777

mkdir -p $WS_DEFAULT_PAGES_DIR
setFilePermissions $WS_DEFAULT_PAGES_DIR 755
cp -r "${DEFAULT_PAGES_DIR}"/* $WS_DEFAULT_PAGES_DIR
mv "${WS_DEFAULT_PAGES_DIR}/index.html" $WS_RESOURCES_DIR
setFileOwner "${WS_RESOURCES_DIR}/*"
setFileOwner "${WS_DEFAULT_PAGES_DIR}/*"

mkdir -p $WS_TEMPORARY_FILES_DIR
setFilePermissions $WS_TEMPORARY_FILES_DIR 700


# Nakopirovani spousteciho skriptu
cp $SERVICE_SCRIPT $SERVICE_SCRIPT_DIR


# Nakopirovani konfiguracnich souboru
mkdir -p $WS_CONFIG_DIR
setFilePermissions $WS_CONFIG_DIR 755
cp $CONFIG_FILE $WS_CONFIG_DIR
setFilePermissions $CONFIG_FILE_PATH 644
cp $RESOURCES_CONFIG_FILE $WS_CONFIG_DIR
setFilePermissions $RESOURCES_CONFIG_FILE_PATH 644


# Nakopirovani certifikatu a klicu
cp $RSA_CERT $WS_CONFIG_DIR
cp $RSA_PRIV_KEY $WS_CONFIG_DIR
cp $ECDSA_CERT $WS_CONFIG_DIR
cp $ECDSA_PRIV_KEY $WS_CONFIG_DIR
setFileOwner "${WS_CONFIG_DIR}/*.crt"
setFileOwner "${WS_CONFIG_DIR}/*.key"


# Nakopirovani demona
cp $WS_DAEMON_PATH $WS_DAEMON_BIN_PATH
chmod +x "${WS_DAEMON_BIN_PATH}/${WS_DAEMON_NAME}"
setcap 'cap_net_bind_service=+ep' "${WS_DAEMON_BIN_PATH}/${WS_DAEMON_NAME}"


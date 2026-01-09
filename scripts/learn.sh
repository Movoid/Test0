#!/usr/bin/bash

function signal_handler() {
	echo "Call handler: ${1}."

	if [[ ${1} == "SIGTERM" ]]; then
		exit 1
	fi

	sleep 3
}

trap '{ signal_handler "SIGHUP"; }' SIGHUP
trap '{ signal_handler "SIGINT"; }' SIGINT
trap '{ signal_handler "SIGTERM"; }' SIGTERM

while true; do
	echo 1
done



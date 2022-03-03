#!/bin/sh

DIR="${1:-./build/tpm-state}"
PIDFILE="$DIR/swtpm.pid"
VERBOSE=

die() { echo "$*" >&2 ; exit 1 ; }
warn() { echo "$*" >&2 ; }

# Make the path absolute to work around https://github.com/stefanberger/swtpm/issues/671
ABS_DIR="$(readlink -f "$DIR")"

mkdir -p "$DIR" || die "$DIR: unable to create TPM state directory"

if [ -r "$PIDFILE" ]; then
	PID="$(cat $PIDFILE)"
	warn "Killing old swtpm process $PID"
	kill "$PID" 2>&-
fi

if [ "$V" = 1 ]; then
	VERBOSE="--log level=20"
fi

if [ ! -r "$DIR/tpm2-00.permall" ]; then
	warn "Creating TPM state"
	swtpm_setup \
		--tpm2 \
		--createek \
		--display \
		--tpmstate "$DIR" \
		--config /dev/null \
	|| die "$DIR: unable to setup TPM"
fi

# swtpm 0.7.1 --daemon fails with relative paths, so give it absolute ones
# startup-clear ensures that the TPM is enabled and not locked
swtpm socket \
	--daemon \
	--tpmstate dir="$ABS_DIR" \
	--pid file="$ABS_DIR/swtpm.pid" \
	--server type=unixio,path="$ABS_DIR/swtpm-sock" \
	--ctrl type=unixio,path="$ABS_DIR/swtpm-sock.ctrl" \
	--flags not-need-init,startup-clear \
	--tpm2 \
	$VERBOSE \
|| die "$DIR: unable to start swtpm"


#if [ ! -r "$DIR/ek.pem" ]; then
	warn "$DIR: reading EK public key"
	TPM2TOOLS_TCTI=swtpm:path="$DIR/swtpm-sock" \
	tpm2 createek \
		-c "$DIR/ek.ctx" \
		-u "$DIR/ek.pem" \
		-f pem \
	|| die "$DIR: unable to read EK"

	openssl rsa \
		-noout \
		-text \
		-pubin \
		-in build/tpm-state/ek.pem
#fi




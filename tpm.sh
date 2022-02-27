#!/bin/sh

mkdir -p /tmp/mytpm1
swtpm socket --tpmstate dir=/tmp/mytpm1 --log level=20 --tpm2 \
    --ctrl type=unixio,path=/tmp/mytpm1/swtpm-sock

#!/bin/sh

# Otteniamo la directory corrente
CURRENT_DIR="$(pwd)"
LKVM=$CURRENT_DIR/../../lkvm
FIRMWARE=$CURRENT_DIR/../../firmware/KVMTOOL_EFI.fd
ALPINE=$CURRENT_DIR/kvm.img
SNAPSHOT=$CURRENT_DIR/snapshot/snapshot.bin


$LKVM run \
--name kvm \
--firmware $FIRMWARE \
--disk $ALPINE \
--cpus 1 \
--mem 1024 \
--nested --e2h0 \
--params "console=ttyS0,115200 earlycon" \
--intercept-vm-exit \
--restore-snapshot $SNAPSHOT

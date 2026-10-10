#!/bin/sh

CURRENT_DIR=$PWD

$CURRENT_DIR/../join_files $CURRENT_DIR/kvm.img $CURRENT_DIR/kvm.img.part*

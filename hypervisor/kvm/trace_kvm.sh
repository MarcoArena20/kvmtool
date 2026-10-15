#!/bin/sh

# Otteniamo la directory corrente
CURRENT_DIR="$(pwd)"
LKVM=$CURRENT_DIR/../../lkvm

$LKVM snap --directory Test


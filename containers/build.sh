#!/bin/sh
podman build -t blkhash-$1 -f $1.containerfile .

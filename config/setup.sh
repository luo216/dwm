#!/bin/sh

HOSTNAME=$(cat /etc/hostname)
cp ./config/$HOSTNAME.h ./config.h

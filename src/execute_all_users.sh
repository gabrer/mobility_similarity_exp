#!/bin/sh

percentuale="60"

UTENTI="3 4 17 30 41 62 68 128 153 163"

for USER in UTENTI
do
	echo ./similarity $USER > "log_"$USER"_"$percentuale"perc_.txt" 2>&1 &
done
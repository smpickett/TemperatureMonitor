@echo off
del photon_firmware_*.bin
particle compile photon . > buildoutput.txt

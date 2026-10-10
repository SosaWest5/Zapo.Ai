@echo off
cd /d "%~dp0"
py -3 server.py
if errorlevel 1 (
  echo Install Python 3.10 or newer from python.org, then try again.
  pause
)

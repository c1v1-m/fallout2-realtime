@echo off
rem Fallout 2 Realtime -- playtest build. Runs the dev build from build\Release (hardlinked Steam data,
rem its own data\SAVEGAME, so the Steam saves are never touched). Traces land in build\Release\trace_*.log.
cd /d "%~dp0build\Release"
start "" fallout2-ce.exe

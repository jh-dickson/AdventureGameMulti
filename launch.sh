#!/bin/bash
#Load the main menu, then if it exits with code 0, run the game file
./main && gnome-terminal --title="Game Window" --geometry=140x50 --command="bash -c './game; $SHELL'"
#!/bin/bash
#runs multiplayer-game-cpp + server
#how to use xterm: xterm -e "./app1; /bin/bash" & xterm -e "app2; /bin/bash" & ...
nano environmentVars.sh
chmod +x environmentVars.sh
source ./environmentVars.sh
chmod +x saveData.sh
source ./saveData.sh
echo "Hopefully you built the project with 'mkdir build', 'cd build' 'cmake ..' and 'make'..."
# Remove any leftover socket files
rm -f /tmp/game_server_*
read -p "Do you have xterm installed? [y/n]: " xtermInstalled
if [ "${xtermInstalled}" = "y" ] || [ "${xtermInstalled}" = "Y" ]; then
    if [ -d "build" ]; then
        rm server
        rm client
        cp build/client .
        cp build/server .
        # Kill any existing server processes
pkill -f "./server"

# Kill any process using our ports
for port in {5766..5776}; do
    fuser -k $port/tcp 2>/dev/null
done

        chmod 644 *.png
        xterm -e "sleep 1; ./client; /bin/bash" & xterm -e "./server; /bin/bash"
    else
        echo "Build directory does not exist. Please create it and build the project."
        exit 1
    fi
else
    read -p "Do you have pacman or apt installed? [y/n]: " packageManager
    if [ "${packageManager}" = "y" ]; then
        read -p "Do you have pacman installed? [y/n]: " pacmanInstalled
        if [ "${pacmanInstalled}" = "y" ]; then
            sudo pacman -S xterm
        else
            sudo apt install xterm
        fi
    else
        echo "Please install xterm and run this script again."
    fi
fi
cd ..
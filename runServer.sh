#runs a multiplayer-game-cpp server
sudo nano environmentVars.sh
chmod +x environmentVars.sh
source ./environmentVars.sh
chmod +x saveData.sh
source ./saveData.sh
read -p "Do you have xterm installed? [y/n]: " xtermInstalled
if [ ${xtermInstalled} == "y" ]; then
    xterm -e "./server; /bin/bash" 
else
    read -p "Do you have pacman or apt installed? [y/n]: " packageManager
    if [ ${packageManager} == "y" ]; then
        read -p "Do you have pacman installed? [y/n]: " pacmanInstalled
        if [ ${pacmanInstalled} == "y" ]; then
            sudo pacman -S xterm
            echo "Please run this script again."
        else
            sudo apt install xterm
            echo "Please run this script again."
        fi
    else
        read -p "Do you really want to use the normal terminal? [y/n]: " normalTerminal
        if [ ${normalTerminal} == "y" ]; then
            ./server
        else
        echo "Please install xterm and run this script again."
    fi
fi

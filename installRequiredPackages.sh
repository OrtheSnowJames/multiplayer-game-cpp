#put apt packages here
read -p "enter package manager: " packagemanager
if [packagemanager == "apt"]; then
sudo apt install build-essential git
sudo apt install libboost-all-dev
sudo apt install nlohmann-json3-dev
sudo apt install libraylib-dev
sudo apt install xterm
sudo apt install libx11-dev
elif [packagemanager == "pacman"]; then
sudo pacman -S base-devel git
sudo pacman -S boost
sudo pacman -S nlohmann-json
sudo pacman -S raylib
sudo pacman -S xterm
sudo pacman -S libx11
elif [packagemanager == "yum"]; then
sudo yum groupinstall "Development Tools"
sudo yum install git
sudo yum install boost
sudo yum install nlohmann-json
sudo yum install raylib
sudo yum install xterm
sudo yum install libx11
else
fi
#put git packages here
cd /home/
git clone https://github.com/raysan5/raylib
#put code packages here
code --install-extension ms-vscode.cpptools

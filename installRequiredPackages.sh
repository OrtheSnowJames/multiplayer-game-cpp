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
sudo pacman -S base-devel git boost nlohmann-json raylib xterm libx11 
elif [packagemanager == "yum"]; then
sudo yum groupinstall "Development Tools"
sudo yum install git
sudo yum install boost
sudo yum install nlohmann-json
sudo yum install raylib
sudo yum install xterm
sudo yum install libx11
elif [packagemanager == "dnf"]; then
sudo dnf groupinstall "Development Tools"
sudo dnf install git
sudo dnf install boost
sudo dnf install nlohmann-json
sudo dnf install raylib
sudo dnf install xterm
sudo dnf install libx11
elif [packagemanager == "zypper"]; then
sudo zypper install -t pattern devel_basis
sudo zypper install git
sudo zypper install boost
sudo zypper install nlohmann-json
sudo zypper install raylib
sudo zypper install xterm
sudo zypper install libx11
else
echo not supported yet
fi

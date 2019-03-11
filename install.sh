#!/bin/bash

clear
echo "This script will install and/or modify"
echo "packages needed for the InfluxDB and Grafana-based"
echo "EnviroPi environmental monitoring device. "
echo
echo "Run time 15+ minutes."
echo

if [ "$1" != '-y' ]; then
	echo -n "CONTINUE? [y/N]"
	read
	if [[ ! "$REPLY" =~ ^(yes|y|Y)$ ]]; then
		echo "Canceled."
		exit 0
	fi
fi

echo "Continuing..."

# Run update first
echo "Upgrading packages first..."
sudo apt-get update && sudo apt-get -y upgrade
 
# INFLUX INSTALL 
echo "Installing InfluxDB..."
curl -sL https://repos.influxdata.com/influxdb.key | sudo apt-key add -
echo "deb https://repos.influxdata.com/debian stretch stable" | sudo tee /etc/apt/sources.list.d/influxdb.list
sudo apt-get update && sudo apt-get install influxdb -y 
sudo systemctl start influxdb

read -p "Enter the desired enviropi_user's database password: " mpass

influx -execute 'CREATE DATABASE enviropi'
influx -execute "CREATE USER \"enviropi_user\" WITH PASSWORD '$mpass' WITH ALL PRIVILEGES"

# sudo nano /etc/influxdb/influxdb.conf
# uncomment # auth-enabled = true
# sudo systemctl restart influxdb

### GRAFANA ###

echo "Installing Grafana..."
curl -sL https://packages.grafana.com/gpg.key | sudo apt-key add -
echo "deb https://packages.grafana.com/oss/deb stable main" | sudo tee /etc/apt/sources.list.d/grafana.list
sudo apt-get update && sudo apt-get install grafana -y 
sudo systemctl daemon-reload
sudo systemctl enable grafana-server.service
sudo systemctl start grafana-server
# Install grafana plugin for table in graph 
sudo grafana-cli plugins install yesoreyeram-boomtable-panel
sudo systemctl restart grafana-server
# ANy way to import dashboards and config setup for grafana? i.e. create account etc?


## Python setup ##

echo "Creating Python vitual environment..."
sudo apt-get install python-virtualenv -y
virtualenv --python=/usr/bin/python3 /home/pi/enviropi/env
cd /home/pi/enviropi/
source env/bin/activate
pip install --upgrade pip
pip install -r requirements.txt

# Add db credentials to config file 
echo "[DATABASE]" > pygatt_sensors.ini
echo "db_name = enviropi" >> pygatt_sensors.ini
echo "username = enviropi_user" >> pygatt_sensors.ini
echo "password = $mpass" >> pygatt_sensors.ini
echo "host = localhost" >> pygatt_sensors.ini
echo "port = 8086" >> pygatt_sensors.ini

echo "Configuring the BLE sensor devices to be used..."

# prompt user to enter the device(s) hw address

echo "Please enter each sensor device's information... Press q when finished adding devices."
num=1
while true; do
        read -p "Enter the BLE hardware address for device $num (q to quit) : " a
        if [ $a = "q" ]
        then
                break
        fi
        echo "Adding device $num to the configuration..."
		echo "[DEVICE$num]" >> pygatt_sensors.ini
		echo "address: $a" >> pygatt_sensors.ini
		echo "sensor_id: Si7021" >> pygatt_sensors.ini
		# These are the standard handles for the Adafruit Feather 32u4 Bluefruit LE
		# will need to be modified in pygatt_sensors.ini if a different device is used  
		echo "temp_handle: 00002a6e-0000-1000-8000-00805f9b34fb" >> pygatt_sensors.ini
		echo "humid_handle: 00002a6f-0000-1000-8000-00805f9b34fb" >> pygatt_sensors.ini
		echo "battery_handle: 00002a19-0000-1000-8000-00805f9b34fb" >> pygatt_sensors.ini
		echo "temp_calibrate: 0" >> pygatt_sensors.ini
		echo "humid_calibrate: 0" >> pygatt_sensors.ini
        echo "Finished with device $a !"
		num=$((num + 1))
done

echo "Creating crontab to collect data every 10 minutes..."

(crontab -l 2>/dev/null; echo "SHELL=/bin/bash") | crontab -
(crontab -l 2>/dev/null; echo '*/10 * * * * source /home/pi/enviropi/env/bin/activate && sudo "PATH=$PATH" /home/pi/enviropi/env/bin/python3 /home/pi/enviropi/pygatt_sensors.py') | crontab -

addr=`hostname -I`

# Open browser at boot
echo '@/usr/bin/chromium-browser --incognito --start-maximized --kiosk http://localhost:3000' | sudo tee -a /etc/xdg/lxsession/LXDE-pi/autostart

echo
echo "----------------------------------------"
echo "All finished!"
echo "Grafana dashboard interface viewable at: http://$addr:3000"
echo "Default Grafana login:"
echo "      user:     admin"
echo "      password: admin"
echo "----------------------------------------"
echo 


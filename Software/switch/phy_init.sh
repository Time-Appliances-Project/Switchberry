#!/bin/bash

PHY=0
MDIOBUS=gpio-0



# Write to MMD register
mdio_mmd_write() {
    local dev=$1    # MMD device address (e.g. 2)
    local reg=$2    # MMD register address (e.g. 0x10)
    local val=$3    # Value to write (e.g. 0x0001)

    # Step 1: Set device address
    sudo mdio $MDIOBUS phy $PHY raw 0x0D $((dev & 0x1F))
    # Step 2: Set register address
    sudo mdio $MDIOBUS phy $PHY raw 0x0E $((reg & 0xFFFF))
    # Step 3: Select data register
    sudo mdio $MDIOBUS phy $PHY raw 0x0D $((0x4000 | (dev & 0x1F)))
    # Step 4: Write value
    sudo mdio $MDIOBUS phy $PHY raw 0x0E $((val & 0xFFFF))
}

# Read from MMD register
mdio_mmd_read() {
    local dev=$1    # MMD device address
    local reg=$2    # MMD register address

    # Step 1: Set device address
    sudo mdio $MDIOBUS phy $PHY raw 0x0D $((dev & 0x1F))
    # Step 2: Set register address
    sudo mdio $MDIOBUS phy $PHY raw 0x0E $((reg & 0xFFFF))
    # Step 3: Select data register
    sudo mdio $MDIOBUS phy $PHY raw 0x0D $((0x8000 | (dev & 0x1F)))
    # Step 4: Read value
    sudo mdio $MDIOBUS phy $PHY raw 0x0E
}







# ID Check
id=$(sudo mdio $MDIOBUS phy $PHY raw 2)
if [ "$id" != "0x0022" ]; then
    echo "ERROR: PHY ID is $id, expected 0x0022"
    exit 1
fi
echo "PHY ID check passed ($id)"

# Reset PHY
sudo mdio $MDIOBUS phy $PHY raw 0 0x8000
for ((i=0; i<25; i++)); do
    val=$(sudo mdio $MDIOBUS phy $PHY raw 0)
    if [ $((val & 0x8000)) -eq 0 ]; then break; fi
    echo "Checking reset cleared, waiting"
    sleep 0.2
done

echo "Done after reset phy"

#sleep 10


# enable RGMII 1G only
#mdio_mmd_write 0x2 0x2 0x1000


# Advertise gigabit only
#sudo mdio $MDIOBUS phy $PHY raw 4 0x01
sudo mdio $MDIOBUS phy $PHY raw 9 0x0200

echo "Done after set advertise gigabit"
#sleep 10

# Enable auto-neg, restart auto-neg
sudo mdio $MDIOBUS phy $PHY raw 0 0x1340

echo "Done after enable autoneg and restart autoneg"

#remote loopback mode
#sudo mdio $MDIOBUS phy $PHY raw 0 0x140
#sudo mdio $MDIOBUS phy $PHY raw 0x11 0x100





# Wait for link up, up to 5 seconds
link_up=0
for ((i=0; i<25; i++)); do
    val=$(sudo mdio $MDIOBUS phy $PHY raw 1)
    if [ $((val & 0x0004)) -ne 0 ]; then
        link_up=1
        break
    fi
    sleep 0.2
done

if [[ $link_up -eq 1 ]]; then
    echo "Link up!"
else
    echo "Link did not come up within 5 seconds."
fi


#debug set in loopback


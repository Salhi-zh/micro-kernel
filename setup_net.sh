
#!/bin/bash

# Configuration
TAP_DEV="tap0"
USER_ID=$(whoami)
GATEWAY_IP="10.0.2.1"
NETMASK="255.255.255.0"

# Check if tap0 exists
if ! ip link show $TAP_DEV > /dev/null 2>&1; then
    echo "Configuring network interface $TAP_DEV..."
    
    # Create the TAP interface
    sudo ip tuntap add dev $TAP_DEV mode tap user $USER_ID
    
    # Assign IP address to the host side (the Gateway for the micro-kernel)
    sudo ip addr add $GATEWAY_IP/24 dev $TAP_DEV
    
    # Bring the interface up
    sudo ip link set $TAP_DEV up
    
    # Enable IP Forwarding (so the kernel can reach the real internet)
    sudo sysctl -w net.ipv4.ip_forward=1 > /dev/null
    
    # Set up NAT (Masquerade) - Replace 'eth0' or 'wlan0' with your actual internet interface if needed
    # This automatically finds your default outbound interface
    IFACE=$(ip route | grep default | sed -e 's/^.*dev //; s/ .*$//' | head -n 1)
    sudo iptables -t nat -A POSTROUTING -o $IFACE -j MASQUERADE
    sudo iptables -A FORWARD -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
    sudo iptables -A FORWARD -i $TAP_DEV -o $IFACE -j ACCEPT

    echo "Network $TAP_DEV is ready."
else
    echo "Network $TAP_DEV is already configured."
fi

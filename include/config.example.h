#pragma once

// ==============================================================================
// Configuration Template for ESP32-035 NOC Monitor
// Copy or rename this file to 'config.h' and fill in your network details.
// 'config.h' is gitignored to protect sensitive IP addresses and credentials.
// ==============================================================================

// --- WiFi Credentials (Optional: Leave empty for Offline Demo Mode) ---
#define WIFI_SSID       ""          // e.g., "Your_WiFi_Name"
#define WIFI_PASSWORD   ""          // e.g., "Your_WiFi_Password"
#define WIFI_TIMEOUT_MS 8000        // Connection attempt timeout in ms

// --- NOC Header Branding ---
#define NOC_TITLE       "CAMPUS NOC MONITOR"
#define DOMAIN_NAME     "corp.local"

// --- Infrastructure Node 1 ---
#define NODE_1_NAME     "Proxmox Hypervisor"
#define NODE_1_IP       "192.168.1.10"
#define NODE_1_PORT     8006
#define NODE_1_ROLE     "VM Root Node Cluster"

// --- Infrastructure Node 2 ---
#define NODE_2_NAME     "Primary Domain Controller"
#define NODE_2_IP       "192.168.1.11"
#define NODE_2_PORT     53
#define NODE_2_ROLE     "Active Directory & DNS"

// --- Infrastructure Node 3 ---
#define NODE_3_NAME     "Secondary DC & Backup"
#define NODE_3_IP       "192.168.1.12"
#define NODE_3_PORT     53
#define NODE_3_ROLE     "DNS Secondary & Hyper-V"

// --- Infrastructure Node 4 ---
#define NODE_4_NAME     "Gateway / Proxy"
#define NODE_4_IP       "192.168.1.1"
#define NODE_4_PORT     443
#define NODE_4_ROLE     "Web SSL & Audit Proxy"

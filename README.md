# IoT System

## Overview

The IoT Simulator is a C-based system designed to manage and simulate interactions between sensors, user consoles, and system management processes. As depicted in Figure 1, the system operates through various processes and threads:

- **Sensor:** A process that periodically generates data and sends it to the System Manager via a named pipe. Multiple Sensor processes can run simultaneously.
- **User Console:** A process that interacts with users and communicates with the System Manager through a named pipe to send commands. It handles system management, queries statistics and other information, and receives real-time alerts via a Message Queue. Multiple User Console processes can run.
- **System Manager:** The central process responsible for starting the system, reading the configuration file, and creating Worker and Alerts Watcher processes.
- **Worker:** A process that handles requests from Sensor and User Console processes. Multiple Worker processes can run.
- **Alerts Watcher:** A process that monitors if sensor values exceed predefined limits and sends alerts to User Console processes via the Message Queue (only to those that set the alert).
- **Console Reader:** A thread that reads commands from User Consoles sent through the named pipe CONSOLE_PIPE.
- **Sensor Reader:** A thread that reads data from Sensor processes sent through the named pipe SENSOR_PIPE.
- **Dispatcher:** A thread that takes requests from the INTERNAL_QUEUE and sends them through unnamed pipes to available Workers.

IPC Mechanisms:
- **Named Pipe SENSOR_PIPE:** Allows Sensor processes to send data to the System Manager.
- **Named Pipe CONSOLE_PIPE:** Used for sending commands from User Consoles.
- **Shared Memory (SHM):** Accessed by System Manager, Worker, and Alerts Watcher processes.
- **Unnamed Pipes:** Facilitate communication between the System Manager and each Worker.
- **Message Queue:** Used to send alert messages from the Alerts Watcher to User Consoles and responses to User Console requests.

The System Manager also includes a fixed-size data structure, INTERNAL_QUEUE, to store data or commands to be processed by Workers. A log file records all information for later analysis, and this information should also be displayed on the screen.

## Component and Functionality Description

# Sensor

A process that sends data to the system at regular intervals. Multiple Sensor processes can run simultaneously, each with its parameters. Data is written to the named pipe SENSOR_PIPE.

# User Console

A process providing an interactive menu for users to send management and data queries to the system and display results. It also receives and displays real-time alerts.

# System Manager

Initializes the system and handles the following tasks:

    Reads and validates the configuration file.
    Creates named pipes SENSOR_PIPE and CONSOLE_PIPE.
    Spawns Worker processes.
    Sets up unnamed pipes for each Worker.
    Starts Alerts Watcher process.
    Creates Sensor Reader, Console Reader, and Dispatcher threads.
    Sets up Message Queue and INTERNAL_QUEUE.
    Initializes shared memory for sensor data and alert rules.
    Captures SIGINT to terminate the program.

# Sensor Reader

Thread responsible for handling incoming sensor data and placing it in the INTERNAL_QUEUE for the Dispatcher to process. If the queue is full, the request is discarded and an error message is logged.

# Console Reader

Thread responsible for handling commands received from User Consoles and placing them in the INTERNAL_QUEUE for the Dispatcher to process. The queue is shared with sensor data; if full, the thread waits until space is available.

# Dispatcher

Thread that retrieves entries from the queue and sends them to an available Worker via unnamed pipes. Prioritizes User Console commands over Sensor data. Workers are marked as busy once they receive a task and do not receive new tasks until they are free.

# Worker

Processes that receive messages through unnamed pipes, which can be either sensor data or console commands. Updates shared memory with sensor data and processes commands. Handles both read and write operations, updating shared memory or sending responses as needed.

# Alerts Watcher

Monitors sensor values for out-of-bounds conditions and sends alerts to User Consoles via the Message Queue. Notifications are logged and displayed on the console.

# Controlled Shutdown

Upon receiving SIGINT from the System Manager, the system follows these steps:

- Logs the initiation of the shutdown.
- Stops the Dispatcher, Sensor Reader, and Console Reader threads.
- Waits for all tasks in Workers to complete.
- Logs pending tasks in the INTERNAL_QUEUE.
- Cleans up all resources except for Sensor and User Console processes.


"""Platform starter script

A starter script that parses the config file and sets up the environment
before running the platform executable.
"""

import json
import subprocess
import os
from typing import Any, Dict, List

PLATFORM_CONFIG_PATH = "/opt/platform_config.json"
EXECUTABLE_PATH = "/usr/bin/menabrea"

def main() -> None:
    """Script entry point."""
    platform_config = parse_platform_config_file(PLATFORM_CONFIG_PATH)
    set_up_environment(platform_config)
    command_line = format_command_line(platform_config)
    spawn_platform_subprocess(command_line)

def parse_platform_config_file(pathname: str) -> Dict[str, Any]:
    """Parse platform configuration file."""
    with open(pathname) as fstream:
        return json.load(fstream)

def set_up_environment(config: Dict[str, Any]) -> None:
    """Set up the environment based on the parsed config."""
    environment = config["environment"]
    for key, value in environment.items():
        if value:
            os.environ[key] = str(value)

def format_command_line(config: Dict[str, Any]) -> List[str]:
    """Prepare command-line arguments based on the parsed config."""
    command_line = []

    node_id = config["node_id"]
    command_line.append("--nodeId")
    command_line.append(f"{node_id}")

    network_if = config["network_if"]
    command_line.append("--netIf")
    command_line.append(f"{network_if}")

    # Configure the default event pool used by EM
    command_line.append("--defaultPoolConfig")
    command_line.append(serialize_pool_config(config["pools"]["default"]))

    # Configure the message pool
    command_line.append("--messagePoolConfig")
    command_line.append(serialize_pool_config(config["pools"]["message"]))

    # Configure the memory pool for application allocations
    command_line.append("--memoryPoolConfig")
    command_line.append(serialize_pool_config(config["pools"]["memory"]))

    pktio_bufs = config["pktio_bufs_kilo"] * 1024
    command_line.append("--pktioBufs")
    command_line.append(f"{pktio_bufs}")

    return command_line

def serialize_pool_config(pool_config: Dict[str, int]) -> str:
    """Parse event pool config and format it for use as command-line argument."""
    num_subpools = pool_config["num_subpools"]
    serialized = f"{num_subpools}"
    for subpool_index in range(num_subpools):
        subpool_config = pool_config["subpool"][subpool_index]
        # Parse the size of the events...
        size = subpool_config["size"]
        # ...number of events in the pool...
        num = subpool_config["num"]
        # ...and size of the event cache
        cache_size = subpool_config["cache_size"]
        serialized += f",{size}:{num}:{cache_size}"
    return serialized

def spawn_platform_subprocess(command_line: List[str]) -> None:
    """Spawn the platform process."""
    command = [EXECUTABLE_PATH] + command_line
    subprocess.Popen(command)

if __name__ == "__main__":
    main()

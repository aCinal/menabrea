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
    """Parse platform configuration file (YAML)."""
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

    global_worker_id = config["workers"]["own_node_id"]
    command_line.append("--globalWorkerId")
    command_line.append(f"{global_worker_id}")

    command_line.append("--messagingPoolConfig")
    command_line.append(serialize_event_pool_config(config["event_pools"]["default"]))

    command_line.append("--defaultPoolConfig")
    command_line.append(serialize_event_pool_config(config["event_pools"]["messaging"]))

    return command_line

def serialize_event_pool_config(pool_config: Dict[str, int]) -> str:
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

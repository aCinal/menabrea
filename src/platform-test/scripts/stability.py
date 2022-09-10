"""Stability test runner."""

import socket
import json
from typing import Any, Dict, List

UDS_PATH = "/tmp/test_framework.sock"
REGRESSION_CONFIG_PATH = "/opt/regression.json"

def main() -> None:
    """Application entry point."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    sock.connect(UDS_PATH)
    regression = parse_regression_json(REGRESSION_CONFIG_PATH)

    # Run the regression suite forever
    while True:
        for case in regression["cases"]:
            command = make_run_command(case)
            sock.send(bytes(command, "utf-8"))
            result = str(sock.recv(4096), "utf-8")
            if "PASSED" not in result:
                print("### Stability test failed! ###")
                print(f"Command: {command}")
                print(f"Result: {result}")
                return

def parse_regression_json(path: str) -> Dict[str, List[Dict[str, Any]]]:
    """Parse the regression list from a JSON file."""
    with open(path) as fstream:
        return json.load(fstream)

def make_run_command(case: Dict[str, Any]) -> str:
    """Construct a test framework run command based on the regression JSON entry."""
    command = "run " + case["name"]

    for key, value in case["params"].items():
        if type(value) == bool:
            # Convert 'True' to 'true' and 'False' to 'false
            value = str(value).lower()
        command += f" {key}={value}"

    return command

if __name__ == "__main__":
    main()


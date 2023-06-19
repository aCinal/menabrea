"""Stability test runner."""

import socket
import json
from typing import Any, Dict, List

UDS_PATH = "/tmp/test_framework.sock"
TEST_SUITE_PATH = "/opt/test_suite.json"

def main() -> None:
    """Application entry point."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    sock.connect(UDS_PATH)
    suite = parse_test_suite_json(TEST_SUITE_PATH)

    # Run the test suite forever
    while True:
        for case in suite["cases"]:
            command = make_run_command(case)
            sock.send(bytes(command, "utf-8"))
            result = str(sock.recv(4096), "utf-8")
            if "PASSED" not in result:
                print("### Stability test failed! ###")
                print(f"Command: {command}")
                print(f"Result: {result}")
                return

def parse_test_suite_json(path: str) -> Dict[str, List[Dict[str, Any]]]:
    """Load the test suite from a JSON file."""
    with open(path) as fstream:
        return json.load(fstream)

def make_run_command(case: Dict[str, Any]) -> str:
    """Construct a test framework run command based on the test suite JSON entry."""
    command = "run " + case["name"]

    for key, value in case["params"].items():
        if type(value) == bool:
            # Convert 'True' to 'true' and 'False' to 'false
            value = str(value).lower()
        command += f" {key}={value}"

    return command

if __name__ == "__main__":
    main()


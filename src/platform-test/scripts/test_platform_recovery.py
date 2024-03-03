"""Recovery test runner."""

import argparse
import logging
import subprocess
import time
from typing import List, Optional, Tuple

RECOVERY_TIMEOUT = 30
VICTIM_PROCESS_NAME = "disp_0"

def main() -> None:
    """Application entry point."""
    # Parse command line arguments
    opts = parse_args()
    log = init_logger(opts.verbose)

    log.debug("Checking if the platform is already running...")
    # Check if platform already running - if so, break the test
    if not is_platform_up():
        log.warning("Platform not running. Breaking test execution...")
        return

    for _ in range(opts.number):
        # Find one of the dispatcher's PID
        pid = find_pid(VICTIM_PROCESS_NAME)
        assert pid, "Dispatcher process not found"
        log.debug(f"Killing the platform (process '{VICTIM_PROCESS_NAME}' at PID: {pid})...")
        # Send SIGABRT to the platform
        run_command(f"kill -s SIGABRT {pid}")

        log.debug("Waiting for platform recovery")
        # Wait for recovery
        wait_until_platform_recovery(RECOVERY_TIMEOUT)
        log.debug("Platform has recovered successfully")

    log.info("Success")

def run_command(command: str) -> Tuple[int, List[str]]:
    """Run a shell command."""
    completed = subprocess.run(
        command,
        shell=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT
    )
    # Return the return code and tokenized stdout
    return completed.returncode, completed.stdout.splitlines()

def find_pid(process_name: str) -> Optional[int]:
    """Find a pid of process by name."""
    status, ps_lines = run_command("ps")
    assert status == 0, f"Failed to list running processes: {ps_lines}"
    # Filter the ps output to find a dispatcher process
    matching_lines = list(filter(lambda line: process_name in line, ps_lines))
    assert len(matching_lines) <= 1, f"Multiple ps lines matched the filter: {matching_lines}"
    if len(matching_lines) == 0:
        return None
    else:
        # Extract the process ID from the matching line
        pid = int(matching_lines[0].split()[0])
        return pid

def wait_until_platform_recovery(timeout: int) -> None:
    """Wait until platform has recovered from a crash."""
    # Sleep in one second increments
    for _ in range(timeout):
        if is_platform_up():
            return
        time.sleep(1)
    # Do one final check
    if not is_platform_up():
        raise Exception(f"Timed out while waiting for the platform to recover. Service status: {status}")

def is_platform_up() -> bool:
    """Check if the platform is up and running."""
    status, _ = run_command("systemctl status menabrea")
    if status:
        # Non-zero status means the platform is definitely not up
        return False
    else:
        # Check if all dispatchers have their names already
        # TODO: Find a cleaner way to check startup progress (note that platform makes no assumption about the number of cores)
        return all([ find_pid("disp_0"), find_pid("disp_1"), find_pid("disp_2"), find_pid("disp_3") ])

def parse_args() -> argparse.Namespace:
    """Parse the command line arguments."""
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-n",
        "--number",
        type=int,
        default=1,
        help="Number of crash-recovery cycles to run"
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Print debug logs"
    )
    return parser.parse_args()

def init_logger(verbose: bool) -> logging.Logger:
    """Initialize the logger."""
    logging.basicConfig(
        format = "[%(levelname)s] %(asctime)s (%(threadName)s): %(message)s",
        level = logging.DEBUG if verbose else logging.INFO
    )
    return logging.getLogger()

if __name__ == "__main__":
    main()
